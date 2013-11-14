// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "heap.h"
#include "interp.h"
#include "log.h"
#include "runtime.h"
#include "try-inl.h"
#include "value-inl.h"

const char *get_value_domain_name(value_domain_t domain) {
  switch (domain) {
    case vdInteger:
      return "Integer";
    case vdObject:
      return "Object";
    case vdMovedObject:
      return "MovedObject";
    case vdSignal:
      return "Signal";
    default:
      return "invalid domain";
  }
}

const char *get_object_family_name(object_family_t family) {
  switch (family) {
#define __GEN_CASE__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)       \
    case of##Family: return #Family;
    ENUM_OBJECT_FAMILIES(__GEN_CASE__)
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


// --- I n t e g e r ---

static value_t integer_plus_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(this.encoded + that.encoded);
}

static value_t integer_minus_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(this.encoded - that.encoded);
}

static value_t integer_times_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(this) * get_integer_value(that);
  return new_integer(result);
}

static value_t integer_divide_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(this) / get_integer_value(that);
  return new_integer(result);
}

static value_t integer_modulo_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  int64_t result = get_integer_value(this) % get_integer_value(that);
  return new_integer(result);
}

static value_t integer_equals_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  runtime_t *runtime = get_builtin_runtime(args);
  return runtime_bool(runtime, is_same_value(this, that));
}

static value_t integer_negate(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  CHECK_DOMAIN(vdInteger, this);
  return decode_value(-this.encoded);
}

static value_t integer_print(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  print_ln("%v", this);
  return ROOT(args->runtime, nothing);
}

value_t add_integer_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(integer, INFIX("+"), 1, integer_plus_integer);
  ADD_BUILTIN(integer, INFIX("-"), 1, integer_minus_integer);
  ADD_BUILTIN(integer, INFIX("*"), 1, integer_times_integer);
  ADD_BUILTIN(integer, INFIX("/"), 1, integer_divide_integer);
  ADD_BUILTIN(integer, INFIX("%"), 1, integer_modulo_integer);
  ADD_BUILTIN(integer, INFIX("=="), 1, integer_equals_integer);
  ADD_BUILTIN(integer, PREFIX("-"), 0, integer_negate);
  ADD_BUILTIN(integer, INFIX("print"), 0, integer_print);
  return success();
}


// --- O b j e c t ---

void set_object_header(value_t value, value_t species) {
  *access_object_field(value, kObjectHeaderOffset) = species;
}

value_t get_object_header(value_t value) {
  return *access_object_field(value, kObjectHeaderOffset);
}

void set_object_species(value_t value, value_t species) {
  CHECK_FAMILY(ofSpecies, species);
  set_object_header(value, species);
}

value_t get_object_species(value_t value) {
  return *access_object_field(value, kObjectHeaderOffset);
}

object_family_t get_object_family(value_t value) {
  value_t species = get_object_species(value);
  return get_species_instance_family(species);
}

bool in_syntax_family(value_t value) {
  if (get_value_domain(value) != vdObject)
    return false;
  switch (get_object_family(value)) {
#define __MAKE_CASE__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)      \
  EM(                                                                          \
    case of##Family: return true;,                                             \
    )
    ENUM_OBJECT_FAMILIES(__MAKE_CASE__)
#undef __MAKE_CASE__
    default:
      return false;
  }
}

const char *in_syntax_family_name(bool value) {
  return value ? "true" : "false";
}

family_behavior_t *get_object_family_behavior(value_t self) {
  CHECK_DOMAIN(vdObject, self);
  value_t species = get_object_species(self);
  CHECK_TRUE("invalid object header", in_family(ofSpecies, species));
  return get_species_family_behavior(species);
}

family_behavior_t *get_object_family_behavior_unchecked(value_t self) {
  CHECK_DOMAIN(vdObject, self);
  value_t species = get_object_species(self);
  return get_species_family_behavior(species);
}


// --- S p e c i e s ---

TRIVIAL_PRINT_ON_IMPL(Species, species);

void set_species_instance_family(value_t value,
    object_family_t instance_family) {
  *access_object_field(value, kSpeciesInstanceFamilyOffset) =
      new_integer(instance_family);
}

object_family_t get_species_instance_family(value_t value) {
  value_t family = *access_object_field(value, kSpeciesInstanceFamilyOffset);
  return (object_family_t) get_integer_value(family);
}

void set_species_family_behavior(value_t value, family_behavior_t *behavior) {
  *access_object_field(value, kSpeciesFamilyBehaviorOffset) = pointer_to_value_bit_cast(behavior);
}

family_behavior_t *get_species_family_behavior(value_t value) {
  return value_to_pointer_bit_cast(*access_object_field(value, kSpeciesFamilyBehaviorOffset));
}

void set_species_division_behavior(value_t value, division_behavior_t *behavior) {
  *access_object_field(value, kSpeciesDivisionBehaviorOffset) = pointer_to_value_bit_cast(behavior);
}

division_behavior_t *get_species_division_behavior(value_t value) {
  return value_to_pointer_bit_cast(*access_object_field(value, kSpeciesDivisionBehaviorOffset));
}

species_division_t get_species_division(value_t value) {
  return get_species_division_behavior(value)->division;
}

species_division_t get_object_division(value_t value) {
  return get_species_division(get_object_species(value));
}

value_t species_validate(value_t value) {
  VALIDATE_FAMILY(ofSpecies, value);
  return success();
}

void get_species_layout(value_t value, object_layout_t *layout) {
  division_behavior_t *behavior = get_species_division_behavior(value);
  (behavior->get_species_layout)(value, layout);
}


// --- C o m p a c t   s p e c i e s ---

void get_compact_species_layout(value_t species, object_layout_t *layout) {
  // Compact species have no value fields.
  object_layout_set(layout, kCompactSpeciesSize, kCompactSpeciesSize);
}


// --- I n s t a n c e   s p e c i e s ---

void get_instance_species_layout(value_t species, object_layout_t *layout) {
  // The object is kInstanceSpeciesSize large and the values start after the
  // header.
  object_layout_set(layout, kInstanceSpeciesSize, kSpeciesHeaderSize);
}

CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance, Protocol,
    PrimaryProtocol, primary_protocol);


// --- M o d a l   s p e c i e s ---

void get_modal_species_layout(value_t species, object_layout_t *layout) {
  // The object is kModalSpeciesSize large and there are no values. Well okay
  // the mode is stored as a tagged integer but that doesn't count.
  object_layout_set(layout, kModalSpeciesSize, kModalSpeciesSize);
}

value_mode_t get_modal_species_mode(value_t value) {
  value_t mode = *access_object_field(value, kModalSpeciesModeOffset);
  return (object_family_t) get_integer_value(mode);
}

void set_modal_species_mode(value_t value, value_mode_t mode) {
  *access_object_field(value, kModalSpeciesModeOffset) = new_integer(mode);
}

value_mode_t get_modal_object_mode(value_t value) {
  value_t species = get_object_species(value);
  CHECK_DIVISION(sdModal, species);
  return get_modal_species_mode(species);
}

value_t set_modal_object_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  value_t old_species = get_object_species(self);
  value_t new_species = get_modal_species_sibling_with_mode(runtime, old_species,
      mode);
  set_object_species(self, new_species);
  return success();
}

size_t get_modal_species_base_root(value_t value) {
  value_t base_root = *access_object_field(value, kModalSpeciesBaseRootOffset);
  return get_integer_value(base_root);
}

void set_modal_species_base_root(value_t value, size_t base_root) {
  *access_object_field(value, kModalSpeciesBaseRootOffset) = new_integer(base_root);
}

bool is_mutable(value_t value) {
  return get_value_mode(value) <= vmMutable;
}

bool is_frozen(value_t value) {
  return get_value_mode(value) >= vmFrozen;
}

bool peek_deep_frozen(value_t value) {
  return get_value_mode(value) == vmDeepFrozen;
}

value_t ensure_shallow_frozen(runtime_t *runtime, value_t value) {
  return set_value_mode(runtime, value, vmFrozen);
}

value_t ensure_frozen(runtime_t *runtime, value_t value) {
  if (get_value_mode(value) == vmDeepFrozen)
    return success();
  if (get_value_domain(value) == vdObject) {
    TRY(ensure_shallow_frozen(runtime, value));
    family_behavior_t *behavior = get_object_family_behavior(value);
    value_t (*freeze_owned)(runtime_t*, value_t) = behavior->ensure_owned_values_frozen;
    if (freeze_owned == NULL) {
      return success();
    } else {
      return freeze_owned(runtime, value);
    }
  } else {
    UNREACHABLE("non-object not deep frozen");
    return new_signal(scNotDeepFrozen);
  }
}

// Assume tentatively that the given value is deep frozen and then see if that
// makes the whole graph deep frozen. If not we'll restore the object, otherwise
// we can leave it deep frozen.
value_t transitively_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  CHECK_DOMAIN(vdObject, value);
  CHECK_EQ("tentatively freezing non-frozen", vmFrozen, get_value_mode(value));
  // Deep freeze the object.
  set_value_mode_unchecked(runtime, value, vmDeepFrozen);
  // Scan through the object's fields.
  value_field_iter_t iter;
  value_field_iter_init(&iter, value);
  value_t *field = NULL;
  while (value_field_iter_next(&iter, &field)) {
    // Try to deep freeze the field's value.
    value_t ensured = validate_deep_frozen(runtime, *field, offender_out);
    if (get_value_domain(ensured) == vdSignal) {
      // Deep freezing failed for some reason. Restore the object to its previous
      // state and bail.
      set_value_mode_unchecked(runtime, value, vmFrozen);
      return ensured;
    }
  }
  // Deep freezing succeeded for all references. Hence we can leave this object
  // deep frozen and return success.
  return success();
}

value_t validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  value_mode_t mode = get_value_mode(value);
  if (mode == vmDeepFrozen) {
    return success();
  } else if (mode == vmFrozen) {
    // The object is frozen. We'll try deep freezing it.
    return transitively_validate_deep_frozen(runtime, value, offender_out);
  } else {
    if (offender_out != NULL)
      *offender_out = value;
    return new_not_deep_frozen_signal();
  }
}

value_t try_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  value_t ensured = validate_deep_frozen(runtime, value, offender_out);
  if (get_value_domain(ensured) == vdSignal) {
    if (is_signal(scNotDeepFrozen, ensured)) {
      // A NotFrozen signal indicates that there is something mutable somewhere
      // in the object graph.
      return internal_false_value();
    } else {
      // We got a different signal; propagate it.
      return ensured;
    }
  } else {
    // Non-signal so freezing must have succeeded.
    return internal_true_value();
  }
}


// --- S t r i n g ---

GET_FAMILY_PROTOCOL_IMPL(string);
FIXED_GET_MODE_IMPL(string, vmDeepFrozen);

size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kObjectHeaderSize               // header
       + kValueSize                      // length
       + align_size(kValueSize, bytes);  // contents
}

INTEGER_ACCESSORS_IMPL(String, string, Length, length);

char *get_string_chars(value_t value) {
  CHECK_FAMILY(ofString, value);
  return (char*) access_object_field(value, kStringCharsOffset);
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

void get_string_layout(value_t value, object_layout_t *layout) {
  // Strings have no value fields.
  size_t size = calc_string_size(get_string_length(value));
  object_layout_set(layout, size, size);
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
  return to_internal_boolean(string_equals(&a_contents, &b_contents));
}

value_t string_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofString, a);
  CHECK_FAMILY(ofString, b);
  string_t a_contents;
  get_string_contents(a, &a_contents);
  string_t b_contents;
  get_string_contents(b, &b_contents);
  return int_to_ordering(string_compare(&a_contents, &b_contents));
}

void string_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  if ((flags & pfUnquote) == 0)
    string_buffer_putc(buf, '"');
  string_buffer_append_string(buf, value);
  if ((flags & pfUnquote) == 0)
    string_buffer_putc(buf, '"');
}

void string_buffer_append_string(string_buffer_t *buf, value_t value) {
  CHECK_FAMILY(ofString, value);
  string_t contents;
  get_string_contents(value, &contents);
  string_buffer_printf(buf, "%s", contents.chars);
}

static value_t string_plus_string(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofString, this);
  CHECK_FAMILY(ofString, that);
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_t str;
  get_string_contents(this, &str);
  string_buffer_append(&buf, &str);
  get_string_contents(that, &str);
  string_buffer_append(&buf, &str);
  string_buffer_flush(&buf, &str);
  TRY_DEF(result, new_heap_string(get_builtin_runtime(args), &str));
  string_buffer_dispose(&buf);
  return result;
}

static value_t string_print(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  CHECK_FAMILY(ofString, this);
  print_ln("%v", this);
  return ROOT(args->runtime, nothing);
}

value_t add_string_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(string, INFIX("+"), 1, string_plus_string);
  ADD_BUILTIN(string, INFIX("print"), 0, string_print);
  return success();
}


// --- B l o b ---

GET_FAMILY_PROTOCOL_IMPL(blob);
NO_BUILTIN_METHODS(blob);
FIXED_GET_MODE_IMPL(blob, vmDeepFrozen);

size_t calc_blob_size(size_t size) {
  return kObjectHeaderSize              // header
       + kValueSize                     // length
       + align_size(kValueSize, size);  // contents
}

INTEGER_ACCESSORS_IMPL(Blob, blob, Length, length);

void get_blob_data(value_t value, blob_t *blob_out) {
  CHECK_FAMILY(ofBlob, value);
  size_t length = get_blob_length(value);
  byte_t *data = (byte_t*) access_object_field(value, kBlobDataOffset);
  blob_init(blob_out, data, length);
}

value_t blob_validate(value_t value) {
  VALIDATE_FAMILY(ofBlob, value);
  return success();
}

void get_blob_layout(value_t value, object_layout_t *layout) {
  // Blobs have no value fields.
  size_t size = calc_blob_size(get_blob_length(value));
  object_layout_set(layout, size, size);
}

void blob_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofBlob, value);
  string_buffer_printf(buf, "#<blob: [");
  blob_t blob;
  get_blob_data(value, &blob);
  size_t length = blob_length(&blob);
  size_t bytes_to_show = (length <= 8) ? length : 8;
  for (size_t i = 0; i < bytes_to_show; i++) {
    static const char *kChars = "0123456789abcdef";
    byte_t byte = blob_byte_at(&blob, i);
    string_buffer_printf(buf, "%c%c", kChars[byte >> 4], kChars[byte & 0xF]);
  }
  if (bytes_to_show < length)
    string_buffer_printf(buf, "...");
  string_buffer_printf(buf, "]>");
}


// --- V o i d   P ---

TRIVIAL_PRINT_ON_IMPL(VoidP, void_p);
FIXED_GET_MODE_IMPL(void_p, vmDeepFrozen);

void set_void_p_value(value_t value, void *ptr) {
  CHECK_FAMILY(ofVoidP, value);
  *access_object_field(value, kVoidPValueOffset) = pointer_to_value_bit_cast(ptr);
}

void *get_void_p_value(value_t value) {
  CHECK_FAMILY(ofVoidP, value);
  return value_to_pointer_bit_cast(*access_object_field(value, kVoidPValueOffset));
}

value_t void_p_validate(value_t value) {
  VALIDATE_FAMILY(ofVoidP, value);
  return success();
}

void get_void_p_layout(value_t value, object_layout_t *layout) {
  // A void-p has no value fields.
  object_layout_set(layout, kVoidPSize, kVoidPSize);
}


// --- A r r a y ---

GET_FAMILY_PROTOCOL_IMPL(array);

size_t calc_array_size(size_t length) {
  return kObjectHeaderSize       // header
       + kValueSize              // length
       + (length * kValueSize);  // contents
}

INTEGER_ACCESSORS_IMPL(Array, array, Length, length);

value_t get_array_at(value_t value, size_t index) {
  CHECK_FAMILY(ofArray, value);
  SIG_CHECK_TRUE("array index out of bounds", scOutOfBounds,
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
  return access_object_field(value, kArrayElementsOffset);
}

value_t array_validate(value_t value) {
  VALIDATE_FAMILY(ofArray, value);
  return success();
}

void get_array_layout(value_t value, object_layout_t *layout) {
  size_t size = calc_array_size(get_array_length(value));
  object_layout_set(layout, size, kArrayElementsOffset);
}

void array_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  if (depth == 1) {
    // If we can't print the elements anyway we might as well just show the
    // count.
    string_buffer_printf(buf, "#<array[%i]>", (int) get_array_length(value));
  } else {
    string_buffer_printf(buf, "[");
    for (size_t i = 0; i < get_array_length(value); i++) {
      if (i > 0)
        string_buffer_printf(buf, ", ");
      value_print_inner_on(get_array_at(value, i), buf, flags, depth - 1);
    }
    string_buffer_printf(buf, "]");
  }
}

// Compares two values pointed to by two void pointers.
static int value_compare_function(const void *vp_a, const void *vp_b) {
  value_t a = *((const value_t*) vp_a);
  value_t b = *((const value_t*) vp_b);
  value_t comparison = value_ordering_compare(a, b);
  CHECK_FALSE("not comparable", in_domain(vdSignal, comparison));
  return ordering_to_int(comparison);
}

value_t sort_array(value_t value) {
  CHECK_FAMILY(ofArray, value);
  CHECK_MUTABLE(value);
  size_t length = get_array_length(value);
  value_t *elements = get_array_elements(value);
  // Just use qsort. This means that we can't propagate signals from the compare
  // functions back out but that shouldn't be a huge issue. We'll check on them
  // for now and later on this will have to be rewritten in n anyway.
  qsort(elements, length, kValueSize, &value_compare_function);
  return success();
}

bool is_array_sorted(value_t value) {
  CHECK_FAMILY(ofArray, value);
  size_t length = get_array_length(value);
  for (size_t i = 1; i < length; i++) {
    value_t a = get_array_at(value, i - 1);
    value_t b = get_array_at(value, i);
    value_t comparison = value_ordering_compare(a, b);
    CHECK_FALSE("not comparable", in_domain(vdSignal, comparison));
    if (ordering_to_int(comparison) > 0)
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
    value_t comparison = value_ordering_compare(a, b);
    CHECK_FALSE("not comparable", in_domain(vdSignal, comparison));
    if (ordering_to_int(comparison) > 0)
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
    TRY_DEF(ordering_value, value_ordering_compare(current_key, key));
    int ordering = ordering_to_int(ordering_value);
    if (ordering < 0) {
      // The current key is smaller than the key we're looking for. Advance the
      // min past it.
      min = mid + 1;
    } else if (ordering > 0) {
      // The current key is larger than the key we're looking for. Move the max
      // below it.
      max = mid - 1;
    } else {
      return get_pair_array_second_at(self, mid);
    }
  }
  return new_not_found_signal();
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
    return internal_false_value();
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  for (size_t i = 0; i < length; i++) {
    value_t a_elm = get_array_at(a, i);
    value_t b_elm = get_array_at(b, i);
    TRY_DEF(cmp, value_identity_compare_cycle_protect(a_elm, b_elm, &inner));
    if (!is_internal_true_value(cmp))
      return cmp;
  }
  return internal_true_value();
}

value_t array_length(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofArray, self);
  return new_integer(get_array_length(self));
}

value_t add_array_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(array, INFIX("length"), 0, array_length);
  return success();
}


// --- A r r a y   b u f f e r ---

TRIVIAL_PRINT_ON_IMPL(ArrayBuffer, array_buffer);
GET_FAMILY_PROTOCOL_IMPL(array_buffer);
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

value_t get_array_buffer_at(value_t self, size_t index) {
  CHECK_FAMILY(ofArrayBuffer, self);
  SIG_CHECK_TRUE("array buffer index out of bounds", scOutOfBounds,
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


// --- I d e n t i t y   h a s h   m a p ---

GET_FAMILY_PROTOCOL_IMPL(id_hash_map);
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
  value_t null = ROOT(runtime, null);
  entry[kIdHashMapEntryKeyOffset] = ROOT(runtime, nothing);
  entry[kIdHashMapEntryHashOffset] = null;
  entry[kIdHashMapEntryValueOffset] = null;
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
    return new_signal(scMapFull);
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
    return new_not_found_signal();
  }
}

value_t get_id_hash_map_at_with_default(value_t map, value_t key, value_t defawlt) {
  value_t result = get_id_hash_map_at(map, key);
  return is_signal(scNotFound, result) ? defawlt : result;
}

bool has_id_hash_map_at(value_t map, value_t key) {
  return !is_signal(scNotFound, get_id_hash_map_at(map, key));
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
    return new_not_found_signal();
  }
}

void fixup_id_hash_map_post_migrate(runtime_t *runtime, value_t new_object,
    value_t old_object) {
  // In this fixup we rehash the migrated hash map since the hash values are
  // allowed to change during garbage collection. We do it by copying the
  // entries currently stored in the object into space that's left over by the
  // old one, clearing out the new entry array completely, and then re-adding
  // the entries one by one, reading them from the scratch storage. Dumb but it
  // works.

  // Get the raw entry array from the new map.
  value_t new_entry_array = get_id_hash_map_entry_array(new_object);
  size_t entry_array_length = get_array_length(new_entry_array);
  value_t *new_entries = get_array_elements(new_entry_array);
  // Get the raw entry array from the old map. This requires going directly
  // through the object since the nice accessors do sanity checking and the
  // state of the object at this point is, well, not sane.
  value_t old_entry_array = *access_object_field(old_object, kIdHashMapEntryArrayOffset);
  CHECK_DOMAIN(vdMovedObject, get_object_header(old_entry_array));
  value_t *old_entries = get_array_elements_unchecked(old_entry_array);
  // Copy the contents of the new entry array into the old one and clear it as
  // we go so it's ready to have elements added back.
  value_t null = ROOT(runtime, null);
  for (size_t i = 0; i < entry_array_length; i++) {
    old_entries[i] = new_entries[i];
    new_entries[i] = null;
  }
  // Reset the map's fields. It is now empty.
  set_id_hash_map_size(new_object, 0);
  set_id_hash_map_occupied_count(new_object, 0);
  // Fake an iterator that scans over the old array.
  id_hash_map_iter_t iter;
  iter.entries = old_entries;
  iter.cursor = 0;
  iter.capacity = get_id_hash_map_capacity(new_object);
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
    value_t added = try_set_id_hash_map_at(new_object, key, value, true);
    CHECK_FALSE("rehash failed to set", get_value_domain(added) == vdSignal);
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

void id_hash_map_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  if (depth == 1) {
    string_buffer_printf(buf, "#<map{%i}>", get_id_hash_map_size(value));
  } else {
    string_buffer_printf(buf, "{");
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, value);
    bool is_first = true;
    while (id_hash_map_iter_advance(&iter)) {
      if (is_first) {
        is_first = false;
      } else {
        string_buffer_printf(buf, ", ");
      }
      value_t key;
      value_t value;
      id_hash_map_iter_get_current(&iter, &key, &value);
      value_print_inner_on(key, buf, flags, depth - 1);
      string_buffer_printf(buf, ": ");
      value_print_inner_on(value, buf, flags, depth - 1);
    }
    string_buffer_printf(buf, "}");
  }
}


// --- N u l l ---

GET_FAMILY_PROTOCOL_IMPL(null);
NO_BUILTIN_METHODS(null);
FIXED_GET_MODE_IMPL(null, vmDeepFrozen);

value_t null_validate(value_t value) {
  VALIDATE_FAMILY(ofNull, value);
  return success();
}

void null_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "null");
}


// --- N o t h i n g ---

FIXED_GET_MODE_IMPL(nothing, vmDeepFrozen);

value_t nothing_validate(value_t value) {
  VALIDATE_FAMILY(ofNothing, value);
  return success();
}

void nothing_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<nothing>");
}


// --- B o o l e a n ---

GET_FAMILY_PROTOCOL_IMPL(boolean);
NO_BUILTIN_METHODS(boolean);
FIXED_GET_MODE_IMPL(boolean, vmDeepFrozen);

void set_boolean_value(value_t value, bool truth) {
  CHECK_FAMILY(ofBoolean, value);
  *access_object_field(value, kBooleanValueOffset) = new_integer(truth ? 1 : 0);
}

bool get_boolean_value(value_t value) {
  CHECK_FAMILY(ofBoolean, value);
  return get_integer_value(*access_object_field(value, kBooleanValueOffset));
}

value_t boolean_validate(value_t value) {
  VALIDATE_FAMILY(ofBoolean, value);
  bool which = get_boolean_value(value);
  VALIDATE((which == true) || (which == false));
  return success();
}

value_t boolean_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofBoolean, a);
  CHECK_FAMILY(ofBoolean, b);
  return new_integer(get_boolean_value(a) - get_boolean_value(b));
}

void boolean_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, get_boolean_value(value) ? "true" : "false");
}


// --- K e y ---

GET_FAMILY_PROTOCOL_IMPL(key);
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
  return to_internal_boolean(a_id == b_id);
}

value_t key_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofKey, a);
  CHECK_FAMILY(ofKey, b);
  return new_integer(get_key_id(a) - get_key_id(b));
}

void key_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  value_t display_name = get_key_display_name(value);
  if (is_nothing(display_name))
    display_name = new_integer(get_key_id(value));
  string_buffer_printf(buf, "%%");
  value_print_inner_on(display_name, buf, flags | pfUnquote, depth - 1);
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

void instance_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofInstance, value);
  string_buffer_printf(buf, "#<instance of ");
  value_print_inner_on(get_instance_primary_protocol(value), buf, flags,
      depth - 1);
  string_buffer_printf(buf, ": ");
  value_print_inner_on(get_instance_fields(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_instance_contents(value_t instance, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  set_instance_fields(instance, contents);
  return success();
}

value_t get_instance_protocol(value_t self, runtime_t *runtime) {
  return get_instance_primary_protocol(self);
}

value_mode_t get_instance_mode(value_t self) {
  return vmMutable;
}

value_t set_instance_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode) {
  // TODO: implement this.
  UNREACHABLE("setting instance mode not implemented");
  return new_invalid_mode_change_signal(get_instance_mode(self));
}


// --- F a c t o r y ---

ACCESSORS_IMPL(Factory, factory, acInFamily, ofVoidP, Constructor, constructor);
FIXED_GET_MODE_IMPL(factory, vmDeepFrozen);

value_t factory_validate(value_t value) {
  VALIDATE_FAMILY(ofFactory, value);
  VALIDATE_FAMILY(ofVoidP, get_factory_constructor(value));
  return success();
}

void factory_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofFactory, value);
  string_buffer_printf(buf, "#<factory: ");
  value_print_inner_on(get_factory_constructor(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
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

void code_block_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  CHECK_FAMILY(ofCodeBlock, value);
  string_buffer_printf(buf, "#<code block: bc@%i, vp@%i>",
      get_blob_length(get_code_block_bytecode(value)),
      get_array_length(get_code_block_value_pool(value)));
}

value_t ensure_code_block_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_code_block_value_pool(self)));
  return success();
}


// --- P r o t o c o l ---

GET_FAMILY_PROTOCOL_IMPL(protocol);
NO_BUILTIN_METHODS(protocol);

ACCESSORS_IMPL(Protocol, protocol, acNoCheck, 0, DisplayName, display_name);

value_t protocol_validate(value_t value) {
  VALIDATE_FAMILY(ofProtocol, value);
  return success();
}

void protocol_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofProtocol, value);
  value_t display_name = get_protocol_display_name(value);
  string_buffer_printf(buf, "#<protocol");
  if (!is_null(display_name)) {
    string_buffer_printf(buf, " ");
    value_print_inner_on(display_name, buf, flags | pfUnquote, depth - 1);
  }
  string_buffer_printf(buf, ">");
}

value_t plankton_new_protocol(runtime_t *runtime) {
  return new_heap_protocol(runtime, afMutable, ROOT(runtime, nothing));
}

value_t plankton_set_protocol_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_protocol_display_name(object, name);
  return success();
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
    TRY(add_to_array_buffer(runtime, children, ROOT(runtime, null)));
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


// --- L a m b d a ---

GET_FAMILY_PROTOCOL_IMPL(lambda);
TRIVIAL_PRINT_ON_IMPL(Lambda, lambda);

ACCESSORS_IMPL(Lambda, lambda, acInFamilyOpt, ofMethodspace, Methods, methods);
ACCESSORS_IMPL(Lambda, lambda, acInFamilyOpt, ofArray, Outers, outers);

value_t lambda_validate(value_t self) {
  VALIDATE_FAMILY(ofLambda, self);
  VALIDATE_FAMILY_OPT(ofMethodspace, get_lambda_methods(self));
  VALIDATE_FAMILY_OPT(ofArray, get_lambda_outers(self));
  return success();
}

value_t emit_lambda_call_trampoline(assembler_t *assm) {
  TRY(assembler_emit_delegate_lambda_call(assm));
  return success();
}

value_t add_lambda_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  TRY(add_methodspace_custom_method(runtime, deref(s_space),
      ROOT(runtime, lambda_protocol), OPERATION(otCall, NULL), 0, true,
      emit_lambda_call_trampoline));
  return success();
}

value_t get_lambda_outer(value_t self, size_t index) {
  CHECK_FAMILY(ofLambda, self);
  value_t outers = get_lambda_outers(self);
  return get_array_at(outers, index);
}

value_t ensure_lambda_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_lambda_outers(self)));
  return success();
}


// --- N a m e s p a c e ---

ACCESSORS_IMPL(Namespace, namespace, acInFamilyOpt, ofIdHashMap, Bindings, bindings);

value_t namespace_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespace, self);
  VALIDATE_FAMILY(ofIdHashMap, get_namespace_bindings(self));
  return success();
}

value_t plankton_set_namespace_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, bindings);
  set_namespace_bindings(object, bindings);
  return success();
}

value_t plankton_new_namespace(runtime_t *runtime) {
  return new_heap_namespace(runtime);
}

value_t get_namespace_binding_at(value_t namespace, value_t name) {
  value_t bindings = get_namespace_bindings(namespace);
  value_t result = get_id_hash_map_at(bindings, name);
  if (is_signal(scNotFound, result))
    return new_lookup_error_signal(lcNamespace);
  return result;
}

value_t set_namespace_binding_at(runtime_t *runtime, value_t namespace,
    value_t name, value_t value) {
  value_t bindings = get_namespace_bindings(namespace);
  return set_id_hash_map_at(runtime, bindings, name, value);
}

value_t ensure_namespace_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_namespace_bindings(self)));
  return success();
}

void namespace_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofNamespace, value);
  string_buffer_printf(buf, "#<namespace ");
  value_print_inner_on(get_namespace_bindings(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- M o d u l e---

FIXED_GET_MODE_IMPL(module, vmMutable);

ACCESSORS_IMPL(Module, module, acInFamily, ofPath, Path, path);
ACCESSORS_IMPL(Module, module, acInFamily, ofArrayBuffer, Fragments, fragments);

value_t module_validate(value_t self) {
  VALIDATE_FAMILY(ofModule, self);
  VALIDATE_FAMILY(ofPath, get_module_path(self));
  VALIDATE_FAMILY(ofArrayBuffer, get_module_fragments(self));
  return success();
}

void module_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofModule, value);
  string_buffer_printf(buf, "<module>");
}

value_t get_module_fragment_at(value_t self, value_t stage) {
  value_t fragments = get_module_fragments(self);
  for (size_t i = 0; i < get_array_buffer_length(fragments); i++) {
    value_t fragment = get_array_buffer_at(fragments, i);
    if (is_same_value(get_module_fragment_stage(fragment), stage))
      return fragment;
  }
  return new_not_found_signal();
}

value_t get_module_fragment_before(value_t self, value_t stage) {
  int32_t limit = get_integer_value(stage);
  value_t fragments = get_module_fragments(self);
  int32_t best_stage = kMostNegativeInt32;
  value_t best_fragment = new_not_found_signal();
  for (size_t i = 0; i < get_array_buffer_length(fragments); i++) {
    value_t fragment = get_array_buffer_at(fragments, i);
    int32_t fragment_stage = get_integer_value(get_module_fragment_stage(fragment));
    if (fragment_stage < limit && fragment_stage > best_stage) {
      best_stage = fragment_stage;
      best_fragment = fragment;
    }
  }
  return best_fragment;
}

bool has_module_fragment_at(value_t self, value_t stage) {
  return !is_signal(scNotFound, get_module_fragment_at(self, stage));
}

value_t module_lookup_identifier(runtime_t *runtime, value_t self, value_t ident) {
  CHECK_FAMILY(ofIdentifier, ident);
  value_t stage = get_identifier_stage(ident);
  value_t fragments = get_module_fragments(self);
  value_t path = get_identifier_path(ident);
  value_t head = get_path_head(path);
  if (value_identity_compare(head, RSTR(runtime, ctrino)))
    return ROOT(runtime, ctrino);
  for (size_t i = 0; i < get_array_buffer_length(fragments); i++) {
    value_t fragment = get_array_buffer_at(fragments, i);
    if (is_same_value(get_module_fragment_stage(fragment), stage)) {
      value_t namespace = get_module_fragment_namespace(fragment);
      return get_namespace_binding_at(namespace, head);
    }
  }
  WARN("Couldn't find fragment when looking up %v", ident);
  return new_lookup_error_signal(lcNoSuchStage);
}


// --- M o d u l e   f r a g m e n t ---

GET_FAMILY_PROTOCOL_IMPL(module_fragment);

ACCESSORS_IMPL(ModuleFragment, module_fragment, acNoCheck, 0, Stage, stage);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofModule,
    Module, module);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofNamespace,
    Namespace, namespace);
ACCESSORS_IMPL(ModuleFragment, module_fragment, acInFamilyOpt, ofMethodspace,
    Methodspace, methodspace);

void set_module_fragment_epoch(value_t self, module_fragment_epoch_t value) {
  *access_object_field(self, kModuleFragmentEpochOffset) = new_integer(value);
}

module_fragment_epoch_t get_module_fragment_epoch(value_t self) {
  value_t epoch = *access_object_field(self, kModuleFragmentEpochOffset);
  return (module_fragment_epoch_t) get_integer_value(epoch);
}

value_t module_fragment_validate(value_t value) {
  VALIDATE_FAMILY(ofModuleFragment, value);
  VALIDATE_FAMILY_OPT(ofNamespace, get_module_fragment_namespace(value));
  VALIDATE_FAMILY_OPT(ofMethodspace, get_module_fragment_methodspace(value));
  return success();
}

void module_fragment_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofModuleFragment, value);
  string_buffer_printf(buf, "<module fragment>");
}

static value_t module_fragment_print(builtin_arguments_t *args) {
  value_t this = get_builtin_subject(args);
  print_ln("%v", this);
  return ROOT(args->runtime, nothing);
}

value_t add_module_fragment_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(module_fragment, INFIX("print"), 0, module_fragment_print);
  return success();
}

bool is_module_fragment_bound(value_t fragment) {
  return get_module_fragment_epoch(fragment) == feComplete;
}


// --- P a t h ---

GET_FAMILY_PROTOCOL_IMPL(path);
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
  return new_heap_path(runtime, afMutable, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}

value_t plankton_set_path_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, names);
  if (get_array_length(names) == 0) {
    CHECK_TRUE("new path was non-empty", is_path_empty(object));
  } else {
    value_t head = get_array_at(names, 0);
    TRY_DEF(tail, new_heap_path_with_names(runtime, names, 1));
    set_path_raw_head(object, head);
    set_path_raw_tail(object, tail);
  }
  return success();
}

value_t get_path_head(value_t path) {
  value_t raw_head = get_path_raw_head(path);
  SIG_CHECK_TRUE("head of empty path", scEmptyPath, !is_nothing(raw_head));
  return raw_head;
}

value_t get_path_tail(value_t path) {
  value_t raw_tail = get_path_raw_tail(path);
  SIG_CHECK_TRUE("tail of empty path", scEmptyPath, !is_nothing(raw_tail));
  return raw_tail;
}

void path_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofPath, value);
  if (is_path_empty(value)) {
    string_buffer_printf(buf, "#<empty path>");
  } else {
    value_t current = value;
    while (!is_path_empty(current)) {
      string_buffer_printf(buf, ":");
      value_print_inner_on(get_path_head(current), buf, flags | pfUnquote,
          depth - 1);
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
  if (!is_internal_true_value(cmp_head))
    return cmp_head;
  // TODO: As above, this should be done iteratively.
  value_t a_tail = get_path_raw_tail(a);
  value_t b_tail = get_path_raw_tail(b);
  return value_identity_compare_cycle_protect(a_tail, b_tail, &inner);
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
  return new_heap_identifier(runtime, ROOT(runtime, nothing), ROOT(runtime, nothing));
}

value_t plankton_set_identifier_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, path, stage);
  set_identifier_path(object, path);
  set_identifier_stage(object, stage);
  return success();
}

void identifier_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofIdentifier, value);
  int32_t stage = get_integer_value(get_identifier_stage(value));
  if (stage < 0) {
    for (int32_t i = stage; i < 0; i++)
      string_buffer_putc(buf, '@');
  } else {
    for (int32_t i = 0; i <= stage; i++)
      string_buffer_putc(buf, '$');
  }
  value_print_inner_on(get_identifier_path(value), buf, flags, depth - 1);
}


// --- F u n c t i o n ---

GET_FAMILY_PROTOCOL_IMPL(function);
NO_BUILTIN_METHODS(function);

ACCESSORS_IMPL(Function, function, acNoCheck, 0, DisplayName, display_name);

value_t function_validate(value_t self) {
  VALIDATE_FAMILY(ofFunction, self);
  return success();
}

value_t plankton_new_function(runtime_t *runtime) {
  return new_heap_function(runtime, afMutable, ROOT(runtime, nothing));
}

value_t plankton_set_function_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, display_name);
  set_function_display_name(object, display_name);
  return success();
}

void function_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<function");
  value_t display_name = get_function_display_name(value);
  if (!is_nothing(display_name)) {
    string_buffer_printf(buf, " ");
    value_print_inner_on(display_name, buf, flags | pfUnquote, depth - 1);
  }
  string_buffer_printf(buf, ">");
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

void unknown_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<? ");
  value_t header = get_unknown_header(value);
  value_print_inner_on(header, buf, flags, depth - 1);
  string_buffer_printf(buf, " ");
  value_t payload = get_unknown_payload(value);
  value_print_inner_on(payload, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- O p t i o n s ---

FIXED_GET_MODE_IMPL(options, vmMutable);

ACCESSORS_IMPL(Options, options, acInFamilyOpt, ofArray, Elements, elements);

value_t plankton_new_options(runtime_t *runtime) {
  return new_heap_options(runtime, ROOT(runtime, nothing));
}

value_t plankton_set_options_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, elements);
  set_options_elements(object, elements);
  return success();
}

value_t options_validate(value_t self) {
  VALIDATE_FAMILY(ofOptions, self);
  return success();
}

void options_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<options ");
  value_t elements = get_options_elements(value);
  value_print_inner_on(elements, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
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
    if (is_signal(scNotFound, element_key) || !value_identity_compare(element_key, key))
      continue;
    value_t element_value = get_id_hash_map_at(payload, RSTR(runtime, value));
    if (!is_signal(scNotFound, element_value))
      return element_value;
  }
  return defawlt;
}


// --- O r d e r i n g ---

int ordering_to_int(value_t value) {
  return get_integer_value(value);
}

value_t int_to_ordering(int value) {
  return new_integer(value);
}


// --- M i s c ---

// Adds a binding to the given plankton environment map.
static value_t add_plankton_binding(value_t map, value_t category, const char *name,
    value_t value, runtime_t *runtime) {
  string_t key_str;
  string_init(&key_str, name);
  // Build the key, [category, name].
  TRY_DEF(name_obj, new_heap_string(runtime, &key_str));
  TRY_DEF(key_obj, new_heap_array(runtime, 2));
  set_array_at(key_obj, 0, category);
  set_array_at(key_obj, 1, name_obj);
  TRY(ensure_frozen(runtime, key_obj));
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
  TRY(add_plankton_factory(map, core, "Function", plankton_new_function, runtime));
  TRY(add_plankton_factory(map, core, "Identifier", plankton_new_identifier, runtime));
  TRY(add_plankton_factory(map, core, "Library", plankton_new_library, runtime));
  TRY(add_plankton_factory(map, core, "Namespace", plankton_new_namespace, runtime));
  TRY(add_plankton_factory(map, core, "Operation", plankton_new_operation, runtime));
  TRY(add_plankton_factory(map, core, "Path", plankton_new_path, runtime));
  TRY(add_plankton_factory(map, core, "Protocol", plankton_new_protocol, runtime));
  TRY(add_plankton_factory(map, core, "UnboundModule", plankton_new_unbound_module, runtime));
  TRY(add_plankton_factory(map, core, "UnboundModuleFragment", plankton_new_unbound_module_fragment, runtime));
  // Singletons
  TRY(add_plankton_binding(map, core, "ctrino", ROOT(runtime, ctrino), runtime));
  TRY(add_plankton_binding(map, core, "subject", ROOT(runtime, subject_key),
      runtime));
  TRY(add_plankton_binding(map, core, "selector", ROOT(runtime, selector_key),
      runtime));
  TRY(add_plankton_binding(map, core, "builtin_methodspace",
      ROOT(runtime, builtin_methodspace), runtime));
  // Protocols
  value_t protocol = RSTR(runtime, protocol);
  TRY(add_plankton_binding(map, protocol, "Integer", ROOT(runtime, integer_protocol),
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
