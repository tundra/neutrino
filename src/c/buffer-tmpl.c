// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// This is a make-your-own template; define the appropriate macros and include
// this file and it will expand to the implementation of a buffer type that
// works with that type.

#ifndef BUFFER_TYPE
#error No buffer type defined
#endif

#ifndef MAKE_BUFFER_NAME
#error No buffer name maker defined
#endif

void MAKE_BUFFER_NAME(init)(MAKE_BUFFER_NAME(t) *buf) {
  buf->length = 0;
  buf->memory = allocator_default_malloc(128 * sizeof(BUFFER_TYPE));
}

void MAKE_BUFFER_NAME(dispose)(MAKE_BUFFER_NAME(t) *buf) {
  allocator_default_free(buf->memory);
}

// Expands the buffer to make room for 'length' elements if necessary.
static void MAKE_BUFFER_NAME(ensure_capacity)(MAKE_BUFFER_NAME(t) *buf, size_t length) {
  size_t length_bytes = length * sizeof(BUFFER_TYPE);
  if (length_bytes < buf->memory.size)
    return;
  size_t new_capacity = (length * 2);
  memory_block_t new_memory = allocator_default_malloc(new_capacity * sizeof(BUFFER_TYPE));
  memcpy(new_memory.memory, buf->memory.memory, buf->length * sizeof(BUFFER_TYPE));
  allocator_default_free(buf->memory);
  buf->memory = new_memory;
}

void MAKE_BUFFER_NAME(append)(MAKE_BUFFER_NAME(t) *buf, BUFFER_TYPE value) {
  MAKE_BUFFER_NAME(ensure_capacity)(buf, buf->length + 1);
  ((BUFFER_TYPE*) buf->memory.memory)[buf->length] = value;
  buf->length++;
}

void MAKE_BUFFER_NAME(flush)(MAKE_BUFFER_NAME(t) *buf, blob_t *blob_out) {
  blob_init(blob_out, (byte_t*) buf->memory.memory, buf->length * sizeof(BUFFER_TYPE));
}

void MAKE_BUFFER_NAME(append_cursor)(MAKE_BUFFER_NAME(t) *buf,
    MAKE_BUFFER_NAME(cursor_t) *cursor_out) {
  cursor_out->buf = buf;
  cursor_out->offset = buf->length;
  MAKE_BUFFER_NAME(append)(buf, 0);
}

void MAKE_BUFFER_NAME(cursor_set)(MAKE_BUFFER_NAME(cursor_t) *cursor,
    BUFFER_TYPE value) {
  BUFFER_TYPE *block = (BUFFER_TYPE*) cursor->buf->memory.memory;
  block[cursor->offset] = value;
}
