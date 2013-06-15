#include "check.h"
#include "utils.h"

#include <string.h>

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
