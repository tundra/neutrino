//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "interp.h"
#include "plankton.h"
#include "runtime-inl.h"
#include "safe-inl.h"
#include "serialize.h"
#include "tagged.h"
#include "try-inl.h"
#include "utils/crash.h"
#include "utils/log.h"
#include "value.h"

// Convert a C string containing base64 encoded data to a heap blob with the
// raw bytes represented by the string.
static value_t base64_c_str_to_blob(runtime_t *runtime, const char *data) {
  if ((data == NULL) || (strstr(data, "p64/") != data)) {
    return null();
  } else {
    byte_buffer_t buf;
    byte_buffer_init(&buf);
    base64_decode(new_c_string(data + 4), &buf);
    blob_t blob = byte_buffer_flush(&buf);
    value_t result = new_heap_blob_with_data(runtime, blob);
    byte_buffer_dispose(&buf);
    return result;
  }
}

// Assemble a module that can be executed from unbound modules in the module
// loader.
static value_t assemble_module(value_t ambience, value_t unbound_module) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  runtime_t *runtime = get_ambience_runtime(ambience);
  TRY_DEF(module, build_bound_module(ambience, unbound_module));
  value_t methodspace = get_ambience_methodspace(ambience);
  TRY(ensure_frozen(runtime, methodspace));
  return get_module_fragment_at(module, present_stage());
}

// Executes the given program syntax tree within the given runtime.
static value_t safe_execute_syntax(runtime_t *runtime, safe_value_t s_program) {
  CHECK_FAMILY(ofProgramAst, deref(s_program));
  TRY_DEF(ambience, new_heap_ambience(runtime));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  safe_value_t s_ambience = protect(pool, ambience);
  // Forward declare these to avoid msvc complaining.
  safe_value_t s_module, s_entry_point;
  E_BEGIN_TRY_FINALLY();
    value_t unbound_module = get_program_ast_module(deref(s_program));
    E_TRY_DEF(module, assemble_module(deref(s_ambience), unbound_module));
    s_module = protect(pool, module);
    s_entry_point = protect(pool, get_program_ast_entry_point(deref(s_program)));
    E_TRY_DEF(code_block, safe_compile_expression(runtime, s_entry_point,
        s_module, scope_get_bottom()));
    E_RETURN(run_code_block(s_ambience, protect(pool, code_block)));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

// Data used by the custom allocator.
typedef struct {
  // The default allocator this one is replacing.
  allocator_t *outer;
  // The config to use.
  runtime_config_t *config;
  // The total amount of live memory.
  size_t live_memory;
  // The custom allocator to use.
  allocator_t allocator;
} main_allocator_data_t;

void main_free(void *raw_data, memory_block_t memory) {
  main_allocator_data_t *data = (main_allocator_data_t*) raw_data;
  data->live_memory -= memory.size;
  allocator_free(data->outer, memory);
}

memory_block_t main_malloc(void *raw_data, size_t size) {
  main_allocator_data_t *data = (main_allocator_data_t*) raw_data;
  if (data->live_memory + size > data->config->system_memory_limit) {
    WARN("Tried to allocate more than %i of system memory. At %i, requested %i.",
        data->config->system_memory_limit, data->live_memory, size);
    return memory_block_empty();
  }
  memory_block_t result = allocator_malloc(data->outer, size);
  if (!memory_block_is_empty(result))
    data->live_memory += result.size;
  return result;
}

static void main_allocator_data_init(main_allocator_data_t *data, runtime_config_t *config) {
  data->config = config;
  data->live_memory = 0;
  data->allocator.data = data;
  data->allocator.free = main_free;
  data->allocator.malloc = main_malloc;
  data->outer = allocator_set_default(&data->allocator);
}

static void main_allocator_data_dispose(main_allocator_data_t *data) {
  allocator_set_default(data->outer);
  if (data->live_memory > 0)
    WARN("Disposing with %ib of live memory.", data->live_memory);
}

// Holds all the options understood by the main executable.
typedef struct {
  // The config to store config-related flags directly into.
  runtime_config_t *config;
  // The reader that owns the parsed data.
  pton_command_line_reader_t *owner;
  // Parsed command-line.
  pton_command_line_t *cmdline;
} main_options_t;

// Initializes a options struct.
static void main_options_init(main_options_t *flags, runtime_config_t *config) {
  flags->config = config;
  flags->owner = NULL;
  flags->cmdline = NULL;
}

// Free any memory allocated for the options struct.
static void main_options_dispose(main_options_t *flags) {
  pton_dispose_command_line_reader(flags->owner);
  flags->owner = NULL;
  flags->cmdline = NULL;
}

// Parse a set of command-line arguments.
static void parse_options(size_t argc, const char **argv, main_options_t *flags_out) {
  pton_command_line_reader_t *reader = pton_new_command_line_reader();
  flags_out->owner = reader;
  pton_command_line_t *cmdline = pton_command_line_reader_parse(reader, argc, argv);
  flags_out->cmdline = cmdline;
  if (!pton_command_line_is_valid(cmdline)) {
    pton_syntax_error_t *error = pton_command_line_error(cmdline);
    ERROR("Error parsing command line options at char '%c'",
        pton_syntax_error_offender(error));
    return;
  }

  flags_out->config->gc_fuzz_freq = pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-frequency"),
          pton_integer(0)));
  flags_out->config->gc_fuzz_seed = pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-seed"),
          pton_integer(0)));
}

// Constructs a module loader based on the given command-line options.
static value_t build_module_loader(runtime_t *runtime, pton_command_line_t *cmdline) {
  value_t loader = deref(runtime->module_loader);
  pton_variant_t module_loader_options = pton_command_line_option(cmdline,
      pton_c_str("module_loader"), pton_null());
  if (!pton_is_null(module_loader_options))
    TRY(module_loader_process_options(runtime, loader, module_loader_options));
  return loader;
}

static value_t test_echo(builtin_arguments_t *args) {
  return get_builtin_argument(args, 0);
}

#define kTestMethodCount 1
static const c_object_method_t kTestMethods[kTestMethodCount] = {
  BUILTIN_METHOD("echo", 1, test_echo)
};

static const c_object_info_t **get_main_plugins() {
  static c_object_info_t test_plugin;
  c_object_info_reset(&test_plugin);
  c_object_info_set_methods(&test_plugin, kTestMethods, kTestMethodCount);
  static const c_object_info_t *result[1] = { &test_plugin };
  return result;
}

// Override some of the basic defaults to make the config better suited for
// running scripts.
static void runtime_config_init_main_defaults(runtime_config_t *config) {
  // Currently the runtime doesn't handle allocation failures super well
  // (particularly plankton parsing) so keep the semispace size big.
  config->semispace_size_bytes = 10 * kMB;
  config->plugin_count = 1;
  config->plugins = get_main_plugins();
}

// Create a vm and run the program.
static value_t neutrino_main(int argc, const char **argv) {
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  runtime_config_init_main_defaults(&config);
  // Set up a custom allocator we get tighter control over allocation.
  main_allocator_data_t allocator_data;
  main_allocator_data_init(&allocator_data, &config);

  // Parse the options.
  main_options_t options;
  main_options_init(&options, &config);
  parse_options(argc, argv, &options);

  runtime_t *runtime;
  TRY(new_runtime(&config, &runtime));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    value_t result = whatever();
    E_TRY(build_module_loader(runtime, options.cmdline));
    size_t argc = pton_command_line_argument_count(options.cmdline);
    for (size_t i = 1; i < argc; i++) {
      const char *filename = pton_string_chars(pton_command_line_argument(options.cmdline, i));
      value_t input;
      if (strcmp("-", filename) == 0) {
        io_stream_t *stdin_handle = file_system_stdin(runtime->file_system);
        E_TRY_SET(input, read_handle_to_blob(runtime, stdin_handle));
      } else {
        utf8_t filename_str = new_c_string(filename);
        E_TRY_SET(input, read_file_to_blob(runtime, filename_str));
      }
      E_TRY_DEF(program, safe_runtime_plankton_deserialize(runtime, protect(pool, input)));
      result = safe_execute_syntax(runtime, protect(pool, program));
    }
    E_RETURN(result);
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
    TRY(delete_runtime(runtime));
    main_options_dispose(&options);
    main_allocator_data_dispose(&allocator_data);
  E_END_TRY_FINALLY();
}

int main(int argc, const char *argv[]) {
  install_crash_handler();
  value_t result = neutrino_main(argc, argv);
  if (is_condition(result)) {
    print_ln("Error: %v", result);
    return 1;
  } else {
    return 0;
  }
}
