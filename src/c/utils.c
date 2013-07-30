// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "check.h"
#include "utils.h"

#include <string.h>
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

// Throws away the data argument and just calls malloc.
static void *system_malloc_trampoline(void *data, size_t size) {
  CHECK_EQ("invalid system allocator", NULL, data);
  void *result = malloc(size);
  memset(result, kMallocHeapMarker, size);
  return result;
}

// Throws away the data argument and just calls free.
static void system_free_trampoline(void *data, void *ptr) {
  CHECK_EQ("invalid system allocator", NULL, data);
  free(ptr);
}

void init_system_allocator(allocator_t *alloc) {
  alloc->malloc = system_malloc_trampoline;
  alloc->free = system_free_trampoline;
  alloc->data = NULL;
}

address_t allocator_malloc(allocator_t *alloc, size_t size) {
  return (alloc->malloc)(alloc->data, size);
}

void allocator_free(allocator_t *alloc, address_t ptr) {
  (alloc->free)(alloc->data, ptr);
}

void string_buffer_init(string_buffer_t *buf, allocator_t *alloc_or_null) {
  if (alloc_or_null == NULL) {
    init_system_allocator(&buf->allocator);
  } else {
    buf->allocator = *alloc_or_null;
  }
  buf->length = 0;
  buf->capacity = 128;
  buf->chars = (char*) allocator_malloc(&buf->allocator, buf->capacity);
}

void string_buffer_dispose(string_buffer_t *buf) {
  allocator_free(&buf->allocator, (address_t) buf->chars);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void string_buffer_ensure_capacity(string_buffer_t *buf,
    size_t length) {
  if (length < buf->capacity)
    return;
  size_t new_capacity = (length * 2);
  char *new_chars = (char*) allocator_malloc(&buf->allocator,
      new_capacity);
  memcpy(new_chars, buf->chars, buf->length);
  allocator_free(&buf->allocator, (address_t) buf->chars);
  buf->chars = new_chars;
  buf->capacity = new_capacity;
}

// Appends the given string to the string buffer, extending it as necessary.
static void string_buffer_append(string_buffer_t *buf, string_t *str) {
  string_buffer_ensure_capacity(buf, buf->length + string_length(str));
  string_copy_to(str, buf->chars + buf->length, buf->capacity - buf->length);
  buf->length += string_length(str);
}

void string_buffer_putc(string_buffer_t *buf, char c) {
  string_buffer_ensure_capacity(buf, buf->length + 1);
  buf->chars[buf->length] = c;
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
  CHECK_TRUE("no room for null terminator", buf->length < buf->capacity);
  buf->chars[buf->length] = '\0';
  str_out->length = buf->length;
  str_out->chars = buf->chars;
}


// --- B y t e   b u f f e r ---

void byte_buffer_init(byte_buffer_t *buf, allocator_t *alloc_or_null) {
  if (alloc_or_null == NULL) {
    init_system_allocator(&buf->allocator);
  } else {
    buf->allocator = *alloc_or_null;
  }
  buf->length = 0;
  buf->capacity = 128;
  buf->data = allocator_malloc(&buf->allocator, buf->capacity);
}

void byte_buffer_dispose(byte_buffer_t *buf) {
  allocator_free(&buf->allocator, (address_t) buf->data);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void byte_buffer_ensure_capacity(byte_buffer_t *buf, size_t length) {
  if (length < buf->capacity)
    return;
  size_t new_capacity = (length * 2);
  uint8_t *new_data = allocator_malloc(&buf->allocator, new_capacity);
  memcpy(new_data, buf->data, buf->length);
  allocator_free(&buf->allocator, (address_t) buf->data);
  buf->data = new_data;
  buf->capacity = new_capacity;
}

void byte_buffer_append(byte_buffer_t *buf, uint8_t value) {
  byte_buffer_ensure_capacity(buf, buf->length + 1);
  buf->data[buf->length] = value;
  buf->length++;
}

void byte_buffer_flush(byte_buffer_t *buf, blob_t *blob_out) {
  blob_init(blob_out, buf->data, buf->length);
}
