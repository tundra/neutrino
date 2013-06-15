#include "heap.h"

size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kObjectHeaderSize  // header
       + kValueSize  // length
       + align_size(kValueSize, bytes);  // contents
}

void set_string_length(value_ptr_t value, size_t length) {
  CHECK_EQ(otString, get_object_type(value));
  *access_object_field(value, 1) = (value_ptr_t) length;
}

size_t get_string_length(value_ptr_t value) {
  CHECK_EQ(otString, get_object_type(value));
  return (size_t) *access_object_field(value, 1);
}

char *get_string_chars(value_ptr_t value) {
  CHECK_EQ(otString, get_object_type(value));
  return (char*) access_object_field(value, 2);  
}

void get_string_contents(value_ptr_t value, string_t *out) {
  out->length = get_string_length(value);
  out->chars = get_string_chars(value);
}