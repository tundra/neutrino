// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "file.h"

value_t read_handle_to_blob(runtime_t *runtime, FILE *handle) {
  // Read the complete file into a byte buffer.
  byte_buffer_t buffer;
  byte_buffer_init(&buffer);
  while (true) {
    static const size_t kBufSize = 1024;
    byte_t raw_buffer[kBufSize];
    size_t was_read = fread(raw_buffer, 1, kBufSize, handle);
    if (was_read <= 0)
      break;
    for (size_t i = 0; i < was_read; i++)
      byte_buffer_append(&buffer, raw_buffer[i]);
  }
  blob_t data_blob;
  byte_buffer_flush(&buffer, &data_blob);
  // Create a blob to hold the result and copy the data into it.
  value_t result = new_heap_blob_with_data(runtime, &data_blob);
  byte_buffer_dispose(&buffer);
  return result;
}

value_t read_file_to_blob(runtime_t *runtime, string_t *filename) {
  FILE *handle = fopen(filename->chars, "r");
  if (handle == NULL)
    return new_system_error_condition(seFileNotFound);
  E_BEGIN_TRY_FINALLY();
    E_RETURN(read_handle_to_blob(runtime, handle));
  E_FINALLY();
    fclose(handle);
  E_END_TRY_FINALLY();
}
