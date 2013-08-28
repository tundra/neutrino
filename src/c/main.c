// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "crash.h"
#include "interp.h"
#include "plankton.h"
#include "runtime.h"
#include "value-inl.h"

#include <unistd.h>

static value_t read_file_to_blob(runtime_t *runtime, FILE *file) {
  // Read the complete file into a byte buffer.
  byte_buffer_t buffer;
  byte_buffer_init(&buffer, NULL);
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

// Executes the given program (syntax tree) within the given runtime.
static value_t execute_program(runtime_t *runtime, value_t program) {
  TRY_DEF(space, new_heap_method_space(runtime));
  TRY(add_method_space_builtin_methods(runtime, space));
  TRY_DEF(code_block, compile_syntax(runtime, program, space, runtime_null(runtime)));
  return run_code_block(runtime, code_block);
}

// Create a vm and run the program.
static value_t neutrino_main(int argc, char *argv[]) {
  runtime_t *runtime;
  TRY(new_runtime(NULL, &runtime));
  E_BEGIN_TRY_FINALLY();
    for (int i = 1; i < argc; i++) {
      const char *filename = argv[i];
      value_t input;
      if (strcmp("-", filename) == 0) {
        E_TRY_SET(input, read_file_to_blob(runtime, stdin));
      } else {
        FILE *file = fopen(filename, "r");
        E_TRY_SET(input, read_file_to_blob(runtime, file));
        fclose(file);
      }
      value_mapping_t syntax_mapping;
      E_TRY(init_syntax_mapping(&syntax_mapping, runtime));
      E_TRY_DEF(program, plankton_deserialize(runtime, &syntax_mapping, input));
      E_TRY_DEF(result, execute_program(runtime, program));
      value_print_ln(result);
    }
    E_RETURN(success());
  E_FINALLY();
    TRY(delete_runtime(runtime));
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
