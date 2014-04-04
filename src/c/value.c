//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "freeze.h"
#include "heap.h"
#include "interp.h"
#include "log.h"
#include "runtime.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "value-inl.h"

const char *get_value_domain_name(value_domain_t domain) {
  switch (domain) {
#define __EMIT_DOMAIN_CASE__(Name, TAG, ORDINAL) case vd##Name: return #Name;
  FOR_EACH_VALUE_DOMAIN(__EMIT_DOMAIN_CASE__)
#undef __EMIT_DOMAIN_CASE__
    default:
      return "invalid domain";
  }
}

int get_value_domain_ordinal(value_domain_t domain) {
  switch (domain) {
#define __EMIT_DOMAIN_CASE__(Name, TAG, ORDINAL) case vd##Name: return (ORDINAL);
  FOR_EACH_VALUE_DOMAIN(__EMIT_DOMAIN_CASE__)
#undef __EMIT_DOMAIN_CASE__
    default:
      return -1;
  }
}

const char *get_heap_object_family_name(heap_object_family_t family) {
  switch (family) {
#define __GEN_CASE__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW, N)\
    case of##Family: return #Family;
    ENUM_HEAP_OBJECT_FAMILIES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "invalid family";
  }
}

const char *get_species_division_name(species_division_t division) {
  switch (division) {
#define __GEN_CASE__(Name, name) case sd##Name: return #Name;
    ENUM_SPECIES_DIVISIONS(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "invalid division";
  }
}

const char *get_custom_tagged_phylum_name(custom_tagged_phylum_t phylum) {
  switch (phylum) {
#define __GEN_CASE__(Name, name, CM, SR) case tp##Name: return #Name;
    ENUM_CUSTOM_TAGGED_PHYLUMS(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return "invalid phylum";
  }
}


// --- I n t e g e r ---

static value_t integer_plus_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(self.encoded + that.encoded);
}

static value_t integer_minus_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(self.encoded - that.encoded);
}

static value_t integer_times_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(self) * get_integer_value(that);
  return new_integer(result);
}

static value_t integer_divide_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(self) / get_integer_value(that);
  return new_integer(result);
}

static value_t integer_modulo_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(self) % get_integer_value(that);
  return new_integer(result);
}

static value_t integer_less_integer(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, self);
  CHECK_DOMAIN(vdInteger, that);
  return new_boolean(get_integer_value(self) < get_integer_value(that));
}

static value_t integer_negate(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_DOMAIN(vdInteger, self);
  return decode_value(-self.encoded);
}

static value_t integer_print(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  print_ln("%v", self);
  return nothing();
}

value_t add_integer_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("-int", 0, integer_negate);
  ADD_BUILTIN_IMPL("int+int", 1, integer_plus_integer);
  ADD_BUILTIN_IMPL("int-int", 1, integer_minus_integer);
  ADD_BUILTIN_IMPL("int*int", 1, integer_times_integer);
  ADD_BUILTIN_IMPL("int/int", 1, integer_divide_integer);
  ADD_BUILTIN_IMPL("int%int", 1, integer_modulo_integer);
  ADD_BUILTIN_IMPL("int<int", 1, integer_less_integer);
  ADD_BUILTIN_IMPL("int.print()", 0, integer_print);
  return success();
}


// --- O b j e c t ---

void set_heap_object_header(value_t value, value_t species) {
  *access_heap_object_field(value, kHeapObjectHeaderOffset) = species;
}

value_t get_heap_object_header(value_t value) {
  return *access_heap_object_field(value, kHeapObjectHeaderOffset);
}

void set_heap_object_species(value_t value, value_t species) {
  CHECK_FAMILY(ofSpecies, species);
  set_heap_object_header(value, species);
}

bool in_syntax_family(value_t value) {
  if (!is_heap_object(value))
    return false;
  switch (get_heap_object_family(value)) {
#define __MAKE_CASE__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW, N)   \
  EM(                                                                          \
    case of##Family: return true;,                                             \
    )
    ENUM_HEAP_OBJECT_FAMILIES(__MAKE_CASE__)
#undef __MAKE_CASE__
    default:
      return false;
  }
}

const char *in_syntax_family_name(bool value) {
  return value ? "true" : "false";
}

family_behavior_t *get_heap_object_family_behavior(value_t self) {
  CHECK_DOMAIN(vdHeapObject, self);
  value_t species = get_heap_object_species(self);
  CHECK_TRUE("invalid object header", in_family(ofSpecies, species));
  return get_species_family_behavior(species);
}

family_behavior_t *get_heap_object_family_behavior_unchecked(value_t self) {
  CHECK_DOMAIN(vdHeapObject, self);
  value_t species = get_heap_object_species(self);
  return get_species_family_behavior(species);
}

static value_t object_is_identical(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  return new_boolean(value_identity_compare(self, that));
}

value_t add_heap_object_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("obj.is_identical?()", 1, object_is_identical);
  return success();
}


// --- S p e c i e s ---

TRIVIAL_PRINT_ON_IMPL(Species, species);

void set_species_instance_family(value_t self, heap_object_family_t family) {
  value_t as_value;
  // This is only possible because of the way the family enum values are
  // constructed.
  as_value.encoded = family;
  CHECK_DOMAIN(vdInteger, as_value);
  *access_heap_object_field(self, kSpeciesInstanceFamilyOffset) = as_value;
}

void set_species_family_behavior(value_t value, family_behavior_t *behavior) {
  *access_heap_object_field(value, kSpeciesFamilyBehaviorOffset) = pointer_to_value_bit_cast(behavior);
}

family_behavior_t *get_species_family_behavior(value_t value) {
  void *ptr = value_to_pointer_bit_cast(*access_heap_object_field(value, kSpeciesFamilyBehaviorOffset));
  return (family_behavior_t*) ptr;
}

void set_species_division_behavior(value_t value, division_behavior_t *behavior) {
  *access_heap_object_field(value, kSpeciesDivisionBehaviorOffset) = pointer_to_value_bit_cast(behavior);
}

division_behavior_t *get_species_division_behavior(value_t value) {
  void *ptr = value_to_pointer_bit_cast(*access_heap_object_field(value, kSpeciesDivisionBehaviorOffset));
  return (division_behavior_t*) ptr;
}

species_division_t get_species_division(value_t value) {
  return get_species_division_behavior(value)->division;
}

species_division_t get_heap_object_division(value_t value) {
  return get_species_division(get_heap_object_species(value));
}

value_t species_validate(value_t value) {
  VALIDATE_FAMILY(ofSpecies, value);
  return success();
}

void get_species_layout(value_t value, heap_object_layout_t *layout) {
  division_behavior_t *behavior = get_species_division_behavior(value);
  (behavior->get_species_layout)(value, layout);
}


// --- C o m p a c t   s p e c i e s ---

void get_compact_species_layout(value_t species, heap_object_layout_t *layout) {
  // Compact species have no value fields.
  heap_object_layout_set(layout, kCompactSpeciesSize, kCompactSpeciesSize);
}


// --- I n s t a n c e   s p e c i e s ---

void get_instance_species_layout(value_t species, heap_object_layout_t *layout) {
  // The object is kInstanceSpeciesSize large and the values start after the
  // header.
  heap_object_layout_set(layout, kInstanceSpeciesSize, kSpeciesHeaderSize);
}

CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance,
    acInFamily, ofType, PrimaryTypeField, primary_type_field);
CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance,
    acInFamilyOpt, ofInstanceManager, Manager, manager);


// --- M o d a l   s p e c i e s ---

void get_modal_species_layout(value_t species, heap_object_layout_t *layout) {
  // The object is kModalSpeciesSize large and there are no values. Well okay
  // the mode is stored as a tagged integer but that doesn't count.
  heap_object_layout_set(layout, kModalSpeciesSize, kModalSpeciesSize);
}

value_mode_t get_modal_species_mode(value_t value) {
  value_t mode = *access_heap_object_field(value, kModalSpeciesModeOffset);
  return (value_mode_t) get_integer_value(mode);
}

void set_modal_species_mode(value_t value, value_mode_t mode) {
  *access_heap_object_field(value, kModalSpeciesModeOffset) = new_integer(mode);
}

value_mode_t get_modal_heap_object_mode(value_t value) {
  value_t species = get_heap_object_species(value);
  CHECK_DIVISION(sdModal, species);
  return get_modal_species_mode(species);
}

void set_modal_heap_object_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  value_t old_species = get_heap_object_species(self);
  value_t new_species = get_modal_species_sibling_with_mode(runtime, old_species,
      mode);
  set_heap_object_species(self, new_species);
}

size_t get_modal_species_base_root(value_t value) {
  value_t base_root = *access_heap_object_field(value, kModalSpeciesBaseRootOffset);
  return get_integer_value(base_root);
}

void set_modal_species_base_root(value_t value, size_t base_root) {
  *access_heap_object_field(value, kModalSpeciesBaseRootOffset) = new_integer(base_root);
}


// --- S t r i n g ---

GET_FAMILY_PRIMARY_TYPE_IMPL(string);
FIXED_GET_MODE_IMPL(string, vmDeepFrozen);

size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kHeapObjectHeaderSize               // header
       + kValueSize                      // length
       + align_size(kValueSize, bytes);  // contents
}

INTEGER_ACCESSORS_IMPL(String, string, Length, length);

char *get_string_chars(value_t value) {
  CHECK_FAMILY(ofString, value);
  return (char*) access_heap_object_field(value, kStringCharsOffset);
}

void get_string_contents(value_t value, string_t *out) {
  out->length = get_string_length(value);
  out->chars = get_string_chars(value);
}

value_t string_validate(value_t value) {
  VALIDATE_FAMILY(ofString, value);
  // Check that the string is null-terminated.
  size_t length = get_string_length(value);
  VALIDATE(get_string_chars(value)[length] == '\0');
  return success();
}

void get_string_layout(value_t value, heap_object_layout_t *layout) {
  // Strings have no value fields.
  size_t size = calc_string_size(get_string_length(value));
  heap_object_layout_set(layout, size, size);
}

value_t string_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  string_t contents;
  get_string_contents(value, &contents);
  hash_stream_write_data(stream, contents.chars, contents.length);
  return success();
}

value_t string_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  CHECK_FAMILY(ofString, a);
  CHECK_FAMILY(ofString, b);
  string_t a_contents;
  get_string_contents(a, &a_contents);
  string_t b_contents;
  get_string_contents(b, &b_contents);
  return new_boolean(string_equals(&a_contents, &b_contents));
}

value_t string_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofString, a);
  CHECK_FAMILY(ofString, b);
  string_t a_contents;
  get_string_contents(a, &a_contents);
  string_t b_contents;
  get_string_contents(b, &b_contents);
  return integer_to_relation(string_compare(&a_contents, &b_contents));
}

void string_print_on(value_t value, print_on_context_t *context) {
  if ((context->flags & pfUnquote) == 0)
    string_buffer_putc(context->buf, '"');
  string_buffer_append_string(context->buf, value);
  if ((context->flags & pfUnquote) == 0)
    string_buffer_putc(context->buf, '"');
}

void string_buffer_append_string(string_buffer_t *buf, value_t value) {
  CHECK_FAMILY(ofString, value);
  string_t contents;
  get_string_contents(value, &contents);
  string_buffer_printf(buf, "%s", contents.chars);
}

static value_t string_plus_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofString, self);
  CHECK_FAMILY(ofString, that);
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_t str;
  get_string_contents(self, &str);
  string_buffer_append(&buf, &str);
  get_string_contents(that, &str);
  string_buffer_append(&buf, &str);
  string_buffer_flush(&buf, &str);
  TRY_DEF(result, new_heap_string(get_builtin_runtime(args), &str));
  string_buffer_dispose(&buf);
  return result;
}

static value_t string_equals_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofString, self);
  CHECK_FAMILY(ofString, that);
  return new_boolean(value_identity_compare(self, that));
}

static value_t string_print_raw(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofString, self);
  string_t contents;
  get_string_contents(self, &contents);
  fwrite(contents.chars, sizeof(char), contents.length, stdout);
  fputc('\n', stdout);
  return nothing();
}

static value_t string_get_ascii_characters(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofString, self);
  string_t contents;
  get_string_contents(self, &contents);
  size_t length = string_length(&contents);
  TRY_DEF(result, new_heap_array(runtime, length));
  for (size_t i = 0; i < length; i++) {
    char c = string_char_at(&contents, i);
    char char_c_str[2] = {c, '\0'};
    string_t char_str = {1, char_c_str};
    TRY_DEF(char_obj, new_heap_string(runtime, &char_str));
    set_array_at(result, i, char_obj);
  }
  return result;
}

value_t add_string_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("str+str", 1, string_plus_string);
  ADD_BUILTIN_IMPL("str==str", 1, string_equals_string);
  ADD_BUILTIN_IMPL("str.print_raw()", 0, string_print_raw);
  ADD_BUILTIN_IMPL("str.get_ascii_characters()", 0, string_get_ascii_characters);
  return success();
}


// --- B l o b ---

GET_FAMILY_PRIMARY_TYPE_IMPL(blob);
NO_BUILTIN_METHODS(blob);
FIXED_GET_MODE_IMPL(blob, vmDeepFrozen);

size_t calc_blob_size(size_t size) {
  return kHeapObjectHeaderSize              // header
       + kValueSize                     // length
       + align_size(kValueSize, size);  // contents
}

INTEGER_ACCESSORS_IMPL(Blob, blob, Length, length);

void get_blob_data(value_t value, blob_t *blob_out) {
  CHECK_FAMILY(ofBlob, value);
  size_t length = get_blob_length(value);
  byte_t *data = (byte_t*) access_heap_object_field(value, kBlobDataOffset);
  blob_init(blob_out, data, length);
}

value_t blob_validate(value_t value) {
  VALIDATE_FAMILY(ofBlob, value);
  return success();
}

void get_blob_layout(value_t value, heap_object_layout_t *layout) {
  // Blobs have no value fields.
  size_t size = calc_blob_size(get_blob_length(value));
  heap_object_layout_set(layout, size, size);
}

void blob_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBlob, value);
  string_buffer_printf(context->buf, "#<blob: [");
  blob_t blob;
  get_blob_data(value, &blob);
  size_t length = blob_byte_length(&blob);
  size_t bytes_to_show = (length <= 8) ? length : 8;
  for (size_t i = 0; i < bytes_to_show; i++) {
    static const char *kChars = "0123456789abcdef";
    byte_t byte = blob_byte_at(&blob, i);
    string_buffer_printf(context->buf, "%c%c", kChars[byte >> 4], kChars[byte & 0xF]);
  }
  if (bytes_to_show < length)
    string_buffer_printf(context->buf, "...");
  string_buffer_printf(context->buf, "]>");
}


// --- V o i d   P ---

TRIVIAL_PRINT_ON_IMPL(VoidP, void_p);
FIXED_GET_MODE_IMPL(void_p, vmDeepFrozen);

void set_void_p_value(value_t value, void *ptr) {
  CHECK_FAMILY(ofVoidP, value);
  *access_heap_object_field(value, kVoidPValueOffset) = pointer_to_value_bit_cast(ptr);
}

void *get_void_p_value(value_t value) {
  CHECK_FAMILY(ofVoidP, value);
  return value_to_pointer_bit_cast(*access_heap_object_field(value, kVoidPValueOffset));
}

value_t void_p_validate(value_t value) {
  VALIDATE_FAMILY(ofVoidP, value);
  return success();
}

void get_void_p_layout(value_t value, heap_object_layout_t *layout) {
  // A void-p has no value fields.
  heap_object_layout_set(layout, kVoidPSize, kVoidPSize);
}


/// ## Array

GET_FAMILY_PRIMARY_TYPE_IMPL(array);

size_t calc_array_size(size_t length) {
  return kHeapObjectHeaderSize       // header
       + kValueSize              // length
       + (length * kValueSize);  // contents
}

INTEGER_ACCESSORS_IMPL(Array, array, Length, length);

value_t get_array_at(value_t value, size_t index) {
  CHECK_FAMILY(ofArray, value);
  COND_CHECK_TRUE("array index out of bounds", ccOutOfBounds,
      index < get_array_length(value));
  return get_array_elements(value)[index];
}

void set_array_at(value_t value, size_t index, value_t element) {
  CHECK_FAMILY(ofArray, value);
  CHECK_MUTABLE(value);
  CHECK_REL("array index out of bounds", index, <, get_array_length(value));
  get_array_elements(value)[index] = element;
}

value_t *get_array_elements(value_t value) {
  CHECK_FAMILY(ofArray, value);
  return get_array_elements_unchecked(value);
}

value_t *get_array_elements_unchecked(value_t value) {
  return access_heap_object_field(value, kArrayElementsOffset);
}

value_t array_validate(value_t value) {
  VALIDATE_FAMILY(ofArray, value);
  return success();
}

void get_array_layout(value_t value, heap_object_layout_t *layout) {
  size_t size = calc_array_size(get_array_length(value));
  heap_object_layout_set(layout, size, kArrayElementsOffset);
}

void array_print_on(value_t value, print_on_context_t *context) {
  if (context->depth == 1) {
    // If we can't print the elements anyway we might as well just show the
    // count.
    string_buffer_printf(context->buf, "#<array[%i]>", (int) get_array_length(value));
  } else {
    string_buffer_printf(context->buf, "[");
    for (size_t i = 0; i < get_array_length(value); i++) {
      if (i > 0)
        string_buffer_printf(context->buf, ", ");
      value_print_inner_on(get_array_at(value, i), context, -1);
    }
    string_buffer_printf(context->buf, "]");
  }
}

// Compares two values pointed to by two void pointers.
static int value_compare_function(const void *vp_a, const void *vp_b) {
  value_t a = *((const value_t*) vp_a);
  value_t b = *((const value_t*) vp_b);
  value_t comparison = value_ordering_compare(a, b);
  CHECK_FALSE("not comparable", is_condition(comparison));
  return relation_to_integer(comparison);
}

value_t sort_array(value_t value) {
  return sort_array_partial(value, get_array_length(value));
}

value_t sort_array_partial(value_t value, size_t elmc) {
  CHECK_FAMILY(ofArray, value);
  CHECK_MUTABLE(value);
  CHECK_REL("sorting out of bounds", elmc, <=, get_array_length(value));
  value_t *elements = get_array_elements(value);
  // Just use qsort. This means that we can't propagate conditions from the
  // compare functions back out but that shouldn't be a huge issue. We'll check
  // on them for now and later on this will have to be rewritten in n anyway.
  qsort(elements, elmc, kValueSize, &value_compare_function);
  return success();
}

bool is_array_sorted(value_t value) {
  CHECK_FAMILY(ofArray, value);
  size_t length = get_array_length(value);
  for (size_t i = 1; i < length; i++) {
    value_t a = get_array_at(value, i - 1);
    value_t b = get_array_at(value, i);
    if (test_relation(value_ordering_compare(a, b), reGreaterThan))
      return false;
  }
  return true;
}

void set_pair_array_first_at(value_t self, size_t index, value_t value) {
  set_array_at(self, index << 1, value);
}

value_t get_pair_array_first_at(value_t self, size_t index) {
  return get_array_at(self, index << 1);
}

void set_pair_array_second_at(value_t self, size_t index, value_t value) {
  set_array_at(self, (index << 1) + 1, value);
}

value_t get_pair_array_second_at(value_t self, size_t index) {
  return get_array_at(self, (index << 1) + 1);
}

size_t get_pair_array_length(value_t self) {
  return get_array_length(self) >> 1;
}

value_t co_sort_pair_array(value_t value) {
  CHECK_FAMILY(ofArray, value);
  CHECK_MUTABLE(value);
  size_t length = get_array_length(value);
  CHECK_EQ("pair sorting odd-length array", 0, length & 1);
  size_t pair_count = length >> 1;
  value_t *elements = get_array_elements(value);
  // The value compare function works in this case too because it'll compare the
  // first value pointed to by its arguments, it doesn't care if there are more
  // values after it.
  qsort(elements, pair_count, kValueSize << 1, &value_compare_function);
  return success();
}

bool is_pair_array_sorted(value_t value) {
  CHECK_FAMILY(ofArray, value);
  CHECK_EQ("not pair array", 0, get_array_length(value) & 1);
  size_t length = get_pair_array_length(value);
  for (size_t i = 1; i < length; i++) {
    value_t a = get_pair_array_first_at(value, i - 1);
    value_t b = get_pair_array_first_at(value, i);
    if (test_relation(value_ordering_compare(a, b), reGreaterThan))
      return false;
  }
  return true;
}

value_t binary_search_pair_array(value_t self, value_t key) {
  CHECK_FAMILY(ofArray, self);
  CHECK_EQ("not pair array", 0, get_array_length(self) & 1);
  // Using signed values allows the max to go negative which simplifies the
  // logic. It does in principle narrow the range we can represent but the
  // longest possible pair array is half the length of the longest possible
  // array, which gives us the sign bit WLOG.
  int64_t min = 0;
  int64_t max = ((int64_t) get_pair_array_length(self)) - 1;
  while (min <= max) {
    int64_t mid = ((max - min) >> 1) + min;
    value_t current_key = get_pair_array_first_at(self, mid);
    TRY_DEF(ordering, value_ordering_compare(current_key, key));
    if (test_relation(ordering, reLessThan)) {
      // The current key is smaller than the key we're looking for. Advance the
      // min past it.
      min = mid + 1;
    } else if (test_relation(ordering, reGreaterThan)) {
      // The current key is larger than the key we're looking for. Move the max
      // below it.
      max = mid - 1;
    } else {
      return get_pair_array_second_at(self, mid);
    }
  }
  return new_not_found_condition();
}

value_t array_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  size_t length = get_array_length(value);
  hash_stream_write_int64(stream, length);
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, value));
  for (size_t i = 0; i < length; i++) {
    value_t elm = get_array_at(value, i);
    TRY(value_transient_identity_hash_cycle_protect(elm, stream, &inner));
  }
  return success();
}

value_t array_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  size_t length = get_array_length(a);
  size_t b_length = get_array_length(b);
  if (length != b_length)
    return no();
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  for (size_t i = 0; i < length; i++) {
    value_t a_elm = get_array_at(a, i);
    value_t b_elm = get_array_at(b, i);
    TRY_DEF(cmp, value_identity_compare_cycle_protect(a_elm, b_elm, &inner));
    if (!get_boolean_value(cmp))
      return cmp;
  }
  return yes();
}

static value_t array_length(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofArray, self);
  return new_integer(get_array_length(self));
}

static value_t array_get_at(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t index_value = get_builtin_argument(args, 0);
  size_t index = get_integer_value(index_value);
  if (index >= get_array_length(self)) {
    ESCAPE_BUILTIN(args, out_of_bounds, index_value);
  } else {
    return get_array_at(self, index);
  }
}

static value_t array_set_at(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t index_value = get_builtin_argument(args, 0);
  size_t index = get_integer_value(index_value);
  if (index >= get_array_length(self)) {
    ESCAPE_BUILTIN(args, out_of_bounds, index_value);
  } else if (!is_mutable(self)) {
    ESCAPE_BUILTIN(args, is_frozen, self);
  } else {
    value_t value = get_builtin_argument(args, 1);
    set_array_at(self, index, value);
    return value;
  }
}

value_t add_array_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("array[]", 1, 1, array_get_at);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("array[]:=()", 2, 1, array_set_at);
  ADD_BUILTIN_IMPL("array.length", 0, array_length);
  return success();
}


// --- T u p l e ---

value_t get_tuple_at(value_t self, size_t index) {
  return get_array_at(self, index);
}


// --- A r r a y   b u f f e r ---

GET_FAMILY_PRIMARY_TYPE_IMPL(array_buffer);
NO_BUILTIN_METHODS(array_buffer);

ACCESSORS_IMPL(ArrayBuffer, array_buffer, acInFamily, ofArray, Elements, elements);
INTEGER_ACCESSORS_IMPL(ArrayBuffer, array_buffer, Length, length);

value_t array_buffer_validate(value_t self) {
  VALIDATE_FAMILY(ofArrayBuffer, self);
  VALIDATE_FAMILY(ofArray, get_array_buffer_elements(self));
  return success();
}

value_t ensure_array_buffer_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_array_buffer_elements(self)));
  return success();
}

bool try_add_to_array_buffer(value_t self, value_t value) {
  CHECK_FAMILY(ofArrayBuffer, self);
  CHECK_MUTABLE(self);
  value_t elements = get_array_buffer_elements(self);
  size_t capacity = get_array_length(elements);
  size_t index = get_array_buffer_length(self);
  if (index >= capacity)
    return false;
  set_array_at(elements, index, value);
  set_array_buffer_length(self, index + 1);
  return true;
}

static value_t extend_array_buffer(runtime_t *runtime, value_t buffer) {
  value_t old_elements = get_array_buffer_elements(buffer);
  size_t old_capacity = get_array_length(old_elements);
  size_t new_capacity = (old_capacity + 1) * 2;
  TRY_DEF(new_elements, new_heap_array(runtime, new_capacity));
  for (size_t i = 0; i < old_capacity; i++)
    set_array_at(new_elements, i, get_array_at(old_elements, i));
  set_array_buffer_elements(buffer, new_elements);
  return success();
}

value_t add_to_array_buffer(runtime_t *runtime, value_t buffer, value_t value) {
  CHECK_FAMILY(ofArrayBuffer, buffer);
  if (!try_add_to_array_buffer(buffer, value)) {
    TRY(extend_array_buffer(runtime, buffer));
    bool second_try = try_add_to_array_buffer(buffer, value);
    CHECK_TRUE("second array try", second_try);
  }
  return success();
}

bool try_add_to_pair_array_buffer(value_t self, value_t first, value_t second) {
  CHECK_FAMILY(ofArrayBuffer, self);
  CHECK_MUTABLE(self);
  value_t elements = get_array_buffer_elements(self);
  size_t capacity = get_array_length(elements);
  size_t index = get_array_buffer_length(self);
  if ((index + 1) >= capacity)
    return false;
  set_array_at(elements, index, first);
  set_array_at(elements, index + 1, second);
  set_array_buffer_length(self, index + 2);
  return true;
}

value_t add_to_pair_array_buffer(runtime_t *runtime, value_t buffer, value_t first,
    value_t second) {
  CHECK_FAMILY(ofArrayBuffer, buffer);
  if (!try_add_to_pair_array_buffer(buffer, first, second)) {
    TRY(extend_array_buffer(runtime, buffer));
    bool second_try = try_add_to_pair_array_buffer(buffer, first, second);
    CHECK_TRUE("second array try", second_try);
  }
  return success();
}

bool in_array_buffer(value_t self, value_t value) {
  for (size_t i = 0; i < get_array_buffer_length(self); i++) {
    if (value_identity_compare(value, get_array_buffer_at(self, i)))
      return true;
  }
  return false;
}

value_t ensure_array_buffer_contains(runtime_t *runtime, value_t self,
    value_t value) {
  if (in_array_buffer(self, value))
    return success();
  return add_to_array_buffer(runtime, self, value);
}

void sort_array_buffer(value_t self) {
  value_t elements = get_array_buffer_elements(self);
  sort_array_partial(elements, get_array_buffer_length(self));
}

value_t get_array_buffer_at(value_t self, size_t index) {
  CHECK_FAMILY(ofArrayBuffer, self);
  COND_CHECK_TRUE("array buffer index out of bounds", ccOutOfBounds,
      index < get_array_buffer_length(self));
  return get_array_at(get_array_buffer_elements(self), index);
}

void set_array_buffer_at(value_t self, size_t index, value_t value) {
  CHECK_FAMILY(ofArrayBuffer, self);
  CHECK_MUTABLE(self);
  CHECK_TRUE("array buffer index out of bounds",
      index < get_array_buffer_length(self));
  set_array_at(get_array_buffer_elements(self), index, value);
}

void array_buffer_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "%[");
  for (size_t i = 0; i < get_array_buffer_length(value); i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_print_inner_on(get_array_buffer_at(value, i), context, -1);
  }
  string_buffer_printf(context->buf, "]");
}

value_t get_pair_array_buffer_first_at(value_t self, size_t index) {
  return get_array_buffer_at(self, index << 1);
}

value_t get_pair_array_buffer_second_at(value_t self, size_t index) {
  return get_array_buffer_at(self, (index << 1) + 1);
}

size_t get_pair_array_buffer_length(value_t self) {
  return get_array_buffer_length(self) >> 1;
}



// --- I d e n t i t y   h a s h   m a p ---

GET_FAMILY_PRIMARY_TYPE_IMPL(id_hash_map);
NO_BUILTIN_METHODS(id_hash_map);

ACCESSORS_IMPL(IdHashMap, id_hash_map, acInFamily, ofArray, EntryArray, entry_array);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Size, size);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Capacity, capacity);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, OccupiedCount, occupied_count);

// Returns a pointer to the start of the index'th entry in the given map.
static value_t *get_id_hash_map_entry(value_t map, size_t index) {
  CHECK_REL("map entry out of bounds", index, <, get_id_hash_map_capacity(map));
  value_t array = get_id_hash_map_entry_array(map);
  return get_array_elements(array) + (index * kIdHashMapEntryFieldCount);
}

// Returns true if the given map entry is not storing a binding.
static bool is_id_hash_map_entry_deleted(value_t *entry) {
  return is_nothing(entry[kIdHashMapEntryKeyOffset]);
}

// Returns true if the given map entry is not storing a binding. Note that a
// deleted entry is counted as being empty.
static bool is_id_hash_map_entry_empty(value_t *entry) {
  return !in_domain(vdInteger, entry[kIdHashMapEntryHashOffset]);
}

// Returns the hash value stored in this map entry, which must not be empty.
static size_t get_id_hash_map_entry_hash(value_t *entry) {
  CHECK_FALSE("empty id hash map entry", is_id_hash_map_entry_empty(entry));
  return get_integer_value(entry[kIdHashMapEntryHashOffset]);
}

// Returns the key stored in this hash map entry, which must not be empty.
static value_t get_id_hash_map_entry_key(value_t *entry) {
  CHECK_FALSE("empty id hash map entry", is_id_hash_map_entry_empty(entry));
  return entry[kIdHashMapEntryKeyOffset];
}

// Returns the value stored in this hash map entry, which must not be empty.
static value_t get_id_hash_map_entry_value(value_t *entry) {
  CHECK_FALSE("empty id hash map entry", is_id_hash_map_entry_empty(entry));
  return entry[kIdHashMapEntryValueOffset];
}

// Sets the full contents of a map entry.
static void set_id_hash_map_entry(value_t *entry, value_t key, size_t hash,
    value_t value) {
  entry[kIdHashMapEntryKeyOffset] = key;
  entry[kIdHashMapEntryHashOffset] = new_integer(hash);
  entry[kIdHashMapEntryValueOffset] = value;
}

// Sets the full contents of a map entry such that it can be recognized as
// deleted.
static void delete_id_hash_map_entry(runtime_t *runtime, value_t *entry) {
  entry[kIdHashMapEntryKeyOffset] = nothing();
  entry[kIdHashMapEntryHashOffset] = null();
  entry[kIdHashMapEntryValueOffset] = null();
}

// Identifies if and how a new hash map entry was allocated.
typedef enum {
  // A previously unoccupied entry (empty and not deleted) was claimed.
  cmUnoccupied,
  // A deleted entry was reused.
  cmDeleted,
  // No new entry was created.
  cmNotCreated
} id_hash_map_entry_create_mode_t;

// Finds the appropriate entry to store a mapping for the given key with the
// given hash. If there is already a binding for the key then this function
// stores the index in the index out parameter. If there isn't and a non-null
// create_mode parameter is passed then a free index is stored in the out
// parameter and the mode of creation is stored in create_mode. Otherwise false
// is returned.
static bool find_id_hash_map_entry(value_t map, value_t key, size_t hash,
    value_t **entry_out, id_hash_map_entry_create_mode_t *create_mode) {
  CHECK_FAMILY(ofIdHashMap, map);
  CHECK_TRUE("was_created not initialized", (create_mode == NULL) ||
      (*create_mode == cmNotCreated));
  size_t capacity = get_id_hash_map_capacity(map);
  CHECK_REL("map overfull", get_id_hash_map_size(map), <, capacity);
  size_t current_index = hash % capacity;
  // Loop around until we find the key or an empty entry. Since we know the
  // capacity is at least one greater than the size there must be at least
  // one empty entry so we know the loop will terminate.
  value_t *first_deleted_entry = NULL;
  while (true) {
    value_t *entry = get_id_hash_map_entry(map, current_index);
    if (is_id_hash_map_entry_empty(entry)) {
      if (is_id_hash_map_entry_deleted(entry)) {
        // If this was the first deleted entry we've seen we record it such that
        // we can use that to create the entry if we don't find a match.
        if (first_deleted_entry == NULL)
          first_deleted_entry = entry;
        // Then we just skip on.
        goto keep_going;
      } else if (create_mode == NULL) {
        // Report that we didn't find the entry.
        return false;
      } else {
        // Found an empty entry which the caller wants us to return.
        if (first_deleted_entry == NULL) {
          // We didn't see any deleted entries along the way so return the next
          // free one.
          *entry_out = entry;
          *create_mode = cmUnoccupied;
        } else {
          // We did se a deleted entry which we can now reuse.
          *entry_out = first_deleted_entry;
          *create_mode = cmDeleted;
        }
        return true;
      }
    }
    size_t entry_hash = get_id_hash_map_entry_hash(entry);
    if (entry_hash == hash) {
      value_t entry_key = get_id_hash_map_entry_key(entry);
      if (value_identity_compare(key, entry_key)) {
        // Found the key; just return it.
        *entry_out = entry;
        return true;
      }
    }
   keep_going:
    // Didn't find it here so try the next one.
    current_index = (current_index + 1) % capacity;
  }
  UNREACHABLE("id hash map entry impossible loop");
  return false;
}

value_t try_set_id_hash_map_at(value_t map, value_t key, value_t value,
    bool allow_frozen) {
  CHECK_FAMILY(ofIdHashMap, map);
  CHECK_TRUE("mutating frozen map", allow_frozen || is_mutable(map));
  size_t occupied_count = get_id_hash_map_occupied_count(map);
  size_t size = get_id_hash_map_size(map);
  size_t capacity = get_id_hash_map_capacity(map);
  bool is_full = (occupied_count == (capacity - 1));
  // Calculate the hash.
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  // Locate where the new entry goes in the entry array.
  value_t *entry = NULL;
  id_hash_map_entry_create_mode_t create_mode = cmNotCreated;
  if (!find_id_hash_map_entry(map, key, hash, &entry,
      is_full ? NULL : &create_mode)) {
    // The only way this can return false is if the map is full (since if
    // was_created was non-null we would have created a new entry) and the
    // key couldn't be found. Report this.
    return new_condition(ccMapFull);
  }
  set_id_hash_map_entry(entry, key, hash, value);
  // Only increment the size if we created a new entry.
  if (create_mode != cmNotCreated) {
    // A new mapping was created.
    set_id_hash_map_size(map, size + 1);
    if (create_mode == cmUnoccupied)
      // We claimed a new previously unoccupied entry so increment the occupied
      // count.
      set_id_hash_map_occupied_count(map, occupied_count + 1);
  }
  return success();
}

value_t get_id_hash_map_at(value_t map, value_t key) {
  CHECK_FAMILY(ofIdHashMap, map);
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  value_t *entry = NULL;
  if (find_id_hash_map_entry(map, key, hash, &entry, NULL)) {
    return get_id_hash_map_entry_value(entry);
  } else {
    return new_not_found_condition();
  }
}

value_t get_id_hash_map_at_with_default(value_t map, value_t key, value_t defawlt) {
  value_t result = get_id_hash_map_at(map, key);
  return in_condition_cause(ccNotFound, result) ? defawlt : result;
}

bool has_id_hash_map_at(value_t map, value_t key) {
  return !in_condition_cause(ccNotFound, get_id_hash_map_at(map, key));
}


value_t delete_id_hash_map_at(runtime_t *runtime, value_t map, value_t key) {
  CHECK_FAMILY(ofIdHashMap, map);
  CHECK_MUTABLE(map);
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  // Try to find the key in the map.
  size_t hash = get_integer_value(hash_value);
  value_t *entry = NULL;
  if (find_id_hash_map_entry(map, key, hash, &entry, NULL)) {
    // We found the key; mark its entry as deleted.
    delete_id_hash_map_entry(runtime, entry);
    // We decrement the map size but keep the occupied size the same because
    // a deleted entry counts as occupied.
    set_id_hash_map_size(map, get_id_hash_map_size(map) - 1);
    return success();
  } else {
    return new_not_found_condition();
  }
}

void fixup_id_hash_map_post_migrate(runtime_t *runtime, value_t new_heap_object,
    value_t old_object) {
  // In this fixup we rehash the migrated hash map since the hash values are
  // allowed to change during garbage collection. We do it by copying the
  // entries currently stored in the object into space that's left over by the
  // old one, clearing out the new entry array completely, and then re-adding
  // the entries one by one, reading them from the scratch storage. Dumb but it
  // works.

  // Get the raw entry array from the new map.
  value_t new_entry_array = get_id_hash_map_entry_array(new_heap_object);
  size_t entry_array_length = get_array_length(new_entry_array);
  value_t *new_entries = get_array_elements(new_entry_array);
  // Get the raw entry array from the old map. This requires going directly
  // through the object since the nice accessors do sanity checking and the
  // state of the object at this point is, well, not sane.
  value_t old_entry_array = *access_heap_object_field(old_object, kIdHashMapEntryArrayOffset);
  CHECK_DOMAIN(vdMovedObject, get_heap_object_header(old_entry_array));
  value_t *old_entries = get_array_elements_unchecked(old_entry_array);
  // Copy the contents of the new entry array into the old one and clear it as
  // we go so it's ready to have elements added back.
  for (size_t i = 0; i < entry_array_length; i++) {
    old_entries[i] = new_entries[i];
    new_entries[i] = null();
  }
  // Reset the map's fields. It is now empty.
  set_id_hash_map_size(new_heap_object, 0);
  set_id_hash_map_occupied_count(new_heap_object, 0);
  // Fake an iterator that scans over the old array.
  id_hash_map_iter_t iter;
  iter.entries = old_entries;
  iter.cursor = 0;
  iter.capacity = get_id_hash_map_capacity(new_heap_object);
  iter.current = NULL;
  // Then simple scan over the entries and add them one at a time. Since they
  // come from the map originally adding them again must succeed.
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    // We need to be able to add elements even if the map is frozen and it's
    // okay because at the end it will be in the same state it was in before
    // so it's not really mutating it.
    value_t added = try_set_id_hash_map_at(new_heap_object, key, value, true);
    CHECK_FALSE("rehash failed to set", is_condition(added));
  }
}

void id_hash_map_iter_init(id_hash_map_iter_t *iter, value_t map) {
  value_t entry_array = get_id_hash_map_entry_array(map);
  iter->entries = get_array_elements(entry_array);
  iter->cursor = 0;
  iter->capacity = get_id_hash_map_capacity(map);
  iter->current = NULL;
}

bool id_hash_map_iter_advance(id_hash_map_iter_t *iter) {
  // Test successive entries until we find a non-empty one.
  while (iter->cursor < iter->capacity) {
    value_t *entry = iter->entries + (iter->cursor * kIdHashMapEntryFieldCount);
    iter->cursor++;
    if (!is_id_hash_map_entry_empty(entry)) {
      // Found one, store it in current and return success. Since deleted
      // entries count as empty this automatically skips over those too.
      iter->current = entry;
      return true;
    }
  }
  // Didn't find one. Clear current and return failure.
  iter->current = NULL;
  return false;
}

void id_hash_map_iter_get_current(id_hash_map_iter_t *iter, value_t *key_out, value_t *value_out) {
  CHECK_TRUE("map iter overrun", iter->current != NULL);
  *key_out = get_id_hash_map_entry_key(iter->current);
  *value_out = get_id_hash_map_entry_value(iter->current);
}

value_t id_hash_map_validate(value_t value) {
  VALIDATE_FAMILY(ofIdHashMap, value);
  value_t entry_array = get_id_hash_map_entry_array(value);
  VALIDATE_FAMILY(ofArray, entry_array);
  size_t capacity = get_id_hash_map_capacity(value);
  VALIDATE(get_id_hash_map_size(value) < capacity);
  VALIDATE(get_array_length(entry_array) == (capacity * kIdHashMapEntryFieldCount));
  return success();
}

value_t ensure_id_hash_map_owned_values_frozen(runtime_t *runtime, value_t self) {
  return ensure_frozen(runtime, get_id_hash_map_entry_array(self));
}

void id_hash_map_print_on(value_t value, print_on_context_t *context) {
  if (context->depth == 1) {
    string_buffer_printf(context->buf, "#<map{%i}>", get_id_hash_map_size(value));
  } else {
    string_buffer_printf(context->buf, "{");
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, value);
    bool is_first = true;
    while (id_hash_map_iter_advance(&iter)) {
      if (is_first) {
        is_first = false;
      } else {
        string_buffer_printf(context->buf, ", ");
      }
      value_t key;
      value_t value;
      id_hash_map_iter_get_current(&iter, &key, &value);
      value_print_inner_on(key, context, -1);
      string_buffer_printf(context->buf, ": ");
      value_print_inner_on(value, context, -1);
    }
    string_buffer_printf(context->buf, "}");
  }
}


// --- K e y ---

GET_FAMILY_PRIMARY_TYPE_IMPL(key);
NO_BUILTIN_METHODS(key);

INTEGER_ACCESSORS_IMPL(Key, key, Id, id);
ACCESSORS_IMPL(Key, key, acNoCheck, 0, DisplayName, display_name);

value_t key_validate(value_t value) {
  VALIDATE_FAMILY(ofKey, value);
  return success();
}

value_t key_identity_compare(value_t a, value_t b, size_t depth) {
  size_t a_id = get_key_id(a);
  size_t b_id = get_key_id(b);
  return new_boolean(a_id == b_id);
}

value_t key_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofKey, a);
  CHECK_FAMILY(ofKey, b);
  return compare_signed_integers(get_key_id(a), get_key_id(b));
}

void key_print_on(value_t value, print_on_context_t *context) {
  value_t display_name = get_key_display_name(value);
  if (is_nothing(display_name))
    display_name = new_integer(get_key_id(value));
  string_buffer_printf(context->buf, "%%");
  print_on_context_t new_context = *context;
  new_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
  value_print_inner_on(display_name, &new_context, -1);
}


// --- I n s t a n c e ---

ACCESSORS_IMPL(Instance, instance, acInFamily, ofIdHashMap, Fields, fields);
NO_BUILTIN_METHODS(instance);

value_t get_instance_field(value_t value, value_t key) {
  value_t fields = get_instance_fields(value);
  return get_id_hash_map_at(fields, key);
}

value_t try_set_instance_field(value_t instance, value_t key, value_t value) {
  value_t fields = get_instance_fields(instance);
  return try_set_id_hash_map_at(fields, key, value, false);
}

value_t instance_validate(value_t value) {
  VALIDATE_FAMILY(ofInstance, value);
  VALIDATE_FAMILY(ofIdHashMap, get_instance_fields(value));
  return success();
}

void instance_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofInstance, value);
  string_buffer_printf(context->buf, "#<instance of ");
  value_print_inner_on(get_instance_primary_type_field(value), context, -1);
  string_buffer_printf(context->buf, ": ");
  value_print_inner_on(get_instance_fields(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_instance_contents(value_t instance, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(ccInvalidInput, ofIdHashMap, contents);
  set_instance_fields(instance, contents);
  return success();
}

value_t get_instance_primary_type(value_t self, runtime_t *runtime) {
  return get_instance_primary_type_field(self);
}

value_mode_t get_instance_mode(value_t self) {
  return vmMutable;
}

void set_instance_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  UNREACHABLE("setting instance mode not implemented");
}


// --- I n s t a n c e   m a n a g e r ---

ACCESSORS_IMPL(InstanceManager, instance_manager, acNoCheck, 0, DisplayName, display_name);
GET_FAMILY_PRIMARY_TYPE_IMPL(instance_manager);
FIXED_GET_MODE_IMPL(instance_manager, vmDeepFrozen);

value_t instance_manager_validate(value_t value) {
  VALIDATE_FAMILY(ofInstanceManager, value);
  return success();
}

void instance_manager_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofInstanceManager, value);
  string_buffer_printf(context->buf, "#<instance manager: ");
  value_print_inner_on(get_instance_manager_display_name(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

static value_t instance_manager_new_instance(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t type = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofType, type);
  TRY_DEF(species, new_heap_instance_species(runtime, type, self));
  return new_heap_instance(runtime, species);
}

value_t add_instance_manager_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("instance_manager.new_instance", 1, instance_manager_new_instance);
  return success();
}


// --- F a c t o r y ---

ACCESSORS_IMPL(Factory, factory, acInFamily, ofVoidP, Constructor, constructor);
FIXED_GET_MODE_IMPL(factory, vmDeepFrozen);

value_t factory_validate(value_t value) {
  VALIDATE_FAMILY(ofFactory, value);
  VALIDATE_FAMILY(ofVoidP, get_factory_constructor(value));
  return success();
}

void factory_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofFactory, value);
  string_buffer_printf(context->buf, "#<factory: ");
  value_print_inner_on(get_factory_constructor(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- C o d e   b l o c k ---

ACCESSORS_IMPL(CodeBlock, code_block, acInFamily, ofBlob, Bytecode, bytecode);
ACCESSORS_IMPL(CodeBlock, code_block, acInFamily, ofArray, ValuePool, value_pool);
INTEGER_ACCESSORS_IMPL(CodeBlock, code_block, HighWaterMark, high_water_mark);

value_t code_block_validate(value_t value) {
  VALIDATE_FAMILY(ofCodeBlock, value);
  VALIDATE_FAMILY(ofBlob, get_code_block_bytecode(value));
  VALIDATE_FAMILY(ofArray, get_code_block_value_pool(value));
  return success();
}

void code_block_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofCodeBlock, value);
  string_buffer_printf(context->buf, "#<code block: bc@%i, vp@%i>",
      get_blob_length(get_code_block_bytecode(value)),
      get_array_length(get_code_block_value_pool(value)));
}

value_t ensure_code_block_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_code_block_value_pool(self)));
  return success();
}


// --- T y p e ---

GET_FAMILY_PRIMARY_TYPE_IMPL(type);
NO_BUILTIN_METHODS(type);

ACCESSORS_IMPL(Type, type, acNoCheck, 0, RawOrigin, raw_origin);
ACCESSORS_IMPL(Type, type, acNoCheck, 0, DisplayName, display_name);

value_t type_validate(value_t value) {
  VALIDATE_FAMILY(ofType, value);
  return success();
}

void type_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofType, value);
  value_t display_name = get_type_display_name(value);
  string_buffer_printf(context->buf, "#<type");
  if (!is_nothing(display_name)) {
    string_buffer_printf(context->buf, " ");
    print_on_context_t new_context = *context;
    new_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
    value_print_inner_on(display_name, &new_context, -1);
  }
  string_buffer_printf(context->buf, ">");
}

value_t plankton_new_type(runtime_t *runtime) {
  return new_heap_type(runtime, afMutable, nothing(), nothing());
}

value_t plankton_set_type_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_type_display_name(object, name_value);
  return success();
}

value_t get_type_origin(value_t self, value_t ambience) {
  value_t raw_origin = get_type_raw_origin(self);
  if (in_phylum(tpAmbienceRedirect, raw_origin)) {
    return follow_ambience_redirect(ambience, raw_origin);
  } else {
    return raw_origin;
  }
}


// --- A r g u m e n t   m a p   t r i e ---

NO_BUILTIN_METHODS(argument_map_trie);
TRIVIAL_PRINT_ON_IMPL(ArgumentMapTrie, argument_map_trie);

ACCESSORS_IMPL(ArgumentMapTrie, argument_map_trie, acInFamily, ofArray, Value, value);
ACCESSORS_IMPL(ArgumentMapTrie, argument_map_trie, acInFamily, ofArrayBuffer,
    Children, children);

value_t argument_map_trie_validate(value_t value) {
  VALIDATE_FAMILY(ofArgumentMapTrie, value);
  VALIDATE_FAMILY(ofArray, get_argument_map_trie_value(value));
  VALIDATE_FAMILY(ofArrayBuffer, get_argument_map_trie_children(value));
  return success();
}

// Converts an argument map key object to a plain integer. Argument map keys
// can only be nonnegative integers or null.
static size_t argument_map_key_to_integer(value_t key) {
  // This is okay for now but later on if we want to optimize how arg map tries
  // are built this scheme may have to be replaced by something better.
  return is_null(key) ? 0 : (1 + get_integer_value(key));
}

value_t get_argument_map_trie_child(runtime_t *runtime, value_t self, value_t key) {
  CHECK_MUTABLE(self);
  size_t index = argument_map_key_to_integer(key);
  value_t children = get_argument_map_trie_children(self);
  // Check if we've already build that child.
  if (index < get_array_buffer_length(children)) {
    value_t cached = get_array_buffer_at(children, index);
    if (!is_null(cached))
      return cached;
  }
  // Pad the children buffer if necessary.
  for (size_t i = get_array_buffer_length(children); i <= index; i++)
    TRY(add_to_array_buffer(runtime, children, null()));
  // Create the new child.
  value_t old_value = get_argument_map_trie_value(self);
  size_t old_length = get_array_length(old_value);
  size_t new_length = old_length + 1;
  TRY_DEF(new_value, new_heap_array(runtime, new_length));
  for (size_t i = 0; i < old_length; i++)
    set_array_at(new_value, i, get_array_at(old_value, i));
  set_array_at(new_value, old_length, key);
  TRY(ensure_frozen(runtime, new_value));
  TRY_DEF(new_child, new_heap_argument_map_trie(runtime, new_value));
  set_array_buffer_at(children, index, new_child);
  return new_child;
}

value_t ensure_argument_map_trie_owned_values_frozen(runtime_t *runtime,
    value_t self) {
  TRY(ensure_frozen(runtime, get_argument_map_trie_children(self)));
  return success();
}


// --- N a m e s p a c e ---

ACCESSORS_IMPL(Namespace, namespace, acInFamilyOpt, ofIdHashMap, Bindings, bindings);
ACCESSORS_IMPL(Namespace, namespace, acNoCheck, 0, Value, value);

value_t namespace_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespace, self);
  VALIDATE_FAMILY(ofIdHashMap, get_namespace_bindings(self));
  return success();
}

value_t get_namespace_binding_at(value_t self, value_t path) {
  CHECK_FAMILY(ofNamespace, self);
  CHECK_FAMILY(ofPath, path);
  if (is_path_empty(path)) {
    // In the case where a multi-part path has been defined but not one of the
    // path's prefixes there will be a namespace node with a value present, but
    // the value will be nothing. Hence that case corresponds to the not found
    // case; if it were possible to just store the condition in the node that
    // would be easier but conditions can not be stored in the heap.
    value_t value = get_namespace_value(self);
    return is_nothing(value)
        ? new_not_found_condition()
        : value;
  }
  value_t head = get_path_head(path);
  value_t tail = get_path_tail(path);
  value_t bindings = get_namespace_bindings(self);
  value_t subspace = get_id_hash_map_at(bindings, head);
  if (in_condition_cause(ccNotFound, subspace))
    return subspace;
  return get_namespace_binding_at(subspace, tail);
}

value_t set_namespace_binding_at(runtime_t *runtime, value_t nspace,
    value_t path, value_t value) {
  CHECK_FAMILY(ofNamespace, nspace);
  CHECK_FAMILY(ofPath, path);
  if (is_path_empty(path)) {
    // If the path is empty we've arrived at the namespace node that should
    // hold the value.
    set_namespace_value(nspace, value);
    return success();
  } else {
    // This binding has to go through a subspace; acquire/create is as
    // appropriate.
    value_t head = get_path_head(path);
    value_t tail = get_path_tail(path);
    value_t bindings = get_namespace_bindings(nspace);
    value_t subspace = get_id_hash_map_at(bindings, head);
    if (in_condition_cause(ccNotFound, subspace)) {
      TRY_SET(subspace, new_heap_namespace(runtime, nothing()));
      TRY(set_id_hash_map_at(runtime, bindings, head, subspace));
    }
    return set_namespace_binding_at(runtime, subspace, tail, value);
  }
}

value_t ensure_namespace_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_namespace_bindings(self)));
  return success();
}

void namespace_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofNamespace, value);
  string_buffer_printf(context->buf, "#<namespace ");
  value_print_inner_on(get_namespace_bindings(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- M o d u l e---

FIXED_GET_MODE_IMPL(module, vmMutable);

ACCESSORS_IMPL(Module, module, acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(Module, module, acInFamily, ofArrayBuffer, Fragments, fragments);

value_t module_validate(value_t self) {
  VALIDATE_FAMILY(ofModule, self);
  VALIDATE_FAMILY_OPT(ofPath, get_module_path(self));
  VALIDATE_FAMILY(ofArrayBuffer, get_module_fragments(self));
  return success();
}

void module_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofModule, value);
  string_buffer_printf(context->buf, "#<module ");
  value_print_inner_on(get_module_path(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t get_module_fragment_at(value_t self, value_t stage) {
  value_t fragments = get_module_fragments(self);
  for (size_t i = 0; i < get_array_buffer_length(fragments); i++) {
    value_t fragment = get_array_buffer_at(fragments, i);
    if (is_same_value(get_module_fragment_stage(fragment), stage))
      return fragment;
  }
  return new_not_found_condition();
}

value_t get_or_create_module_fragment_at(runtime_t *runtime, value_t self,
    value_t stage, bool *created_out) {
  value_t existing = get_module_fragment_at(self, stage);
  bool created = in_condition_cause(ccNotFound, existing);
  if (created_out != NULL)
    *created_out = created;
  if (!created)
    return existing;
  // No fragment has been created yet. Create one but leave it uninitialized
  // since we don't have the context do that properly here.
  TRY_DEF(new_fragment, new_heap_module_fragment(runtime, stage,
      get_module_path(self), nothing(), nothing(), nothing(), nothing()));
  set_module_fragment_epoch(new_fragment, feUninitialized);
  TRY(add_to_array_buffer(runtime, get_module_fragments(self), new_fragment));
  return new_fragment;
}

value_t add_module_fragment(runtime_t *runtime, value_t self, value_t fragment) {
  value_t fragments = get_module_fragments(self);
  CHECK_TRUE("fragment already exists", in_condition_cause(ccNotFound,
      get_module_fragment_at(self, get_module_fragment_stage(fragment))));
  return add_to_array_buffer(runtime, fragments, fragment);
}

value_t get_module_fragment_before(value_t self, value_t stage) {
  int32_t limit = get_stage_offset_value(stage);
  value_t fragments = get_module_fragments(self);
  int32_t best_stage = kMostNegativeInt32;
  value_t best_fragment = new_not_found_condition();
  for (size_t i = 0; i < get_array_buffer_length(fragments); i++) {
    value_t fragment = get_array_buffer_at(fragments, i);
    int32_t fragment_stage = get_stage_offset_value(get_module_fragment_stage(fragment));
    if (fragment_stage < limit && fragment_stage > best_stage) {
      best_stage = fragment_stage;
      best_fragment = fragment;
    }
  }
  return best_fragment;
}

static value_t module_fragment_lookup_path_local(runtime_t *runtime, value_t self,
    value_t path);

// Look up a path in the import part of a fragment. If an import is found the
// lookup of the path's tail will continue through the imported module.
static value_t module_fragment_lookup_path_in_imports(runtime_t *runtime,
    value_t self, value_t path) {
  value_t head = get_path_head(path);
  value_t importspace = get_module_fragment_imports(self);
  value_t fragment = get_id_hash_map_at(importspace, head);
  if (in_condition_cause(ccNotFound, fragment))
    return fragment;
  // We found a binding for the head in the imports. However, we don't continue
  // the lookup directly in the fragment since we also want to be able to find
  // bindings in the fragment's predecessors. Instead we only use the fragment
  // to identify the start stage and then look up in the module -- which will
  // start the lookup in the fragment but if it doesn't find the result continue
  // backwards through the stages.
  value_t tail = get_path_tail(path);
  return module_fragment_lookup_path_full(runtime, fragment, tail);
}

// Look up a path in the namespace part of a fragment.
static value_t module_fragment_lookup_path_in_namespace(runtime_t *runtime,
    value_t self, value_t path) {
  value_t nspace = get_module_fragment_namespace(self);
  return get_namespace_binding_at(nspace, path);
}

// Look up a path locally in the given fragment. You generally don't want to
// call this directly unless you explicitly continue backwards through the
// predecessor fragments if this lookup doesn't find what you're looking for.
static value_t module_fragment_lookup_path_local(runtime_t *runtime, value_t self,
    value_t path) {
  CHECK_FAMILY(ofPath, path);
  CHECK_FALSE("looking up empty path", is_path_empty(path));
  // First check the imports.
  value_t as_import = module_fragment_lookup_path_in_imports(runtime, self,
      path);
  if (!in_condition_cause(ccNotFound, as_import))
    return as_import;
  // If not an import try looking up in the appropriate namespace.
  return module_fragment_lookup_path_in_namespace(runtime, self, path);
}

value_t module_fragment_lookup_path_full(runtime_t *runtime, value_t self, value_t path) {
  // The given fragment may be nothing but that only gives a meaningful result
  // if the path is :ctrino.
  CHECK_FAMILY_OPT(ofModuleFragment, self);
  CHECK_FAMILY(ofPath, path);
  value_t head = get_path_head(path);
  if (value_identity_compare(head, RSTR(runtime, ctrino)))
    return ROOT(runtime, ctrino);
  // Loop backwards through the fragments, starting from the specified stage.
  value_t fragment = self;
  while (!is_nothing(fragment)) {
    value_t binding = module_fragment_lookup_path_local(runtime, fragment, path);
    if (!in_condition_cause(ccNotFound, binding))
      return binding;
    fragment = get_module_fragment_predecessor(fragment);
  }
  WARN("Namespace lookup of %v failed", path);
  return new_lookup_error_condition(lcNamespace);
}

value_t ensure_module_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_module_fragments(self)));
  return success();
}


// --- M o d u l e   f r a g m e n t ---

ACCESSORS_IMPL(ModuleFragment, module_fragment, acInPhylumOpt, tpStageOffset,
    Stage, stage);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofPath, Path,
    path);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofModuleFragment,
    Predecessor, predecessor);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofNamespace,
    Namespace, namespace);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofMethodspace,
    Methodspace, methodspace);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofIdHashMap,
    Imports, imports);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamily, ofModuleFragmentPrivate,
    Private, private);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acNoCheck, 0, MethodspacesCache,
    methodspaces_cache);

void set_module_fragment_epoch(value_t self, module_fragment_epoch_t value) {
  *access_heap_object_field(self, kModuleFragmentEpochOffset) = new_integer(value);
}

module_fragment_epoch_t get_module_fragment_epoch(value_t self) {
  value_t epoch = *access_heap_object_field(self, kModuleFragmentEpochOffset);
  return (module_fragment_epoch_t) get_integer_value(epoch);
}

value_t module_fragment_validate(value_t value) {
  VALIDATE_FAMILY(ofModuleFragment, value);
  VALIDATE_FAMILY_OPT(ofNamespace, get_module_fragment_namespace(value));
  VALIDATE_FAMILY_OPT(ofMethodspace, get_module_fragment_methodspace(value));
  VALIDATE_FAMILY(ofModuleFragmentPrivate, get_module_fragment_private(value));
  return success();
}

void module_fragment_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofModuleFragment, value);
  string_buffer_printf(context->buf, "#<fragment ");
  value_print_inner_on(get_module_fragment_path(value), context, -1);
  string_buffer_printf(context->buf, " ");
  value_t stage = get_module_fragment_stage(value);
  value_print_inner_on(stage, context, -1);
  string_buffer_printf(context->buf, ">");
}

bool is_module_fragment_bound(value_t fragment) {
  return get_module_fragment_epoch(fragment) == feComplete;
}

value_t get_module_fragment_predecessor_at(value_t self, value_t stage) {
  value_t current = self;
  while (!is_nothing(current) && !is_same_value(get_module_fragment_stage(current), stage))
    current = get_module_fragment_predecessor(current);
  return current;
}

value_t ensure_module_fragment_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_module_fragment_private(self)));
  TRY(ensure_frozen(runtime, get_module_fragment_methodspaces_cache(self)));
  return success();
}


// --- M o d u l e   f r a g m e n t   a c c e s s ---

GET_FAMILY_PRIMARY_TYPE_IMPL(module_fragment_private);
FIXED_GET_MODE_IMPL(module_fragment_private, vmMutable);

ACCESSORS_IMPL(ModuleFragmentPrivate, module_fragment_private, acInFamilyOpt,
    ofModuleFragment, Owner, owner);
ACCESSORS_IMPL(ModuleFragmentPrivate, module_fragment_private, acInFamilyOpt,
    ofModuleFragment, Successor, successor);

value_t module_fragment_private_validate(value_t value) {
  VALIDATE_FAMILY(ofModuleFragmentPrivate, value);
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_module_fragment_private_owner(value));
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_module_fragment_private_successor(value));
  return success();
}

void module_fragment_private_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofModuleFragmentPrivate, value);
  string_buffer_printf(context->buf, "#<privileged access to ");
  value_t fragment = get_module_fragment_private_owner(value);
  value_t stage = get_module_fragment_stage(fragment);
  value_print_inner_on(stage, context, -1);
  value_t path = get_module_fragment_path(fragment);
  value_print_inner_on(path, context, -1);
  string_buffer_printf(context->buf, ">");
}

static value_t module_fragment_private_new_type(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofModuleFragmentPrivate, self);
  // The type is bound in the namespace of the fragment .new_type is called on
  // but its origin is the next module since that's where the methods for the
  // type are defined there.
  runtime_t *runtime = get_builtin_runtime(args);
  value_t next_fragment = get_module_fragment_private_successor(self);
  return new_heap_type(runtime, afMutable, next_fragment, display_name);
}

static value_t module_fragment_private_new_global_field(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofModuleFragmentPrivate, self);
  return new_heap_global_field(get_builtin_runtime(args), display_name);
}

value_t emit_module_fragment_private_invoke(assembler_t *assm) {
  TRY(assembler_emit_module_fragment_private_invoke(assm));
  return success();
}

value_t add_module_fragment_private_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL("module_fragment_private.new_type", 1,
      module_fragment_private_new_type);
  ADD_BUILTIN_IMPL("module_fragment_private.new_global_field", 1,
      module_fragment_private_new_global_field);
  TRY(add_custom_method_impl(runtime, deref(s_map), "module_fragment_private.invoke",
      1, new_flag_set(kFlagSetAllOff), emit_module_fragment_private_invoke));
  return success();
}


// --- P a t h ---

GET_FAMILY_PRIMARY_TYPE_IMPL(path);
NO_BUILTIN_METHODS(path);

ACCESSORS_IMPL(Path, path, acNoCheck, 0, RawHead, raw_head);
ACCESSORS_IMPL(Path, path, acInFamilyOpt, ofPath, RawTail, raw_tail);

value_t path_validate(value_t self) {
  VALIDATE_FAMILY(ofPath, self);
  VALIDATE_FAMILY_OPT(ofPath, get_path_raw_tail(self));
  return success();
}

bool is_path_empty(value_t value) {
  CHECK_FAMILY(ofPath, value);
  return is_nothing(get_path_raw_head(value));
}

value_t plankton_new_path(runtime_t *runtime) {
  return new_heap_path(runtime, afMutable, nothing(),
      nothing());
}

value_t plankton_set_path_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, names);
  if (get_array_length(names_value) == 0) {
    CHECK_TRUE("new path was non-empty", is_path_empty(object));
  } else {
    value_t head = get_array_at(names_value, 0);
    TRY_DEF(tail, new_heap_path_with_names(runtime, names_value, 1));
    set_path_raw_head(object, head);
    set_path_raw_tail(object, tail);
  }
  return success();
}

value_t get_path_head(value_t path) {
  value_t raw_head = get_path_raw_head(path);
  COND_CHECK_TRUE("head of empty path", ccEmptyPath, !is_nothing(raw_head));
  return raw_head;
}

value_t get_path_tail(value_t path) {
  value_t raw_tail = get_path_raw_tail(path);
  COND_CHECK_TRUE("tail of empty path", ccEmptyPath, !is_nothing(raw_tail));
  return raw_tail;
}

void path_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofPath, value);
  if (is_path_empty(value)) {
    string_buffer_printf(context->buf, "#<empty path>");
  } else {
    value_t current = value;
    while (!is_path_empty(current)) {
      string_buffer_printf(context->buf, ":");
      print_on_context_t new_context = *context;
      new_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
      value_print_inner_on(get_path_head(current), &new_context, -1);
      current = get_path_tail(current);
    }
  }
}

value_t path_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  cycle_detector_t inner;
  cycle_detector_enter(outer, &inner, value);
  // TODO: This should really be done iteratively since this gives a huge
  //   pointless overhead per segment. Alternatively it makes sense to
  //   canonicalize paths and then the address works as a hash.
  value_t head = get_path_raw_head(value);
  value_t tail = get_path_raw_tail(value);
  TRY(value_transient_identity_hash_cycle_protect(head, stream, &inner));
  TRY(value_transient_identity_hash_cycle_protect(tail, stream, &inner));
  return success();
}

value_t path_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  value_t a_head = get_path_raw_head(a);
  value_t b_head = get_path_raw_head(b);
  TRY_DEF(cmp_head, value_identity_compare_cycle_protect(a_head, b_head, &inner));
  if (!get_boolean_value(cmp_head))
    return cmp_head;
  // TODO: As above, this should be done iteratively.
  value_t a_tail = get_path_raw_tail(a);
  value_t b_tail = get_path_raw_tail(b);
  return value_identity_compare_cycle_protect(a_tail, b_tail, &inner);
}

value_t path_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofPath, a);
  CHECK_FAMILY(ofPath, b);
  value_t a_head = get_path_raw_head(a);
  value_t b_head = get_path_raw_head(b);
  value_t cmp_head = value_ordering_compare(a_head, b_head);
  if (!test_relation(cmp_head, reEqual))
    return cmp_head;
  value_t a_tail = get_path_raw_tail(a);
  value_t b_tail = get_path_raw_tail(b);
  return value_ordering_compare(a_tail, b_tail);
}


// --- I d e n t i f i e r ---

FIXED_GET_MODE_IMPL(identifier, vmMutable);

ACCESSORS_IMPL(Identifier, identifier, acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(Identifier, identifier, acNoCheck, 0, Stage, stage);

value_t identifier_validate(value_t self) {
  VALIDATE_FAMILY(ofIdentifier, self);
  VALIDATE_FAMILY_OPT(ofPath, get_identifier_path(self));
  return success();
}

value_t plankton_new_identifier(runtime_t *runtime) {
  return new_heap_identifier(runtime, nothing(), nothing());
}

value_t plankton_set_identifier_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, stage, path);
  set_identifier_stage(object, new_stage_offset(get_integer_value(stage_value)));
  set_identifier_path(object, path_value);
  return success();
}

void identifier_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofIdentifier, value);
  value_print_inner_on(get_identifier_stage(value), context, -1);
  value_print_inner_on(get_identifier_path(value), context, -1);
}

value_t identifier_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  cycle_detector_t inner;
  cycle_detector_enter(outer, &inner, value);
  value_t stage = get_identifier_stage(value);
  value_t path = get_identifier_path(value);
  TRY(value_transient_identity_hash_cycle_protect(stage, stream, &inner));
  TRY(value_transient_identity_hash_cycle_protect(path, stream, &inner));
  return success();
}

value_t identifier_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  value_t a_stage = get_identifier_stage(a);
  value_t b_stage = get_identifier_stage(b);
  TRY_DEF(cmp_head, value_identity_compare_cycle_protect(a_stage, b_stage, &inner));
  if (!get_boolean_value(cmp_head))
    return cmp_head;
  value_t a_path = get_identifier_path(a);
  value_t b_path = get_identifier_path(b);
  return value_identity_compare_cycle_protect(a_path, b_path, &inner);
}

value_t identifier_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofIdentifier, a);
  CHECK_FAMILY(ofIdentifier, b);
  value_t a_stage = get_identifier_stage(a);
  value_t b_stage = get_identifier_stage(b);
  value_t cmp_stage = value_ordering_compare(a_stage, b_stage);
  if (!test_relation(cmp_stage, reEqual))
    return cmp_stage;
  value_t a_path = get_identifier_path(a);
  value_t b_path = get_identifier_path(b);
  return value_ordering_compare(a_path, b_path);
}


// --- F u n c t i o n ---

GET_FAMILY_PRIMARY_TYPE_IMPL(function);
NO_BUILTIN_METHODS(function);

ACCESSORS_IMPL(Function, function, acNoCheck, 0, DisplayName, display_name);

value_t function_validate(value_t self) {
  VALIDATE_FAMILY(ofFunction, self);
  return success();
}

void function_print_on(value_t value, print_on_context_t *context) {
  value_t display_name = get_function_display_name(value);
  if (!is_nothing(display_name)) {
    print_on_context_t new_context = *context;
    new_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
    value_print_inner_on(display_name, &new_context, -1);
  }
}


// --- U n k n o w n ---

FIXED_GET_MODE_IMPL(unknown, vmMutable);

ACCESSORS_IMPL(Unknown, unknown, acNoCheck, 0, Header, header);
ACCESSORS_IMPL(Unknown, unknown, acNoCheck, 0, Payload, payload);

value_t unknown_validate(value_t self) {
  VALIDATE_FAMILY(ofUnknown, self);
  return success();
}

value_t plankton_set_unknown_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  set_unknown_payload(object, contents);
  return success();
}

void unknown_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<? ");
  value_t header = get_unknown_header(value);
  value_print_inner_on(header, context, -1);
  string_buffer_printf(context->buf, " ");
  value_t payload = get_unknown_payload(value);
  value_print_inner_on(payload, context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- O p t i o n s ---

FIXED_GET_MODE_IMPL(options, vmMutable);

ACCESSORS_IMPL(Options, options, acInFamilyOpt, ofArray, Elements, elements);

value_t plankton_new_options(runtime_t *runtime) {
  return new_heap_options(runtime, nothing());
}

value_t plankton_set_options_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, elements);
  set_options_elements(object, elements_value);
  return success();
}

value_t options_validate(value_t self) {
  VALIDATE_FAMILY(ofOptions, self);
  return success();
}

void options_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<options ");
  value_t elements = get_options_elements(value);
  value_print_inner_on(elements, context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t get_options_flag_value(runtime_t *runtime, value_t self, value_t key,
    value_t defawlt) {
  // Yeah so the clean way to do this would have been to create families for
  // the different kinds of elements and let the deserialization code sort all
  // this out. But we can get away with this somewhat hacky by simple and
  // localized approach.
  value_t elements = get_options_elements(self);
  for (size_t i = 0; i < get_array_length(elements); i++) {
    value_t element = get_array_at(elements, i);
    if (!in_family(ofUnknown, element))
      continue;
    value_t header = get_unknown_header(element);
    if (!in_family(ofUnknown, header))
      continue;
    value_t header_payload = get_unknown_payload(header);
    if (!in_family(ofArray, header_payload))
      continue;
    value_t header_payload_last = get_array_at(header_payload,
        get_array_length(header_payload) - 1);
    if (!value_identity_compare(header_payload_last, RSTR(runtime, FlagElement)))
      continue;
    value_t payload = get_unknown_payload(element);
    if (!in_family(ofIdHashMap, payload))
      continue;
    value_t element_key = get_id_hash_map_at(payload, RSTR(runtime, key));
    if (in_condition_cause(ccNotFound, element_key) || !value_identity_compare(element_key, key))
      continue;
    value_t element_value = get_id_hash_map_at(payload, RSTR(runtime, value));
    if (!in_condition_cause(ccNotFound, element_value))
      return element_value;
  }
  return defawlt;
}


// --- D e c i m a l   f r a c t i o n ---

FIXED_GET_MODE_IMPL(decimal_fraction, vmFrozen);

ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Numerator, numerator);
ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Denominator, denominator);
ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Precision, precision);

value_t decimal_fraction_validate(value_t self) {
  VALIDATE_FAMILY(ofDecimalFraction, self);
  return success();
}

void decimal_fraction_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<");
  value_t numerator = get_decimal_fraction_numerator(value);
  value_print_inner_on(numerator, context, -1);
  string_buffer_printf(context->buf, "/10^");
  value_t denominator = get_decimal_fraction_denominator(value);
  value_print_inner_on(denominator, context, -1);
  string_buffer_printf(context->buf, "@10^-");
  value_t precision = get_decimal_fraction_precision(value);
  value_print_inner_on(precision, context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_decimal_fraction_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, numerator, denominator, precision);
  set_decimal_fraction_numerator(object, numerator_value);
  set_decimal_fraction_denominator(object, denominator_value);
  set_decimal_fraction_precision(object, precision_value);
  return success();
}

value_t plankton_new_decimal_fraction(runtime_t *runtime) {
  return new_heap_decimal_fraction(runtime, nothing(), nothing(), nothing());
}


// --- G l o b a l   f i e l d ---

GET_FAMILY_PRIMARY_TYPE_IMPL(global_field);
FIXED_GET_MODE_IMPL(global_field, vmFrozen);

ACCESSORS_IMPL(GlobalField, global_field, acNoCheck, 0, DisplayName, display_name);

value_t global_field_validate(value_t self) {
  VALIDATE_FAMILY(ofGlobalField, self);
  return success();
}

void global_field_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, ".$");
  value_t display_name = get_global_field_display_name(value);
  value_print_inner_on(display_name, context, -1);
  string_buffer_printf(context->buf, "");
}

static value_t global_field_set(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofGlobalField, self);
  value_t instance = get_builtin_argument(args, 0);
  value_t value = get_builtin_argument(args, 1);
  runtime_t *runtime = get_builtin_runtime(args);
  return set_instance_field(runtime, instance, self, value);
}

static value_t global_field_get(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofGlobalField, self);
  value_t instance = get_builtin_argument(args, 0);
  value_t value = get_instance_field(instance, self);
  if (in_condition_cause(ccNotFound, value)) {
    ESCAPE_BUILTIN(args, no_such_field, self, instance);
  } else {
    return value;
  }
}

value_t add_global_field_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("global_field[]", 1, 2, global_field_get);
  ADD_BUILTIN_IMPL("global_field[]:=()", 2, global_field_set);
  return success();
}



// --- R e f e r e n c e ---

FIXED_GET_MODE_IMPL(reference, vmMutable);

ACCESSORS_IMPL(Reference, reference, acNoCheck, 0, Value, value);

value_t reference_validate(value_t self) {
  VALIDATE_FAMILY(ofReference, self);
  return success();
}

void reference_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "&");
  value_t value = get_reference_value(self);
  value_print_inner_on(value, context, -1);
}


// --- A m b i e n c e ---

TRIVIAL_PRINT_ON_IMPL(Ambience, ambience);
FIXED_GET_MODE_IMPL(ambience, vmMutable);

ACCESSORS_IMPL(Ambience, ambience, acInFamilyOpt, ofModuleFragment,
    PresentCoreFragment, present_core_fragment);

value_t ambience_validate(value_t self) {
  VALIDATE_FAMILY(ofAmbience, self);
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_ambience_present_core_fragment(self));
  return success();
}

void get_ambience_layout(value_t value, heap_object_layout_t *layout) {
  // An ambience has one non-value field.
  heap_object_layout_set(layout, kAmbienceSize, HEAP_OBJECT_FIELD_OFFSET(1));
}

void set_ambience_runtime(value_t self, runtime_t *runtime) {
  CHECK_FAMILY(ofAmbience, self);
  *access_heap_object_field(self, kAmbienceRuntimeOffset) = pointer_to_value_bit_cast(runtime);
}

runtime_t *get_ambience_runtime(value_t self) {
  CHECK_FAMILY(ofAmbience, self);
  return (runtime_t*) value_to_pointer_bit_cast(
      *access_heap_object_field(self, kAmbienceRuntimeOffset));
}

value_t follow_ambience_redirect(value_t self, value_t redirect) {
  CHECK_FAMILY(ofAmbience, self);
  CHECK_PHYLUM(tpAmbienceRedirect, redirect);
  size_t offset = get_ambience_redirect_offset(redirect);
  CHECK_REL("invalid redirect", offset, <, kAmbienceSize);
  return *access_heap_object_field(self, offset);
}


// --- M i s c ---

// Adds a binding to the given plankton environment map.
static value_t add_plankton_binding(value_t map, value_t category, const char *name,
    value_t value, runtime_t *runtime) {
  string_t key_str;
  string_init(&key_str, name);
  // Build the key, [category, name].
  TRY_DEF(name_obj, new_heap_string(runtime, &key_str));
  TRY_DEF(key_obj, new_heap_pair(runtime, category, name_obj));
  // Add the mapping to the environment map.
  TRY(set_id_hash_map_at(runtime, map, key_obj, value));
  return success();
}

value_t add_plankton_factory(value_t map, value_t category, const char *name,
    factory_constructor_t constructor, runtime_t *runtime) {
  TRY_DEF(factory, new_heap_factory(runtime, constructor));
  return add_plankton_binding(map, category, name, factory, runtime);
}

value_t init_plankton_core_factories(value_t map, runtime_t *runtime) {
  value_t core = RSTR(runtime, core);
  // Factories
  TRY(add_plankton_factory(map, core, "DecimalFraction", plankton_new_decimal_fraction, runtime));
  TRY(add_plankton_factory(map, core, "Identifier", plankton_new_identifier, runtime));
  TRY(add_plankton_factory(map, core, "Library", plankton_new_library, runtime));
  TRY(add_plankton_factory(map, core, "Operation", plankton_new_operation, runtime));
  TRY(add_plankton_factory(map, core, "Path", plankton_new_path, runtime));
  TRY(add_plankton_factory(map, core, "Type", plankton_new_type, runtime));
  TRY(add_plankton_factory(map, core, "UnboundModule", plankton_new_unbound_module, runtime));
  TRY(add_plankton_factory(map, core, "UnboundModuleFragment", plankton_new_unbound_module_fragment, runtime));
  // Singletons
  TRY(add_plankton_binding(map, core, "subject", ROOT(runtime, subject_key),
      runtime));
  TRY(add_plankton_binding(map, core, "selector", ROOT(runtime, selector_key),
      runtime));
  // Types
  value_t type = RSTR(runtime, type);
  TRY(add_plankton_binding(map, type, "Integer", ROOT(runtime, integer_type),
      runtime));
  // Options
  value_t options = RSTR(runtime, options);
  TRY(add_plankton_factory(map, options, "Options", plankton_new_options, runtime));
  return success();
}


// --- D e b u g ---

void print_ln(const char *fmt, ...) {
  // Write the value on a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Print the result to stdout.
  string_t str;
  string_buffer_flush(&buf, &str);
  printf("%s\n", str.chars);
  fflush(stdout);
  // Done!
  string_buffer_dispose(&buf);
}

const char *value_to_string(value_to_string_t *data, value_t value) {
  string_buffer_init(&data->buf);
  value_print_default_on(value, &data->buf);
  string_buffer_flush(&data->buf, &data->str);
  return data->str.chars;
}

void dispose_value_to_string(value_to_string_t *data) {
  string_buffer_dispose(&data->buf);
}
