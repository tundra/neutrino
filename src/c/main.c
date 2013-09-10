// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "crash.h"
#include "interp.h"
#include "log.h"
#include "plankton.h"
#include "runtime.h"
#include "value-inl.h"

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
static value_t execute_syntax(runtime_t *runtime, value_t program) {
  CHECK_FAMILY(ofProgramAst, program);
  TRY_DEF(space, new_heap_method_space(runtime));
  TRY(add_method_space_builtin_methods(runtime, space));
  value_t entry_point = get_program_ast_entry_point(program);
  TRY_DEF(code_block, compile_expression(runtime, entry_point, space, NULL));
  return run_code_block(runtime, code_block);
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
    WARN("Disposing with %i of live memory.", data->live_memory);
}

// Whether or not to print the output values.
// TODO: accept flags encoded as plankton instead of this.
static bool print_value = false;

// Create a vm and run the program.
static value_t neutrino_main(int argc, char *argv[]) {
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  // Set up a custom allocator we get tighter control over allocation.
  main_allocator_data_t allocator_data;
  main_allocator_data_init(&allocator_data, &config);
  runtime_t *runtime;
  TRY(new_runtime(&config, &runtime));
  E_BEGIN_TRY_FINALLY();
    for (int i = 1; i < argc; i++) {
      const char *filename = argv[i];
      value_t input;
      if (strcmp("-", filename) == 0) {
        E_TRY_SET(input, read_file_to_blob(runtime, stdin));
      } else if (strcmp("--print-value", filename) == 0) {
        print_value = true;
        continue;
      } else {
        FILE *file = fopen(filename, "r");
        E_TRY_SET(input, read_file_to_blob(runtime, file));
        fclose(file);
      }
      value_mapping_t syntax_mapping;
      E_TRY(init_syntax_mapping(&syntax_mapping, runtime));
      E_TRY_DEF(program, plankton_deserialize(runtime, &syntax_mapping, input));
      value_t result = execute_syntax(runtime, program);
      if (print_value)
        value_print_ln(result);
    }
    E_RETURN(success());
  E_FINALLY();
    TRY(delete_runtime(runtime));
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
