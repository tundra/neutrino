//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "freeze.h"
#include "heap.h"
#include "interp.h"
#include "runtime.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "utils/log.h"
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
#define __GEN_CASE__(Family, family, MD, SR, MINOR, N)                         \
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
#define __GEN_CASE__(Name, name, SR, FLAGS, N) case tp##Name: return #Name;
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
  print_ln(NULL, "%v", self);
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

void fast_fill_with_whatever(value_t *data, size_t length) {
  // This fills the data with tagged zeroes. If you change how tagging works
  // this may break unless you ensure that all zeroes has some meaningful value.
  memset(data, 0, sizeof(value_t) * length);
}


// --- O b j e c t ---

void set_heap_object_header(value_t value, value_t species) {
  *access_heap_object_field(value, kHeapObjectHeaderOffset) = species;
}

value_t get_heap_object_header(value_t value) {
  return *access_heap_object_field(value, kHeapObjectHeaderOffset);
}

garbage_value_t get_garbage_object_field(garbage_value_t ref, size_t index) {
  value_t value = ref.value;
  CHECK_DOMAIN(vdHeapObject, value);
  // The object may have been moved so we first check whether it has and if so
  // chase it.
  value_t header = get_heap_object_header(value);
  if (in_domain(vdMovedObject, header))
    value = get_moved_object_target(header);
  CHECK_DOMAIN(vdHeapObject, value);
  // Grab the raw field unchecked.
  garbage_value_t result = {*access_heap_object_field(value, index)};
  return result;
}

heap_object_family_t get_garbage_object_family(garbage_value_t ref) {
  value_t value = ref.value;
  CHECK_DOMAIN(vdHeapObject, value);
  // The object may have been moved so we first check whether it has and if so
  // chase it.
  value_t original_header = get_heap_object_header(value);
  if (in_domain(vdMovedObject, original_header))
    value = get_moved_object_target(original_header);
  CHECK_DOMAIN(vdHeapObject, value);
  // Get the species. If the object has been moved this will be a normal
  // species, if it hasn't it may also be, but it may also be a dead species. In
  // that case the family enum will still be intact within it.
  value_t species = get_heap_object_species(value);
  return get_species_instance_family(species);
}

void set_heap_object_species(value_t value, value_t species) {
  CHECK_FAMILY(ofSpecies, species);
  set_heap_object_header(value, species);
}

bool in_syntax_family(value_t value) {
  if (!is_heap_object(value))
    return false;
  switch (get_heap_object_family(value)) {
#define __MAKE_CASE__(Family, family, MD, SR, MINOR, N)                        \
  mfEm MINOR (                                                                 \
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
CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance,
    acInDomain, vdInteger, RawMode, raw_mode);
CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance,
    acInFamilyOpt, ofArray, Derivatives, derivatives);


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

value_t set_modal_heap_object_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  value_t old_species = get_heap_object_species(self);
  value_t new_species = get_modal_species_sibling_with_mode(runtime, old_species,
      mode);
  set_heap_object_species(self, new_species);
  return success();
}

size_t get_modal_species_base_root(value_t value) {
  value_t base_root = *access_heap_object_field(value, kModalSpeciesBaseRootOffset);
  return get_integer_value(base_root);
}

void set_modal_species_base_root(value_t value, size_t base_root) {
  *access_heap_object_field(value, kModalSpeciesBaseRootOffset) = new_integer(base_root);
}


/// ## Utf8

GET_FAMILY_PRIMARY_TYPE_IMPL(utf8);
FIXED_GET_MODE_IMPL(utf8, vmDeepFrozen);

size_t calc_utf8_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kHeapObjectHeaderSize               // header
       + kValueSize                      // length
       + align_size(kValueSize, bytes);  // contents
}

INTEGER_ACCESSORS_IMPL(Utf8, utf8, Length, length);

char *get_utf8_chars(value_t value) {
  CHECK_FAMILY(ofUtf8, value);
  return (char*) access_heap_object_field(value, kUtf8CharsOffset);
}

utf8_t get_utf8_contents(value_t value) {
  return new_string(get_utf8_chars(value), get_utf8_length(value));
}

value_t utf8_validate(value_t value) {
  VALIDATE_FAMILY(ofUtf8, value);
  // Check that the string is null-terminated.
  size_t length = get_utf8_length(value);
  VALIDATE(get_utf8_chars(value)[length] == '\0');
  return success();
}

void get_utf8_layout(value_t value, heap_object_layout_t *layout) {
  // Strings have no value fields.
  size_t size = calc_utf8_size(get_utf8_length(value));
  heap_object_layout_set(layout, size, size);
}

value_t utf8_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  utf8_t contents = get_utf8_contents(value);
  hash_stream_write_data(stream, contents.chars, string_size(contents));
  return success();
}

value_t utf8_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  CHECK_FAMILY(ofUtf8, a);
  CHECK_FAMILY(ofUtf8, b);
  utf8_t a_contents = get_utf8_contents(a);
  utf8_t b_contents = get_utf8_contents(b);
  return new_boolean(string_equals(a_contents, b_contents));
}

value_t utf8_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofUtf8, a);
  CHECK_FAMILY(ofUtf8, b);
  utf8_t a_contents = get_utf8_contents(a);
  utf8_t b_contents = get_utf8_contents(b);
  return integer_to_relation(string_compare(a_contents, b_contents));
}

void utf8_print_on(value_t value, print_on_context_t *context) {
  if ((context->flags & pfUnquote) == 0)
    string_buffer_putc(context->buf, '"');
  string_buffer_append_utf8(context->buf, value);
  if ((context->flags & pfUnquote) == 0)
    string_buffer_putc(context->buf, '"');
}

void string_buffer_append_utf8(string_buffer_t *buf, value_t value) {
  CHECK_FAMILY(ofUtf8, value);
  utf8_t contents = get_utf8_contents(value);
  string_buffer_printf(buf, "%s", contents.chars);
}

static value_t string_plus_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofUtf8, self);
  CHECK_FAMILY(ofUtf8, that);
  string_buffer_t buf;
  string_buffer_init(&buf);
  utf8_t str = get_utf8_contents(self);
  string_buffer_append(&buf, str);
  str = get_utf8_contents(that);
  string_buffer_append(&buf, str);
  str = string_buffer_flush(&buf);
  TRY_DEF(result, new_heap_utf8(get_builtin_runtime(args), str));
  string_buffer_dispose(&buf);
  return result;
}

static value_t string_equals_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofUtf8, self);
  CHECK_FAMILY(ofUtf8, that);
  return new_boolean(value_identity_compare(self, that));
}

static value_t string_print_raw(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofUtf8, self);
  utf8_t contents = get_utf8_contents(self);
  fwrite(contents.chars, sizeof(char), string_size(contents), stdout);
  fputc('\n', stdout);
  return nothing();
}

static value_t string_get_ascii_characters(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofUtf8, self);
  utf8_t contents = get_utf8_contents(self);
  size_t length = string_size(contents);
  TRY_DEF(result, new_heap_array(runtime, length));
  for (size_t i = 0; i < length; i++) {
    char c = string_byte_at(contents, i);
    char char_c_str[2] = {c, '\0'};
    utf8_t char_str = {1, char_c_str};
    TRY_DEF(char_obj, new_heap_utf8(runtime, char_str));
    set_array_at(result, i, char_obj);
  }
  return result;
}

static value_t string_view_ascii(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofUtf8, self);
  return new_heap_ascii_string_view(runtime, self);
}

value_t add_utf8_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("str+str", 1, string_plus_string);
  ADD_BUILTIN_IMPL("str==str", 1, string_equals_string);
  ADD_BUILTIN_IMPL("str.print_raw()", 0, string_print_raw);
  ADD_BUILTIN_IMPL("str.get_ascii_characters()", 0, string_get_ascii_characters);
  ADD_BUILTIN_IMPL("str.view_ascii", 1, string_view_ascii);
  return success();
}


/// ## Ascii string view

GET_FAMILY_PRIMARY_TYPE_IMPL(ascii_string_view);
FIXED_GET_MODE_IMPL(ascii_string_view, vmDeepFrozen);
TRIVIAL_PRINT_ON_IMPL(AsciiStringView, ascii_string_view);

FROZEN_ACCESSORS_IMPL(AsciiStringView, ascii_string_view, acInFamily, ofUtf8,
    Value, value);

value_t ascii_string_view_validate(value_t self) {
  VALIDATE_FAMILY(ofAsciiStringView, self);
  VALIDATE_FAMILY(ofUtf8, get_ascii_string_view_value(self));
  return success();
}

static value_t ascii_string_view_get_at(builtin_arguments_t *args) {
  value_t subject = get_builtin_subject(args);
  value_t index = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofAsciiStringView, subject);
  CHECK_DOMAIN(vdInteger, index);
  value_t value = get_ascii_string_view_value(subject);
  utf8_t contents = get_utf8_contents(value);
  int64_t i = get_integer_value(index);
  return new_ascii_character(string_byte_at(contents, i));
}

static value_t ascii_string_view_length(builtin_arguments_t *args) {
  value_t subject = get_builtin_subject(args);
  CHECK_FAMILY(ofAsciiStringView, subject);
  value_t value = get_ascii_string_view_value(subject);
  return new_integer(get_utf8_length(value));
}

static value_t ascii_string_view_substring(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t subject = get_builtin_subject(args);
  value_t from = get_builtin_argument(args, 0);
  value_t to = get_builtin_argument(args, 1);
  CHECK_FAMILY(ofAsciiStringView, subject);
  CHECK_DOMAIN(vdInteger, from);
  CHECK_DOMAIN(vdInteger, to);
  value_t value = get_ascii_string_view_value(subject);
  utf8_t contents = get_utf8_contents(value);
  utf8_t substring = string_substring(contents, get_integer_value(from),
      get_integer_value(to));
  return new_heap_utf8(runtime, substring);
}

value_t add_ascii_string_view_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("ascii_string_view[]", 1, ascii_string_view_get_at);
  ADD_BUILTIN_IMPL("ascii_string_view.length", 0, ascii_string_view_length);
  ADD_BUILTIN_IMPL("ascii_string_view.substring", 2, ascii_string_view_substring);
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

blob_t get_blob_data(value_t value) {
  CHECK_FAMILY(ofBlob, value);
  size_t length = get_blob_length(value);
  void *data = access_heap_object_field(value, kBlobDataOffset);
  return new_blob(data, length);
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
  blob_t blob = get_blob_data(value);
  size_t length = blob_byte_length(blob);
  size_t bytes_to_show = (length <= 8) ? length : 8;
  for (size_t i = 0; i < bytes_to_show; i++) {
    static const char *kChars = "0123456789abcdef";
    byte_t byte = blob_byte_at(blob, i);
    string_buffer_printf(context->buf, "%c%c", kChars[byte >> 4], kChars[byte & 0xF]);
  }
  if (bytes_to_show < length)
    string_buffer_printf(context->buf, "...");
  string_buffer_printf(context->buf, "]>");
}

value_t read_stream_to_blob(runtime_t *runtime, in_stream_t *stream) {
  // Read the complete file into a byte buffer.
  byte_buffer_t buffer;
  byte_buffer_init(&buffer);
  while (true) {
    static const size_t kBufSize = 1024;
    byte_t raw_buffer[kBufSize];
    size_t was_read = in_stream_read_bytes(stream, raw_buffer, kBufSize);
    for (size_t i = 0; i < was_read; i++)
      byte_buffer_append(&buffer, raw_buffer[i]);
    if (in_stream_at_eof(stream))
      break;
  }
  blob_t data_blob = byte_buffer_flush(&buffer);
  // Create a blob to hold the result and copy the data into it.
  value_t result = new_heap_blob_with_data(runtime, data_blob);
  byte_buffer_dispose(&buffer);
  return result;
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

value_t get_pair_first(value_t pair) {
  return get_array_at(pair, 0);
}

value_t get_pair_second(value_t pair) {
  return get_array_at(pair, 1);
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

bool in_array(value_t self, value_t value) {
  for (size_t i = 0; i < get_array_length(self); i++) {
    if (value_identity_compare(value, get_array_at(self, i)))
      return true;
  }
  return false;
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


// --- F i f o   b u f f e r ---

TRIVIAL_PRINT_ON_IMPL(FifoBuffer, fifo_buffer);
FIXED_GET_MODE_IMPL(fifo_buffer, vmMutable);

ACCESSORS_IMPL(FifoBuffer, fifo_buffer, acInFamily, ofArray, Nodes, nodes);
INTEGER_ACCESSORS_IMPL(FifoBuffer, fifo_buffer, Size, size);
INTEGER_ACCESSORS_IMPL(FifoBuffer, fifo_buffer, Width, width);

value_t fifo_buffer_validate(value_t self) {
  VALIDATE_FAMILY(ofFifoBuffer, self);
  VALIDATE_FAMILY(ofArray, get_fifo_buffer_nodes(self));
  return success();
}

size_t get_fifo_buffer_nodes_length(size_t width, size_t capacity) {
  size_t count = capacity + kFifoBufferReservedNodeCount;
  return get_fifo_buffer_node_length_for_width(width) * count;
}

static size_t get_fifo_buffer_node_length(value_t self) {
  return get_fifo_buffer_node_length_for_width(get_fifo_buffer_width(self));
}

size_t get_fifo_buffer_capacity(value_t self) {
  CHECK_FAMILY(ofFifoBuffer, self);
  value_t data = get_fifo_buffer_nodes(self);
  return (get_array_length(data) / get_fifo_buffer_node_length(self)) - kFifoBufferReservedNodeCount;
}

void get_fifo_buffer_values_at(value_t self, size_t index, value_t *values_out,
    size_t values_size) {
  CHECK_EQ("invalid fifo buffer get", get_fifo_buffer_width(self), values_size);
  size_t node_offset = index * get_fifo_buffer_node_length(self) + kFifoBufferNodeHeaderSize;
  value_t *nodes_start = get_array_elements(get_fifo_buffer_nodes(self));
  memcpy(values_out, nodes_start + node_offset, sizeof(value_t) * values_size);
}

void set_fifo_buffer_values_at(value_t self, size_t index, value_t *values,
    size_t values_size) {
  CHECK_EQ("invalid fifo buffer set", get_fifo_buffer_width(self), values_size);
  size_t node_offset = index * get_fifo_buffer_node_length(self) + kFifoBufferNodeHeaderSize;
  value_t *nodes_start = get_array_elements(get_fifo_buffer_nodes(self));
  memcpy(nodes_start + node_offset, values, sizeof(value_t) * values_size);
}

void clear_fifo_buffer_values_at(value_t self, size_t index) {
  size_t width = get_fifo_buffer_width(self);
  size_t node_offset = index * get_fifo_buffer_node_length(self) + kFifoBufferNodeHeaderSize;
  value_t *nodes_start = get_array_elements(get_fifo_buffer_nodes(self));
  fast_fill_with_whatever(nodes_start + node_offset, width);
}

size_t get_fifo_buffer_next_at(value_t self, size_t index) {
  return get_integer_value(get_array_at(get_fifo_buffer_nodes(self),
      index * get_fifo_buffer_node_length(self)));
}

void set_fifo_buffer_next_at(value_t self, size_t index, size_t next) {
  set_array_at(get_fifo_buffer_nodes(self), index * get_fifo_buffer_node_length(self),
      new_integer(next));
}

size_t get_fifo_buffer_prev_at(value_t self, size_t index) {
  return get_integer_value(get_array_at(get_fifo_buffer_nodes(self),
      index * get_fifo_buffer_node_length(self) + 1));
}

void set_fifo_buffer_prev_at(value_t self, size_t index, size_t prev) {
  set_array_at(get_fifo_buffer_nodes(self), index * get_fifo_buffer_node_length(self) + 1,
      new_integer(prev));
}

// Unhooks the node at the given index from the list it's currently in.
static void unhook_fifo_buffer_entry(value_t self, size_t index) {
  size_t prev = get_fifo_buffer_prev_at(self, index);
  size_t next = get_fifo_buffer_next_at(self, index);
  set_fifo_buffer_next_at(self, prev, next);
  set_fifo_buffer_prev_at(self, next, prev);
}

// Hooks the given target node into the list that index is currently in.
static void hook_fifo_buffer_entry(value_t self, size_t index, size_t target) {
  size_t next = get_fifo_buffer_next_at(self, index);
  size_t prev = get_fifo_buffer_prev_at(self, next);
  set_fifo_buffer_next_at(self, target, next);
  set_fifo_buffer_prev_at(self, target, prev);
  set_fifo_buffer_next_at(self, prev, target);
  set_fifo_buffer_prev_at(self, next, target);
}

bool try_offer_to_fifo_buffer(value_t self, value_t *values, size_t values_size) {
  CHECK_FAMILY(ofFifoBuffer, self);
  CHECK_MUTABLE(self);
  CHECK_EQ("offer has wrong width", get_fifo_buffer_width(self), values_size);
  size_t capacity = get_fifo_buffer_capacity(self);
  size_t size = get_fifo_buffer_size(self);
  if (size == capacity)
    // If the buffer is full we bail out immediately.
    return false;
  // Take the next free node, which we know must exist because of the size.
  size_t next_free = get_fifo_buffer_next_at(self, kFifoBufferFreeRootOffset);
  // Unhook it from the free list.
  unhook_fifo_buffer_entry(self, next_free);
  // Hook it into the occupied list.
  hook_fifo_buffer_entry(self, kFifoBufferOccupiedRootOffset, next_free);
  set_fifo_buffer_values_at(self, next_free, values, values_size);
  set_fifo_buffer_size(self, size + 1);
  return true;
}

value_t take_from_fifo_buffer(value_t self, value_t *values_out, size_t values_size) {
  CHECK_FAMILY(ofFifoBuffer, self);
  CHECK_EQ("taking wrong size", get_fifo_buffer_width(self), values_size);
  if (is_fifo_buffer_empty(self))
    return new_condition(ccNotFound);
  // Get the least recently accessed occupied node.
  size_t prev_occupied = get_fifo_buffer_prev_at(self, kFifoBufferOccupiedRootOffset);
  // Clear the value; there's no reason to keep it alive from here.
  get_fifo_buffer_values_at(self, prev_occupied, values_out, values_size);
  clear_fifo_buffer_values_at(self, prev_occupied);
  // Unhook it from the occupied list.
  unhook_fifo_buffer_entry(self, prev_occupied);
  // Hook it into the free list.
  hook_fifo_buffer_entry(self, kFifoBufferFreeRootOffset, prev_occupied);
  size_t size = get_fifo_buffer_size(self);
  set_fifo_buffer_size(self, size - 1);
  return success();
}

bool is_fifo_buffer_empty(value_t self) {
  CHECK_FAMILY(ofFifoBuffer, self);
  return get_fifo_buffer_size(self) == 0;
}

value_t offer_to_fifo_buffer(runtime_t *runtime, value_t buffer, value_t *values,
    size_t values_size) {
  if (try_offer_to_fifo_buffer(buffer, values, values_size))
    // The value fits without expanding; success!
    return success();
  // Doesn't fit; allocate a new backing array.
  size_t old_capacity = get_fifo_buffer_capacity(buffer);
  value_t old_nodes = get_fifo_buffer_nodes(buffer);
  size_t old_nodes_size = get_array_length(old_nodes);
  size_t new_nodes_size = old_nodes_size << 1;
  TRY_DEF(new_nodes, new_heap_array(runtime, new_nodes_size));
  // Copy the contents of the old array directly into the new one.
  // TODO: memcpy?
  for (size_t i = 0; i < old_nodes_size; i++)
    set_array_at(new_nodes, i, get_array_at(old_nodes, i));
  set_fifo_buffer_nodes(buffer, new_nodes);
  size_t new_capacity = get_fifo_buffer_capacity(buffer);
  // Make the new entries point to each other. This leaves two of the nodes with
  // an incorrect value: the prev of the first will be wrong, as will the next
  // of the last. We'll fix those below.
  size_t new_first = old_capacity + kFifoBufferReservedNodeCount;
  size_t new_last = new_capacity + kFifoBufferReservedNodeCount - 1;
  for (size_t i = new_first; i <= new_last; i++) {
    set_fifo_buffer_next_at(buffer, i, i + 1);
    set_fifo_buffer_prev_at(buffer, i, i - 1);
  }
  // Hook the new entries into the free list. Since offering failed above we
  // know that there are no free nodes left so we can hook the new ones directly
  // onto the free root.
  set_fifo_buffer_next_at(buffer, kFifoBufferFreeRootOffset, new_first);
  set_fifo_buffer_prev_at(buffer, new_first, kFifoBufferFreeRootOffset);
  set_fifo_buffer_prev_at(buffer, kFifoBufferFreeRootOffset, new_last);
  set_fifo_buffer_next_at(buffer, new_last, kFifoBufferFreeRootOffset);
  bool expanded = try_offer_to_fifo_buffer(buffer, values, values_size);
  CHECK_TRUE("failed to expand fifo buffer", expanded);
  return success();
}

void fifo_buffer_iter_init(fifo_buffer_iter_t *iter, value_t buf) {
  CHECK_FAMILY(ofFifoBuffer, buf);
  iter->buffer = buf;
  iter->current = kFifoBufferFreeRootOffset;
}

bool fifo_buffer_iter_advance(fifo_buffer_iter_t *iter) {
  if (iter->current == kFifoBufferFreeRootOffset) {
    if (is_fifo_buffer_empty(iter->buffer))
      return false;
    iter->current = get_fifo_buffer_prev_at(iter->buffer,
        kFifoBufferOccupiedRootOffset);
    return true;
  } else {
    iter->current = get_fifo_buffer_prev_at(iter->buffer, iter->current);
    return (iter->current != kFifoBufferOccupiedRootOffset);
  }
}

void fifo_buffer_iter_get_current(fifo_buffer_iter_t *iter, value_t *values_out,
    size_t values_size) {
  get_fifo_buffer_values_at(iter->buffer, iter->current, values_out,
      values_size);
}

void fifo_buffer_iter_take_current(fifo_buffer_iter_t *iter) {
  // Clear the value; there's no reason to keep it alive from here.
  clear_fifo_buffer_values_at(iter->buffer, iter->current);
  // Unhook it from the occupied list.
  unhook_fifo_buffer_entry(iter->buffer, iter->current);
  // Hook it into the free list.
  hook_fifo_buffer_entry(iter->buffer, kFifoBufferFreeRootOffset, iter->current);
  size_t size = get_fifo_buffer_size(iter->buffer);
  set_fifo_buffer_size(iter->buffer, size - 1);
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

value_t plankton_new_key(runtime_t *runtime) {
  return nothing();
}

value_t plankton_set_key_contents(value_t instance, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, id);
  if (value_identity_compare(id_value, RSTR(runtime, core_is_async))) {
    return ROOT(runtime, is_async_key);
  } else if (value_identity_compare(id_value, RSTR(runtime, core_selector))) {
    return ROOT(runtime, selector_key);
  } else if (value_identity_compare(id_value, RSTR(runtime, core_subject))) {
    return ROOT(runtime, subject_key);
  } else {
    return new_heap_key(runtime, id_value);
  }
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
  return (value_mode_t) get_integer_value(get_instance_raw_mode(self));
}

value_t set_instance_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  // Get or create the derivatives array.
  value_t species = get_heap_object_species(self);
  value_t derivatives = get_instance_species_derivatives(species);
  if (is_nothing(derivatives)) {
    TRY_SET(derivatives, new_heap_array(runtime, 4));
    set_instance_species_derivatives(species, derivatives);
  }
  // Get or create the clone with the right mode from the derivatives.
  value_t new_species = get_array_at(derivatives, (size_t) mode);
  if (is_null(new_species)) {
    TRY_SET(new_species, clone_heap_instance_species(runtime, species));
    set_instance_species_raw_mode(new_species, new_integer(mode));
    set_array_at(derivatives, (size_t) mode, new_species);
  }
  // Change to the new species with the right mode.
  set_heap_object_species(self, new_species);
  return new_species;
}

value_t ensure_instance_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_instance_fields(self)));
  return success();
}


// --- I n s t a n c e   m a n a g e r ---

FROZEN_ACCESSORS_IMPL(InstanceManager, instance_manager, acNoCheck, 0, DisplayName, display_name);
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
  TRY_DEF(species, new_heap_instance_species(runtime, type, self, vmFluid));
  return new_heap_instance(runtime, species);
}

value_t add_instance_manager_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("instance_manager.new_instance", 1, instance_manager_new_instance);
  return success();
}


// --- F a c t o r y ---

FROZEN_ACCESSORS_IMPL(Factory, factory, acInFamily, ofVoidP, NewInstance, new_instance);
FROZEN_ACCESSORS_IMPL(Factory, factory, acInFamily, ofVoidP, SetContents, set_contents);
FROZEN_ACCESSORS_IMPL(Factory, factory, acInFamily, ofUtf8, Name, name);
FIXED_GET_MODE_IMPL(factory, vmDeepFrozen);

value_t factory_validate(value_t value) {
  VALIDATE_FAMILY(ofFactory, value);
  VALIDATE_FAMILY(ofVoidP, get_factory_new_instance(value));
  VALIDATE_FAMILY(ofVoidP, get_factory_set_contents(value));
  VALIDATE_FAMILY(ofUtf8, get_factory_name(value));
  return success();
}

void factory_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofFactory, value);
  string_buffer_printf(context->buf, "#<factory: ");
  value_print_inner_on(get_factory_new_instance(value), context, -1);
  string_buffer_printf(context->buf, " ");
  value_print_inner_on(get_factory_set_contents(value), context, -1);
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
  return new_heap_type(runtime, afMutable, nothing());
}

value_t plankton_set_type_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_type_display_name(object, name_value);
  return ensure_frozen(runtime, object);
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
  TRY(ensure_id_hash_map_frozen(runtime, get_namespace_bindings(self), mfFreezeValues));
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
  value_t special_imports = ROOT(runtime, special_imports);
  value_t special_import = get_id_hash_map_at(special_imports, head);
  if (!in_condition_cause(ccNotFound, special_import))
    return special_import;
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

value_t ensure_module_fragment_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_module_fragment_private(self)));
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


// --- M o d u l e   f r a g m e n t   a c c e s s ---

GET_FAMILY_PRIMARY_TYPE_IMPL(module_fragment_private);

ACCESSORS_IMPL(ModuleFragmentPrivate, module_fragment_private, acInFamilyOpt,
    ofModuleFragment, Owner, owner);

value_t module_fragment_private_validate(value_t value) {
  VALIDATE_FAMILY(ofModuleFragmentPrivate, value);
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_module_fragment_private_owner(value));
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
  runtime_t *runtime = get_builtin_runtime(args);
  return new_heap_type(runtime, afFreeze, display_name);
}

static value_t module_fragment_private_new_hard_field(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofModuleFragmentPrivate, self);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY(validate_deep_frozen(runtime, display_name, NULL));
  return new_heap_hard_field(runtime, display_name);
}

static value_t module_fragment_private_new_soft_field(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofModuleFragmentPrivate, self);
  runtime_t *runtime = get_builtin_runtime(args);
  return new_heap_soft_field(runtime, display_name);
}

value_t emit_module_fragment_private_invoke(assembler_t *assm) {
  TRY(assembler_emit_module_fragment_private_invoke(assm));
  return success();
}

value_t add_module_fragment_private_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL("module_fragment_private.new_type", 1,
      module_fragment_private_new_type);
  ADD_BUILTIN_IMPL("module_fragment_private.new_hard_field", 1,
      module_fragment_private_new_hard_field);
  ADD_BUILTIN_IMPL("module_fragment_private.new_soft_field", 1,
      module_fragment_private_new_soft_field);
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
    TRY_DEF(tail, new_heap_path_with_names(runtime, afFreeze, names_value, 1));
    set_path_raw_head(object, head);
    set_path_raw_tail(object, tail);
  }
  return ensure_frozen(runtime, object);
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

ACCESSORS_IMPL(Identifier, identifier, acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(Identifier, identifier, acNoCheck, 0, Stage, stage);

value_t identifier_validate(value_t self) {
  VALIDATE_FAMILY(ofIdentifier, self);
  VALIDATE_FAMILY_OPT(ofPath, get_identifier_path(self));
  return success();
}

value_t plankton_new_identifier(runtime_t *runtime) {
  return new_heap_identifier(runtime, afMutable, nothing(), nothing());
}

value_t plankton_set_identifier_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, stage, path);
  set_identifier_stage(object, new_stage_offset(get_integer_value(stage_value)));
  set_identifier_path(object, path_value);
  return ensure_frozen(runtime, object);
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


// --- D e c i m a l   f r a c t i o n ---

FIXED_GET_MODE_IMPL(decimal_fraction, vmDeepFrozen);

FROZEN_ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Numerator, numerator);
FROZEN_ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Denominator, denominator);
FROZEN_ACCESSORS_IMPL(DecimalFraction, decimal_fraction, acNoCheck, 0, Precision, precision);

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
  TRY(validate_deep_frozen(runtime, numerator_value, NULL));
  TRY(validate_deep_frozen(runtime, denominator_value, NULL));
  TRY(validate_deep_frozen(runtime, precision_value, NULL));
  init_frozen_decimal_fraction_numerator(object, numerator_value);
  init_frozen_decimal_fraction_denominator(object, denominator_value);
  init_frozen_decimal_fraction_precision(object, precision_value);
  return success();
}

value_t plankton_new_decimal_fraction(runtime_t *runtime) {
  return new_heap_decimal_fraction(runtime, nothing(), nothing(), nothing());
}


/// ## Hard field

GET_FAMILY_PRIMARY_TYPE_IMPL(hard_field);
FIXED_GET_MODE_IMPL(hard_field, vmDeepFrozen);

FROZEN_ACCESSORS_IMPL(HardField, hard_field, acNoCheck, 0, DisplayName, display_name);

value_t hard_field_validate(value_t self) {
  VALIDATE_FAMILY(ofHardField, self);
  return success();
}

void hard_field_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, ".$");
  value_t display_name = get_hard_field_display_name(value);
  value_print_inner_on(display_name, context, -1);
  string_buffer_printf(context->buf, "");
}

static value_t hard_field_set(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofHardField, self);
  value_t instance = get_builtin_argument(args, 0);
  if (is_mutable(instance)) {
    value_t value = get_builtin_argument(args, 1);
    runtime_t *runtime = get_builtin_runtime(args);
    return set_instance_field(runtime, instance, self, value);
  } else {
    value_t display_name = get_hard_field_display_name(self);
    ESCAPE_BUILTIN(args, changing_frozen, display_name, instance);
  }
}

static value_t hard_field_get(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofHardField, self);
  value_t instance = get_builtin_argument(args, 0);
  value_t value = get_instance_field(instance, self);
  if (in_condition_cause(ccNotFound, value)) {
    value_t display_name = get_hard_field_display_name(self);
    ESCAPE_BUILTIN(args, no_such_field, display_name, instance);
  } else {
    return value;
  }
}

value_t add_hard_field_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("hard_field[]", 1, 2, hard_field_get);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("hard_field[]:=()", 2, 2, hard_field_set);
  return success();
}


/// ## Soft field

GET_FAMILY_PRIMARY_TYPE_IMPL(soft_field);
FIXED_GET_MODE_IMPL(soft_field, vmMutable);

ACCESSORS_IMPL(SoftField, soft_field, acNoCheck, 0, DisplayName, display_name);
ACCESSORS_IMPL(SoftField, soft_field, acInFamily, ofIdHashMap, OverlayMap, overlay_map);

value_t soft_field_validate(value_t self) {
  VALIDATE_FAMILY(ofSoftField, self);
  VALIDATE_FAMILY(ofIdHashMap, get_soft_field_overlay_map(self));
  return success();
}

void soft_field_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "..$");
  value_t display_name = get_soft_field_display_name(value);
  value_print_inner_on(display_name, context, -1);
  string_buffer_printf(context->buf, "");
}

static value_t soft_field_set(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofSoftField, self);
  value_t holder = get_builtin_argument(args, 0);
  value_t value = get_builtin_argument(args, 1);
  runtime_t *runtime = get_builtin_runtime(args);
  if (in_family(ofInstance, holder) && is_mutable(holder)) {
    // If the object is still mutable we can just set the field on it.
    TRY(set_instance_field(runtime, holder, self, value));
  } else {
    // Otherwise we'll set/shadow the field in the map in the field itself.
    value_t overlay_map = get_soft_field_overlay_map(self);
    TRY(set_id_hash_map_at(runtime, overlay_map, holder, value));
  }
  return value;
}

static value_t soft_field_get(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofSoftField, self);
  value_t holder = get_builtin_argument(args, 0);
  // First look for a binding in the overlay since that always takes precedence.
  value_t overlay_map = get_soft_field_overlay_map(self);
  value_t overlay_value = get_id_hash_map_at(overlay_map, holder);
  if (!in_condition_cause(ccNotFound, overlay_value))
    return overlay_value;
  // If it's not there try in the object itself.
  if (in_family(ofInstance, holder)) {
    value_t instance_value = get_instance_field(holder, self);
    if (!in_condition_cause(ccNotFound, instance_value))
      return instance_value;
  }
  value_t display_name = get_soft_field_display_name(self);
  ESCAPE_BUILTIN(args, no_such_field, display_name, holder);
}

value_t add_soft_field_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("soft_field[]", 1, 2, soft_field_get);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("soft_field[]:=()", 2, 2, soft_field_set);
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

ACCESSORS_IMPL(Ambience, ambience, acInFamily, ofMethodspace, Methodspace,
    methodspace);

value_t ambience_validate(value_t self) {
  VALIDATE_FAMILY(ofAmbience, self);
  VALIDATE_FAMILY(ofMethodspace, get_ambience_methodspace(self));
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


/// ## Hash oracle

GET_FAMILY_PRIMARY_TYPE_IMPL(hash_oracle);
TRIVIAL_PRINT_ON_IMPL(HashOracle, hash_oracle);

ACCESSORS_IMPL(HashOracle, hash_oracle, acInFamily, ofHashSource, Source,
    source);
FROZEN_ACCESSORS_IMPL(HashOracle, hash_oracle, acInDomainOpt, vdInteger, Limit,
    limit);

value_t hash_oracle_validate(value_t self) {
  VALIDATE_FAMILY(ofHashOracle, self);
  VALIDATE_FAMILY(ofHashSource, get_hash_oracle_source(self));
  VALIDATE_DOMAIN_OPT(vdInteger, get_hash_oracle_limit(self));
  return success();
}

static value_t hash_oracle_ensure_hash_code(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofHashOracle, self);
  CHECK_MUTABLE(self);
  value_t value = get_builtin_argument(args, 0);
  value_t source = get_hash_oracle_source(self);
  value_t field = get_hash_source_field(source);
  value_t hash_pair = get_instance_field(value, field);
  if (in_condition_cause(ccNotFound, hash_pair)) {
    // There's no hash already so we have to bind one. Generate one from the
    // twister.
    runtime_t *runtime = get_builtin_runtime(args);
    hash_source_state_t *state = get_hash_source_state(source);
    uint64_t next_next_serial = state->next_serial + 1;
    tinymt64_state_t new_twister_state;
    uint64_t code64 = tinymt64_next_uint64(&state->twister, &new_twister_state);
    value_t hash_code = new_hash_code(code64);
    // Try to cache the hash state in the object.
    TRY_SET(hash_pair, new_heap_pair(runtime, hash_code, new_integer(state->next_serial)));
    TRY(set_instance_field(runtime, value, field, hash_pair));
    // If we succeeded we can update the state. If any part failed we have to
    // be sure that nothing has changed so we can try again, that's why the
    // update comes at the very end.
    state->next_serial = next_next_serial;
    state->twister.state = new_twister_state;
  }
  return get_pair_first(hash_pair);
}

static value_t hash_oracle_peek_hash_code(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofHashOracle, self);
  value_t value = get_builtin_argument(args, 0);
  value_t source = get_hash_oracle_source(self);
  value_t field = get_hash_source_field(source);
  value_t hash_pair = get_instance_field(value, field);
  if (in_condition_cause(ccNotFound, hash_pair)) {
    return null();
  } else {
    value_t limit = get_hash_oracle_limit(self);
    if (is_nothing(limit)) {
      return get_pair_first(hash_pair);
    } else {
      value_t serial = get_pair_second(hash_pair);
      if (get_integer_value(serial) < get_integer_value(limit)) {
        return get_pair_first(hash_pair);
      } else {
        return null();
      }
    }
  }
}

value_t ensure_hash_oracle_owned_values_frozen(runtime_t *runtime, value_t self) {
  CHECK_FAMILY(ofHashOracle, self);
  value_t source = get_hash_oracle_source(self);
  hash_source_state_t *state = get_hash_source_state(source);
  init_frozen_hash_oracle_limit(self, new_integer(state->next_serial));
  return success();
}

value_t add_hash_oracle_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("hash_oracle.ensure_hash_code!", 1, hash_oracle_ensure_hash_code);
  ADD_BUILTIN_IMPL("hash_oracle.peek_hash_code", 1, hash_oracle_peek_hash_code);
  return success();
}


/// ## Hash source

GET_FAMILY_PRIMARY_TYPE_IMPL(hash_source);
TRIVIAL_PRINT_ON_IMPL(HashSource, hash_source);
FIXED_GET_MODE_IMPL(hash_source, vmMutable);

static size_t hash_source_state_size() {
  return align_size(kValueSize, sizeof(hash_source_state_t));
}

static size_t hash_source_values_offset() {
  return kHeapObjectHeaderSize + hash_source_state_size();
}

size_t hash_source_size() {
  return kHeapObjectHeaderSize    // header
      + kValueSize                // field
      + hash_source_state_size(); // contents
}

hash_source_state_t *get_hash_source_state(value_t self) {
  CHECK_FAMILY(ofHashSource, self);
  void *data = access_heap_object_field(self, kHashSourceStateOffset);
  return (hash_source_state_t*) data;
}

void set_hash_source_field(value_t self, value_t value) {
  CHECK_FAMILY(ofHashSource, self);
  CHECK_MUTABLE(self);
  CHECK_FAMILY(ofSoftField, value);
  *access_heap_object_field(self, hash_source_values_offset()) = value;
}

value_t get_hash_source_field(value_t self) {
  CHECK_FAMILY(ofHashSource, self);
  return *access_heap_object_field(self, hash_source_values_offset());
}

void get_hash_source_layout(value_t value, heap_object_layout_t *layout) {
  size_t size = hash_source_size();
  heap_object_layout_set(layout, size, hash_source_values_offset());
}

value_t hash_source_validate(value_t self) {
  VALIDATE_FAMILY(ofHashSource, self);
  VALIDATE_FAMILY(ofSoftField, get_hash_source_field(self));
  return success();
}

value_t add_hash_source_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  return success();
}



// --- M i s c ---

value_t add_plankton_factory(value_t map, const char *name,
    factory_new_instance_t new_instance, factory_set_contents_t set_contents,
    runtime_t *runtime) {
  TRY_DEF(factory, new_heap_factory(runtime, new_instance, set_contents, new_c_string(name)));
  TRY_DEF(name_obj, new_heap_utf8(runtime, new_c_string(name)));
  TRY(set_id_hash_map_at(runtime, map, name_obj, factory));
  return success();
}

value_t init_plankton_core_factories(value_t map, runtime_t *runtime) {
  ADD_PLANKTON_FACTORY(map, "core:DecimalFraction", decimal_fraction);
  ADD_PLANKTON_FACTORY(map, "core:Identifier", identifier);
  ADD_PLANKTON_FACTORY(map, "core:Library", library);
  ADD_PLANKTON_FACTORY(map, "core:Operation", operation);
  ADD_PLANKTON_FACTORY(map, "core:Path", path);
  ADD_PLANKTON_FACTORY(map, "core:Type", type);
  ADD_PLANKTON_FACTORY(map, "core:UnboundModule", unbound_module);
  ADD_PLANKTON_FACTORY(map, "core:UnboundModuleFragment", unbound_module_fragment);
  ADD_PLANKTON_FACTORY(map, "core:Key", key);
  return success();
}


// --- D e b u g ---

void print_ln(out_stream_t *out, const char *fmt, ...) {
  if (out == NULL)
    out = file_system_stdout(file_system_native());
  // Write the value on a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Print the result to stdout.
  utf8_t str = string_buffer_flush(&buf);
  out_stream_printf(out, "%s\n", str.chars);
  out_stream_flush(out);
  // Done!
  string_buffer_dispose(&buf);
}

const char *value_to_string(value_to_string_t *data, value_t value) {
  string_buffer_init(&data->buf);
  value_print_default_on(value, &data->buf);
  data->str = string_buffer_flush(&data->buf);
  return data->str.chars;
}

void dispose_value_to_string(value_to_string_t *data) {
  string_buffer_dispose(&data->buf);
}
