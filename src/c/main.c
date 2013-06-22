#include "alloc.h"
#include "plankton.h"
#include "runtime.h"
#include "value-inl.h"

#include <unistd.h>
#include <string.h>

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
  TRY_DEF(result, new_heap_blob(runtime, blob_length(&data_blob)));
  blob_t result_blob;
  get_blob_data(result, &result_blob);
  blob_copy_to(&data_blob, &result_blob);
  byte_buffer_dispose(&buffer);
  return result;
}

// Create a vm and run the program.
static value_t neutrino_main(int argc, char *argv[]) {
  runtime_t runtime;
  TRY(runtime_init(&runtime, NULL));

  for (int i = 1; i < argc; i++) {
    const char *filename = argv[i];
    value_t input;
    if (strcmp("-", filename) == 0) {
      TRY_SET(input, read_file_to_blob(&runtime, stdin));
    } else {
      FILE *file = fopen(filename, "r");
      TRY_SET(input, read_file_to_blob(&runtime, file));
      fclose(file);
    }
    TRY_DEF(value, plankton_deserialize(&runtime, NULL, input));
    value_print_ln(value);
  }

  TRY(runtime_dispose(&runtime));
  return success();
}

int main(int argc, char *argv[]) {
  value_t result = neutrino_main(argc, argv);
  if (get_value_domain(result) == vdSignal) {
    value_print_ln(result);
    return 1;
  } else {
    return 0;
  }
}
