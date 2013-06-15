#include "heap.h"

void set_object_species(value_t value, value_t species) {
  *access_object_field(value, 0) = species;
}

value_t get_object_species(value_t value) {
  return *access_object_field(value, 0);
}

object_family_t get_object_family(value_t value) {
  value_t species = get_object_species(value);
  return get_species_instance_family(species);
}

void set_species_instance_family(value_t value, object_family_t instance_family) {
  *access_object_field(value, 1) = (value_t) instance_family;
}

object_family_t get_species_instance_family(value_t value) {
  return (object_family_t) *access_object_field(value, 1);
}


size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kObjectHeaderSize               // header
       + kValueSize                      // length
       + align_size(kValueSize, bytes);  // contents
}

void set_string_length(value_t value, size_t length) {
  CHECK_FAMILY(ofString, value);
  *access_object_field(value, 1) = (value_t) length;
}

size_t get_string_length(value_t value) {
  CHECK_FAMILY(ofString, value);
  return (size_t) *access_object_field(value, 1);
}

char *get_string_chars(value_t value) {
  CHECK_FAMILY(ofString, value);
  return (char*) access_object_field(value, 2);  
}

void get_string_contents(value_t value, string_t *out) {
  out->length = get_string_length(value);
  out->chars = get_string_chars(value);
}


size_t calc_array_size(size_t length) {
  return kObjectHeaderSize       // header
       + kValueSize              // length
       + (length * kValueSize);  // contents
}

size_t get_array_length(value_t value) {
  CHECK_FAMILY(ofArray, value);
  return (size_t) *access_object_field(value, 1);
}

void set_array_length(value_t value, size_t length) {
  CHECK_FAMILY(ofArray, value);
  *access_object_field(value, 1) = (value_t) length;
}

value_t get_array_at(value_t value, size_t index) {
  CHECK_FAMILY(ofArray, value);
  CHECK_TRUE(index < get_array_length(value));
  return *access_object_field(value, 2 + index);
}

void set_array_at(value_t value, size_t index, value_t element) {
  CHECK_FAMILY(ofArray, value);
  CHECK_TRUE(index < get_array_length(value));
  *access_object_field(value, 2 + index) = element;
}
