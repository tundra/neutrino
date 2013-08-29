// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "check.h"
#include "globals.h"
#include "utils.h"

#include <stdarg.h>


void string_init(string_t *str, const char *chars) {
  str->chars = chars;
  str->length = strlen(chars);
}

size_t string_length(string_t *str) {
  return str->length;
}

char string_char_at(string_t *str, size_t index) {
  CHECK_TRUE("string index out of bounds", index < string_length(str));
  return str->chars[index];
}

void string_copy_to(string_t *str, char *dest, size_t count) {
  // The count must be strictly greater than the number of chars because we
  // also need to fit the terminating null character.
  CHECK_TRUE("string copy destination too small", string_length(str) < count);
  strncpy(dest, str->chars, string_length(str) + 1);
}

bool string_equals(string_t *a, string_t *b) {
  size_t length = string_length(a);
  if (length != string_length(b))
    return false;
  for (size_t i = 0; i < length; i++) {
    if (string_char_at(a, i) != string_char_at(b, i))
      return false;
  }
  return true;
}

int string_compare(string_t *a, string_t *b) {
  size_t a_length = string_length(a);
  size_t b_length = string_length(b);
  if (a_length != b_length)
    return a_length - b_length;
  for (size_t i = 0; i < a_length; i++) {
    char a_char = string_char_at(a, i);
    char b_char = string_char_at(b, i);
    if (a_char != b_char)
      return a_char - b_char;
  }
  return 0;
}

bool string_equals_cstr(string_t *a, const char *str) {
  string_t b;
  string_init(&b, str);
  return string_equals(a, &b);
}

size_t string_hash(string_t *str) {
  // This is a dreadful hash but it has the right properties. Improve later.
  size_t length = string_length(str);
  size_t result = length;
  for (size_t i = 0; i < length; i++)
    result = (result << 1) ^ (string_char_at(str, i));
  return result;
}


void blob_init(blob_t *blob, byte_t *data, size_t length) {
  blob->length = length;
  blob->data = data;
}

size_t blob_length(blob_t *blob) {
  return blob->length;
}

byte_t blob_byte_at(blob_t *blob, size_t index) {
  CHECK_TRUE("blob index out of bounds", index < blob_length(blob));
  return blob->data[index];
}

void blob_fill(blob_t *blob, byte_t value) {
  memset(blob->data, value, blob_length(blob));
}

void blob_copy_to(blob_t *src, blob_t *dest) {
  CHECK_TRUE("blob copy destination too small", blob_length(dest) >= blob_length(src));
  memcpy(dest->data, src->data, blob_length(src));
}

static const byte_t kMallocHeapMarker = 0xB0;

memory_block_t memory_block_empty() {
  return new_memory_block(NULL, 0);
}

memory_block_t new_memory_block(void *memory, size_t size) {
  return (memory_block_t) {memory, size};
}

bool memory_block_is_empty(memory_block_t block) {
  return block.memory == NULL;
}

// Throws away the data argument and just calls malloc.
static memory_block_t system_malloc_trampoline(void *data, size_t size) {
  CHECK_EQ("invalid system allocator", NULL, data);
  void *result = malloc(size);
  if (result == NULL) {
    return memory_block_empty();
  } else {
    memset(result, kMallocHeapMarker, size);
    return (memory_block_t) {result, size};
  }
}

// Throws away the data argument and just calls free.
static void system_free_trampoline(void *data, memory_block_t memory) {
  CHECK_EQ("invalid system allocator", NULL, data);
  free(memory.memory);
}

void init_system_allocator(allocator_t *alloc) {
  alloc->malloc = system_malloc_trampoline;
  alloc->free = system_free_trampoline;
  alloc->data = NULL;
}

memory_block_t allocator_malloc(allocator_t *alloc, size_t size) {
  return (alloc->malloc)(alloc->data, size);
}

void allocator_free(allocator_t *alloc, memory_block_t memory) {
  (alloc->free)(alloc->data, memory);
}

memory_block_t allocator_default_malloc(size_t size) {
  return allocator_malloc(allocator_get_default(), size);
}

void allocator_default_free(memory_block_t block) {
  allocator_free(allocator_get_default(), block);
}

allocator_t kSystemAllocator;
allocator_t *allocator_default = NULL;

allocator_t *allocator_get_default() {
  if (allocator_default == NULL) {
    init_system_allocator(&kSystemAllocator);
    allocator_default = &kSystemAllocator;
  }
  return allocator_default;
}

allocator_t *allocator_set_default(allocator_t *value) {
  allocator_t *previous = allocator_get_default();
  allocator_default = value;
  return previous;
}

void string_buffer_init(string_buffer_t *buf) {
  buf->length = 0;
  buf->memory = allocator_default_malloc(128);
}

void string_buffer_dispose(string_buffer_t *buf) {
  allocator_default_free(buf->memory);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void string_buffer_ensure_capacity(string_buffer_t *buf,
    size_t length) {
  if (length < buf->memory.size)
    return;
  size_t new_capacity = (length * 2);
  memory_block_t new_memory = allocator_default_malloc(new_capacity);
  memcpy(new_memory.memory, buf->memory.memory, buf->length);
  allocator_default_free(buf->memory);
  buf->memory = new_memory;
}

void string_buffer_append(string_buffer_t *buf, string_t *str) {
  string_buffer_ensure_capacity(buf, buf->length + string_length(str));
  char *chars = buf->memory.memory;
  string_copy_to(str, chars + buf->length, buf->memory.size - buf->length);
  buf->length += string_length(str);
}

void string_buffer_putc(string_buffer_t *buf, char c) {
  string_buffer_ensure_capacity(buf, buf->length + 1);
  char *chars = buf->memory.memory;
  chars[buf->length] = c;
  buf->length++;
}

void string_buffer_printf(string_buffer_t *buf, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(buf, fmt, argp);
  va_end(argp);
}

void string_buffer_vprintf(string_buffer_t *buf, const char *fmt, va_list argp) {
  // Write the formatted string into a temporary buffer.
  static const size_t kMaxSize = 1024;
  char buffer[kMaxSize + 1];
  // Null terminate explicitly just to be on the safe side.
  buffer[kMaxSize] = '\0';
  size_t written = vsnprintf(buffer, kMaxSize, fmt, argp);
  // TODO: fix this if we ever hit it.
  CHECK_TRUE("temp buffer too small", written < kMaxSize);
  // Then write the temp string into the string buffer.
  string_t data = {written, buffer};
  string_buffer_append(buf, &data);
}

void string_buffer_flush(string_buffer_t *buf, string_t *str_out) {
  CHECK_TRUE("no room for null terminator", buf->length < buf->memory.size);
  char *chars = buf->memory.memory;
  chars[buf->length] = '\0';
  str_out->length = buf->length;
  str_out->chars = chars;
}


// --- B y t e   b u f f e r ---

void byte_buffer_init(byte_buffer_t *buf) {
  buf->length = 0;
  buf->memory = allocator_default_malloc(128);
}

void byte_buffer_dispose(byte_buffer_t *buf) {
  allocator_default_free(buf->memory);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void byte_buffer_ensure_capacity(byte_buffer_t *buf, size_t length) {
  if (length < buf->memory.size)
    return;
  size_t new_capacity = (length * 2);
  memory_block_t new_memory = allocator_default_malloc(new_capacity);
  memcpy(new_memory.memory, buf->memory.memory, buf->length);
  allocator_default_free(buf->memory);
  buf->memory = new_memory;
}

void byte_buffer_append(byte_buffer_t *buf, uint8_t value) {
  byte_buffer_ensure_capacity(buf, buf->length + 1);
  ((byte_t*) buf->memory.memory)[buf->length] = value;
  buf->length++;
}

void byte_buffer_flush(byte_buffer_t *buf, blob_t *blob_out) {
  blob_init(blob_out, buf->memory.memory, buf->length);
}


// --- B i t   v e c t o r ---

static bool bit_vector_is_small(bit_vector_t *vector) {
  return vector->length < kSmallBitVectorLimit;
}

value_t bit_vector_init(bit_vector_t *vector, size_t length, bool value) {
  vector->length = length;
  size_t byte_size = align_size(8, length) >> 3;
  if (bit_vector_is_small(vector)) {
    vector->data = vector->storage.as_small.inline_data;
  } else {
    memory_block_t memory = allocator_default_malloc(byte_size);
    vector->storage.as_large.memory = memory;
    vector->data = memory.memory;
  }
  memset(vector->data, value ? 0xFF : 0x00, byte_size);
  return success();
}

void bit_vector_dispose(bit_vector_t *vector) {
  if (!bit_vector_is_small(vector))
    allocator_default_free(vector->storage.as_large.memory);
}

void bit_vector_set_at(bit_vector_t *vector, size_t index, bool value) {
  CHECK_TRUE("set bit vector out of bounds", index < vector->length);
  size_t segment = index >> 3;
  size_t offset = (index & 0x7);
  if (value) {
    vector->data[segment] |= (1 << offset);
  } else {
    vector->data[segment] &= ~(1 << offset);
  }
}

bool bit_vector_get_at(bit_vector_t *vector, size_t index) {
  CHECK_TRUE("get bit vector out of bounds", index < vector->length);
  size_t segment = index >> 3;
  size_t offset = (index & 0x7);
  return (vector->data[segment] >> offset) & 0x1;
}
