// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "crash.h"
#include "interp.h"
#include "log.h"
#include "plankton.h"
#include "runtime-inl.h"
#include "safe-inl.h"
#include "try-inl.h"
#include "value.h"

#include <unistd.h>

static value_t read_file_to_blob(runtime_t *runtime, FILE *file) {
  // Read the complete file into a byte buffer.
  byte_buffer_t buffer;
  byte_buffer_init(&buffer);
  while (true) {
    static const size_t kBufSize = 1024;
    byte_t raw_buffer[kBufSize];
    ssize_t was_read = fread(raw_buffer, 1, kBufSize, file);
    if (was_read <= 0)
      break;
    for (ssize_t i = 0; i < was_read; i++)
      byte_buffer_append(&buffer, raw_buffer[i]);
  }
  blob_t data_blob;
  byte_buffer_flush(&buffer, &data_blob);
  // Create a blob to hold the result and copy the data into it.
  value_t result = new_heap_blob_with_data(runtime, &data_blob);
  byte_buffer_dispose(&buffer);
  return result;
}

// Executes the given program syntax tree within the given runtime.
static value_t safe_execute_syntax(runtime_t *runtime, safe_value_t s_program) {
  CHECK_FAMILY(ofProgramAst, deref(s_program));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    safe_value_t s_entry_point = protect(pool, get_program_ast_entry_point(deref(s_program)));
    E_TRY_DEF(code_block, safe_compile_expression(runtime, s_entry_point,
        scope_lookup_callback_get_bottom()));
    E_RETURN(run_code_block(runtime, protect(pool, code_block)));
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
  main_allocator_data_t *data = raw_data;
  data->live_memory -= memory.size;
  allocator_free(data->outer, memory);
}

memory_block_t main_malloc(void *raw_data, size_t size) {
  main_allocator_data_t *data = raw_data;
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
// TODO: accept options encoded as plankton instead of this.
typedef struct {
  // Whether or not to print the output values.
  bool print_value;
  // The config to store config-related flags directly into.
  runtime_config_t *config;
  // The number of positional arguments.
  size_t argc;
  // The values of the positional arguments.
  const char **argv;
  // The memory block that holds the args.
  memory_block_t argv_memory;
} main_options_t;

// Initializes a options struct.
static void main_options_init(main_options_t *flags, runtime_config_t *config) {
  flags->print_value = false;
  flags->config = config;
  flags->argc = 0;
  flags->argv = NULL;
  flags->argv_memory = memory_block_empty();
}

// Free any memory allocated for the options struct.
static void main_options_dispose(main_options_t *flags) {
  allocator_default_free(flags->argv_memory);
  flags->argv = NULL;
}

// Returns true iff the given haystack starts with the given prefix.
static bool c_str_starts_with(const char *haystack, const char *prefix) {
  return strstr(haystack, prefix) == haystack;
}

// Returns true iff the two given c strings are equal.
static bool c_str_equals(const char *a, const char *b) {
  return strcmp(a, b) == 0;
}

// Parses the given string as a base-10 long and returns the value. If parsing
// fails issues an error and aborts execution.
static size_t c_str_as_long_or_die(const char *str) {
  char *end = NULL;
  size_t value = strtol(str, &end, 10);
  if (*end != '\0') {
    ERROR("Couldn't parse '%s' as a number", str);
    UNREACHABLE("c str as long parse error");
  }
  return value;
}

// Parse a set of command-line arguments.
static void parse_options(size_t argc, char **argv, main_options_t *flags_out) {
  // Allocate an argv array that is definitely large enough to store all the
  // positional arguments.
  flags_out->argv_memory = allocator_default_malloc(argc * sizeof(char*));
  flags_out->argv = flags_out->argv_memory.memory;
  // Scan through the arguments and parse them as flags or arguments.
  for (size_t i = 1; i < argc;) {
    const char *arg = argv[i++];
    if (c_str_starts_with(arg, "--")) {
      if (c_str_equals(arg, "--print-value")) {
        flags_out->print_value = true;
      } else if (c_str_equals(arg, "--garbage-collect-fuzz-frequency")) {
        CHECK_REL("missing flag argument", i, <, argc);
        flags_out->config->gc_fuzz_freq = c_str_as_long_or_die(argv[i++]);
      } else if (c_str_equals(arg, "--garbage-collect-fuzz-seed")) {
        CHECK_REL("missing flag argument", i, <, argc);
        flags_out->config->gc_fuzz_seed = c_str_as_long_or_die(argv[i++]);
      } else {
        ERROR("Unknown flags '%s'", arg);
        UNREACHABLE("Flag parsing failed");
      }
    } else {
      flags_out->argv[flags_out->argc++] = arg;
    }
  }
}

// Create a vm and run the program.
static value_t neutrino_main(int argc, char **argv) {
  runtime_config_t config;
  runtime_config_init_defaults(&config);
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
    for (size_t i = 0; i < options.argc; i++) {
      const char *filename = options.argv[i];
      value_t input;
      if (strcmp("-", filename) == 0) {
        E_TRY_SET(input, read_file_to_blob(runtime, stdin));
      } else {
        FILE *file = fopen(filename, "r");
        E_TRY_SET(input, read_file_to_blob(runtime, file));
        fclose(file);
      }
      value_mapping_t syntax_mapping;
      E_TRY(init_plankton_environment_mapping(&syntax_mapping, runtime));
      E_TRY_DEF(program, safe_plankton_deserialize(runtime, &syntax_mapping,
          protect(pool, input)));
      value_t result = safe_execute_syntax(runtime, protect(pool, program));
      if (options.print_value)
        value_print_ln(result);
    }
    E_RETURN(success());
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
    TRY(delete_runtime(runtime));
    main_options_dispose(&options);
    main_allocator_data_dispose(&allocator_data);
  E_END_TRY_FINALLY();
}

int main(int argc, char *argv[]) {
  install_crash_handler();
  value_t result = neutrino_main(argc, argv);
  if (get_value_domain(result) == vdSignal) {
    value_print_ln(result);
    return 1;
  } else {
    return 0;
  }
}
