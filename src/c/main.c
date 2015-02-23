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
static bool parse_options(size_t argc, const char **argv, main_options_t *flags_out) {
  pton_command_line_reader_t *reader = pton_new_command_line_reader();
  flags_out->owner = reader;
  pton_command_line_t *cmdline = pton_command_line_reader_parse(reader, argc, argv);
  flags_out->cmdline = cmdline;
  if (!pton_command_line_is_valid(cmdline)) {
    pton_syntax_error_t *error = pton_command_line_error(cmdline);
    ERROR("Error parsing command line options at char '%c'",
        pton_syntax_error_offender(error));
    return false;
  }

  flags_out->config->gc_fuzz_freq = pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-frequency"),
          pton_integer(0)));
  flags_out->config->gc_fuzz_seed = pton_int64_value(
      pton_command_line_option(cmdline,
          pton_c_str("garbage-collect-fuzz-seed"),
          pton_integer(0)));
  return true;
}

// Reads a library from the given library path and adds the modules to the
// runtime's module loader.
static value_t load_library_from_file(runtime_t *runtime, value_t self,
    pton_variant_t library_path) {
  // Read the library from the file.
  utf8_t library_path_str = new_c_string(pton_string_chars(library_path));
  TRY_DEF(library_path_val, new_heap_utf8(runtime, library_path_str));
  file_streams_t streams = file_system_open(runtime->file_system,
      library_path_str, OPEN_FILE_MODE_READ);
  if (!streams.is_open)
    return new_system_error_condition(seFileNotFound);
  E_BEGIN_TRY_FINALLY();
    E_RETURN(runtime_load_library_from_stream(runtime, streams.in, library_path_val));
  E_FINALLY();
    file_streams_close(&streams);
  E_END_TRY_FINALLY();
}

// Constructs a module loader based on the given command-line options.
static value_t build_module_loader(runtime_t *runtime, pton_command_line_t *cmdline) {
  pton_variant_t options = pton_command_line_option(cmdline,
      pton_c_str("module_loader"), pton_null());
  if (pton_is_null(options))
    return success();
  value_t loader = deref(runtime->module_loader);
  pton_variant_t libraries = pton_map_get(options, pton_c_str("libraries"));
  for (size_t i = 0; i < pton_array_length(libraries); i++) {
    pton_variant_t library_path = pton_array_get(libraries, i);
    TRY(load_library_from_file(runtime, loader, library_path));
  }
  return success();
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
  config->plugins = (const void**) get_main_plugins();
}

// Create a vm and run the program.
static value_t neutrino_main(int argc, const char **argv) {
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  runtime_config_init_main_defaults(&config);
  // Set up a custom allocator we get tighter control over allocation.
  limited_allocator_t limited_allocator;
  limited_allocator_install(&limited_allocator, config.system_memory_limit);

  // Parse the options.
  main_options_t options;
  main_options_init(&options, &config);
  if (!parse_options(argc - 1, argv + 1, &options))
    return new_invalid_input_condition();

  runtime_t *runtime;
  TRY(new_runtime(&config, &runtime));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    value_t result = whatever();
    E_TRY(build_module_loader(runtime, options.cmdline));
    size_t argc = pton_command_line_argument_count(options.cmdline);
    for (size_t i = 0; i < argc; i++) {
      pton_variant_t filename_var = pton_command_line_argument(options.cmdline, i);
      utf8_t filename = new_string(pton_string_chars(filename_var),
          pton_string_length(filename_var));
      value_t input;
      if (string_equals_cstr(filename, "-")) {
        in_stream_t *in = file_system_stdin(runtime->file_system);
        E_TRY_SET(input, read_stream_to_blob(runtime, in));
      } else {
        file_streams_t streams = file_system_open(runtime->file_system, filename,
            OPEN_FILE_MODE_READ);
        if (!streams.is_open)
          return new_system_call_failed_condition("fopen");
        E_TRY_SET(input, read_stream_to_blob(runtime, streams.in));
        file_streams_close(&streams);
      }
      E_TRY_DEF(program, safe_runtime_plankton_deserialize(runtime, protect(pool, input)));
      result = safe_runtime_execute_syntax(runtime, protect(pool, program));
    }
    E_RETURN(result);
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
    TRY(delete_runtime(runtime));
    main_options_dispose(&options);
    limited_allocator_uninstall(&limited_allocator);
  E_END_TRY_FINALLY();
}

int main(int argc, const char *argv[]) {
  install_crash_handler();
  value_t result = neutrino_main(argc, argv);
  if (is_condition(result)) {
    out_stream_t *out = file_system_stderr(file_system_native());
    print_ln(out, "Error: %v", result);
    return 1;
  } else {
    return 0;
  }
}
