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
  CHECK_TRUE(index < string_length(str));
  return str->chars[index];
}

void string_copy_to(string_t *str, char *dest, size_t count) {
  // The count must be strictly greater than the number of chars because we
  // also need to fit the terminating null character.
  CHECK_TRUE(string_length(str) < count);
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

size_t string_hash(string_t *str) {
  // This is a dreadful hash but it has the right properties. Improve later.
  size_t length = string_length(str);
  size_t result = length;
  for (size_t i = 0; i < length; i++)
    result = (result << 1) ^ (string_char_at(str, i));
  return result;
}

// Throws away the data argument and just calls malloc.
static address_t system_malloc_trampoline(void *data, size_t size) {
  CHECK_EQ(NULL, data);
  return (address_t) malloc(size);
}

// Throws away the data argument and just calls free.
static void system_free_trampoline(void *data, address_t ptr) {
  CHECK_EQ(NULL, data);
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

void string_buffer_printf(string_buffer_t *buf, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  // Write the formatted string into a temporary buffer.
  static const size_t kMaxSize = 1024;
  char buffer[kMaxSize + 1];
  // Null terminate explicitly just to be on the safe side.
  buffer[kMaxSize] = '\0';
  size_t written = vsnprintf(buffer, kMaxSize, fmt, argp);
  // Then write the temp string into the string buffer.
  string_t data = {written, buffer};
  string_buffer_append(buf, &data);
  va_end(argp);
}

void string_buffer_flush(string_buffer_t *buf, string_t *str_out) {
  CHECK_TRUE(buf->length < buf->capacity);
  buf->chars[buf->length] = '\0';
  str_out->length = buf->length;
  str_out->chars = buf->chars;
}
