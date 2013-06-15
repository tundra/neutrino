#include "heap.h"

void set_object_species(value_t value, value_t species) {
  *access_object_field(value, 0) = species;
}

value_t get_object_species(value_t value) {
  return *access_object_field(value, 0);
}

object_type_t get_object_type(value_t value) {
  value_t species = get_object_species(value);
  return get_species_instance_type(species);
}

void set_species_instance_type(value_t value, object_type_t instance_type) {
  *access_object_field(value, 1) = (value_t) instance_type;
}

object_type_t get_species_instance_type(value_t value) {
  return (object_type_t) *access_object_field(value, 1);
}


size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kObjectHeaderSize  // header
       + kValueSize  // length
       + align_size(kValueSize, bytes);  // contents
}

void set_string_length(value_t value, size_t length) {
  CHECK_EQ(otString, get_object_type(value));
  *access_object_field(value, 1) = (value_t) length;
}

size_t get_string_length(value_t value) {
  CHECK_EQ(otString, get_object_type(value));
  return (size_t) *access_object_field(value, 1);
}

char *get_string_chars(value_t value) {
  CHECK_EQ(otString, get_object_type(value));
  return (char*) access_object_field(value, 2);  
}

void get_string_contents(value_t value, string_t *out) {
  out->length = get_string_length(value);
  out->chars = get_string_chars(value);
}