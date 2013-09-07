// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "heap.h"
#include "interp.h"
#include "runtime.h"
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
#define __GEN_CASE__(Family, family, CMP, CID, CNT, SUR, NOL, FIX, EMT)        \
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

#define ADD_BUILTIN(family, name, argc, impl)                                  \
  TRY(add_method_space_builtin_method(runtime, space,                          \
      runtime->roots.family##_protocol, name, argc, impl))

static value_t integer_plus_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_this(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(this.encoded + that.encoded);
}

static value_t integer_minus_integer(builtin_arguments_t *args) {
  value_t this = get_builtin_this(args);
  value_t that = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, this);
  CHECK_DOMAIN(vdInteger, that);
  return decode_value(this.encoded - that.encoded);
}

static value_t integer_negate(builtin_arguments_t *args) {
  value_t this = get_builtin_this(args);
  CHECK_DOMAIN(vdInteger, this);
  return decode_value(-this.encoded);
}

value_t add_integer_builtin_methods(runtime_t *runtime, value_t space) {
  ADD_BUILTIN(integer, "+", 1, integer_plus_integer);
  ADD_BUILTIN(integer, "-", 1, integer_minus_integer);
  ADD_BUILTIN(integer, "-", 0, integer_negate);
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

value_t species_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofSpecies, value);
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
  object_layout_set(layout, kInstanceSpeciesSize, kSpeciesHeaderSize);
}

CHECKED_SPECIES_ACCESSORS_IMPL(Instance, instance, Instance, instance, Protocol,
    PrimaryProtocol, primary_protocol);


// --- S t r i n g ---

GET_FAMILY_PROTOCOL_IMPL(string);

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
  VALIDATE_VALUE_FAMILY(ofString, value);
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

value_t string_transient_identity_hash(value_t value, size_t depth) {
  string_t contents;
  get_string_contents(value, &contents);
  size_t hash = string_hash(&contents);
  return new_integer(hash);
}

value_t string_identity_compare(value_t a, value_t b, size_t depth) {
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

void string_print_on(value_t value, string_buffer_t *buf) {
  string_print_atomic_on(value, buf);
}

void string_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofString, value);
  string_t contents;
  get_string_contents(value, &contents);
  string_buffer_printf(buf, "\"%s\"", contents.chars);
}

static value_t string_plus_string(builtin_arguments_t *args) {
  value_t this = get_builtin_this(args);
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
  value_t this = get_builtin_this(args);
  CHECK_FAMILY(ofString, this);
  value_print_ln(this);
  return runtime_nothing(args->runtime);
}

value_t add_string_builtin_methods(runtime_t *runtime, value_t space) {
  ADD_BUILTIN(string, "+", 1, string_plus_string);
  // TODO: change this to a more appropriate name -- say ".print".
  ADD_BUILTIN(string, "()", 0, string_print);
  return success();
}


// --- B l o b ---

GET_FAMILY_PROTOCOL_IMPL(blob);
NO_BUILTIN_METHODS(blob);

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
  VALIDATE_VALUE_FAMILY(ofBlob, value);
  return success();
}

void get_blob_layout(value_t value, object_layout_t *layout) {
  // Blobs have no value fields.
  size_t size = calc_blob_size(get_blob_length(value));
  object_layout_set(layout, size, size);
}

void blob_print_on(value_t value, string_buffer_t *buf) {
  blob_print_atomic_on(value, buf);
}

void blob_print_atomic_on(value_t value, string_buffer_t *buf) {
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

void set_void_p_value(value_t value, void *ptr) {
  CHECK_FAMILY(ofVoidP, value);
  *access_object_field(value, kVoidPValueOffset) = pointer_to_value_bit_cast(ptr);
}

void *get_void_p_value(value_t value) {
  CHECK_FAMILY(ofVoidP, value);
  return value_to_pointer_bit_cast(*access_object_field(value, kVoidPValueOffset));
}

value_t void_p_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofVoidP, value);
  return success();
}

void get_void_p_layout(value_t value, object_layout_t *layout) {
  // A void-p has no value fields.
  object_layout_set(layout, kVoidPSize, kVoidPSize);
}


// --- A r r a y ---

GET_FAMILY_PROTOCOL_IMPL(array);
NO_BUILTIN_METHODS(array);

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
  CHECK_TRUE("array index out of bounds", index < get_array_length(value));
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
  VALIDATE_VALUE_FAMILY(ofArray, value);
  return success();
}

void get_array_layout(value_t value, object_layout_t *layout) {
  size_t size = calc_array_size(get_array_length(value));
  object_layout_set(layout, size, kArrayElementsOffset);
}

void array_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "[");
  for (size_t i = 0; i < get_array_length(value); i++) {
    if (i > 0)
      string_buffer_printf(buf, ", ");
    value_print_atomic_on(get_array_at(value, i), buf);
  }
  string_buffer_printf(buf, "]");
}

void array_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<array[%i]>", (int) get_array_length(value));
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
  CHECK_TRUE("not pair array", (get_array_length(value) & 1) == 0);
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
  CHECK_TRUE("not pair array", (get_array_length(self) & 1) == 0);
  // Using signed values allows the max to go negative which simplifies the
  // logic. It does in principle narrow the range we can represent but the
  // longest possible pair array is half the length of the longest possible
  // array, which gives us the sign bit WLOG.
  int64_t min = 0;
  int64_t max = get_pair_array_length(self) - 1;
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
  return new_signal(scNotFound);
}

value_t array_transient_identity_hash(value_t value, size_t depth) {
  if (depth > kCircularObjectDepthThreshold)
    return new_signal(scMaybeCircular);
  size_t length = get_array_length(value);
  int64_t result = length;
  for (size_t i = 0; i < length; i++) {
    value_t elm = get_array_at(value, i);
    TRY_DEF(hash, value_transient_identity_hash_cycle_protect(elm, depth + 1));
    result = (result << 8) | (result >> 24) | get_integer_value(hash);
  }
  return new_integer(result & ((1LL << 61) - 1LL));
}

value_t array_identity_compare(value_t a, value_t b, size_t depth) {
  size_t length = get_array_length(a);
  size_t b_length = get_array_length(b);
  if (length != b_length)
    return internal_false_value();
  // Wait as long as possible before doing this check since it's the uncommon
  // case.
  if (depth > kCircularObjectDepthThreshold)
    return new_signal(scMaybeCircular);
  for (size_t i = 0; i < length; i++) {
    value_t a_elm = get_array_at(a, i);
    value_t b_elm = get_array_at(b, i);
    TRY_DEF(cmp, value_identity_compare_cycle_protect(a_elm, b_elm, depth + 1));
    if (!is_internal_true_value(cmp))
      return cmp;
  }
  return internal_true_value();
}


// --- A r r a y   b u f f e r ---

TRIVIAL_PRINT_ON_IMPL(ArrayBuffer, array_buffer);
GET_FAMILY_PROTOCOL_IMPL(array_buffer);
NO_BUILTIN_METHODS(array_buffer);

CHECKED_ACCESSORS_IMPL(ArrayBuffer, array_buffer, Array, Elements, elements);
INTEGER_ACCESSORS_IMPL(ArrayBuffer, array_buffer, Length, length);

value_t array_buffer_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArrayBuffer, value);
  return success();
}

bool try_add_to_array_buffer(value_t self, value_t value) {
  CHECK_FAMILY(ofArrayBuffer, self);
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
  CHECK_TRUE("array buffer index out of bounds",
      index < get_array_buffer_length(self));
  set_array_at(get_array_buffer_elements(self), index, value);
}


// --- I d e n t i t y   h a s h   m a p ---

GET_FAMILY_PROTOCOL_IMPL(id_hash_map);
NO_BUILTIN_METHODS(id_hash_map);

CHECKED_ACCESSORS_IMPL(IdHashMap, id_hash_map, Array, EntryArray, entry_array);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Size, size);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Capacity, capacity);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, OccupiedCount, occupied_count);

// Returns a pointer to the start of the index'th entry in the given map.
static value_t *get_id_hash_map_entry(value_t map, size_t index) {
  CHECK_TRUE("map entry out of bounds", index < get_id_hash_map_capacity(map));
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
  value_t null = runtime_null(runtime);
  entry[kIdHashMapEntryKeyOffset] = runtime_nothing(runtime);
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
  CHECK_TRUE("map overfull", get_id_hash_map_size(map) < capacity);
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

value_t try_set_id_hash_map_at(value_t map, value_t key, value_t value) {
  CHECK_FAMILY(ofIdHashMap, map);
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
    return new_signal(scNotFound);
  }
}

bool has_id_hash_map_at(value_t map, value_t key) {
  return !is_signal(scNotFound, get_id_hash_map_at(map, key));
}


value_t delete_id_hash_map_at(runtime_t *runtime, value_t map, value_t key) {
  CHECK_FAMILY(ofIdHashMap, map);
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
    return new_signal(scNotFound);
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
  value_t null = runtime_null(runtime);
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
    value_t added = try_set_id_hash_map_at(new_object, key, value);
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
  VALIDATE_VALUE_FAMILY(ofIdHashMap, value);
  value_t entry_array = get_id_hash_map_entry_array(value);
  VALIDATE_VALUE_FAMILY(ofArray, entry_array);
  size_t capacity = get_id_hash_map_capacity(value);
  VALIDATE(get_id_hash_map_size(value) < capacity);
  VALIDATE(get_array_length(entry_array) == (capacity * kIdHashMapEntryFieldCount));
  return success();
}

void id_hash_map_print_on(value_t value, string_buffer_t *buf) {
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
    value_print_on(key, buf);
    string_buffer_printf(buf, ": ");
    value_print_on(value, buf);
  }
  string_buffer_printf(buf, "}");
}

void id_hash_map_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<map{%i}>", (int) get_id_hash_map_size(value));
}


// --- N u l l ---

GET_FAMILY_PROTOCOL_IMPL(null);
NO_BUILTIN_METHODS(null);

value_t null_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNull, value);
  return success();
}

value_t null_transient_identity_hash(value_t value, size_t depth) {
  static const size_t kNullHash = 0x4323;
  return new_integer(kNullHash);
}

value_t null_identity_compare(value_t a, value_t b, size_t depth) {
  // There is only one null so you should never end up comparing two different
  // ones.
  CHECK_EQ("multiple nulls", a.encoded, b.encoded);
  return internal_true_value();
}

void null_print_on(value_t value, string_buffer_t *buf) {
  null_print_atomic_on(value, buf);
}

void null_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "null");
}


// --- N u l l ---

value_t nothing_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNothing, value);
  return success();
}

void nothing_print_on(value_t value, string_buffer_t *buf) {
  nothing_print_atomic_on(value, buf);
}

void nothing_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<nothing>");
}


// --- B o o l e a n ---

GET_FAMILY_PROTOCOL_IMPL(boolean);
NO_BUILTIN_METHODS(boolean);

void set_boolean_value(value_t value, bool truth) {
  CHECK_FAMILY(ofBoolean, value);
  *access_object_field(value, kBooleanValueOffset) = new_integer(truth ? 1 : 0);
}

bool get_boolean_value(value_t value) {
  CHECK_FAMILY(ofBoolean, value);
  return get_integer_value(*access_object_field(value, kBooleanValueOffset));
}

value_t boolean_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofBoolean, value);
  bool which = get_boolean_value(value);
  VALIDATE((which == true) || (which == false));
  return success();
}

value_t boolean_transient_identity_hash(value_t value, size_t depth) {
  static const size_t kTrueHash = 0x3213;
  static const size_t kFalseHash = 0x5423;
  return new_integer(get_boolean_value(value) ? kTrueHash : kFalseHash);
}

value_t boolean_identity_compare(value_t a, value_t b, size_t depth) {
  // There is only one true and false which are both only equal to themselves.
  return to_internal_boolean(a.encoded == b.encoded);
}

value_t boolean_ordering_compare(value_t a, value_t b) {
  CHECK_FAMILY(ofBoolean, a);
  CHECK_FAMILY(ofBoolean, b);
  return new_integer(get_boolean_value(a) - get_boolean_value(b));
}

void boolean_print_on(value_t value, string_buffer_t *buf) {
  boolean_print_atomic_on(value, buf);
}

void boolean_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, get_boolean_value(value) ? "true" : "false");
}


// --- I n s t a n c e ---

CHECKED_ACCESSORS_IMPL(Instance, instance, IdHashMap, Fields, fields);
NO_BUILTIN_METHODS(instance);

value_t get_instance_field(value_t value, value_t key) {
  value_t fields = get_instance_fields(value);
  return get_id_hash_map_at(fields, key);
}

value_t try_set_instance_field(value_t instance, value_t key, value_t value) {
  value_t fields = get_instance_fields(instance);
  return try_set_id_hash_map_at(fields, key, value);
}

value_t instance_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofInstance, value);
  value_t fields = get_instance_fields(value);
  VALIDATE_VALUE_FAMILY(ofIdHashMap, fields);
  return success();
}

void instance_print_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofInstance, value);
  string_buffer_printf(buf, "#<instance of ");
  value_print_atomic_on(get_instance_primary_protocol(value), buf);
  string_buffer_printf(buf, ": ");
  value_print_on(get_instance_fields(value), buf);
  string_buffer_printf(buf, ">");
}

void instance_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofInstance, value);
  string_buffer_printf(buf, "#<instance>");
}

value_t set_instance_contents(value_t instance, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  set_instance_fields(instance, contents);
  return success();
}

value_t get_instance_protocol(value_t self, runtime_t *runtime) {
  return get_instance_primary_protocol(self);
}


// --- F a c t o r y ---

CHECKED_ACCESSORS_IMPL(Factory, factory, VoidP, Constructor, constructor);

value_t factory_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofFactory, value);
  value_t constructor = get_factory_constructor(value);
  VALIDATE_VALUE_FAMILY(ofVoidP, constructor);
  return success();
}

void factory_print_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofFactory, value);
  string_buffer_printf(buf, "#<factory: ");
  value_print_on(get_factory_constructor(value), buf);
  string_buffer_printf(buf, ">");
}

void factory_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofFactory, value);
  string_buffer_printf(buf, "#<factory>");
}


// --- C o d e   b l o c k ---

CHECKED_ACCESSORS_IMPL(CodeBlock, code_block, Blob, Bytecode, bytecode);
CHECKED_ACCESSORS_IMPL(CodeBlock, code_block, Array, ValuePool, value_pool);
INTEGER_ACCESSORS_IMPL(CodeBlock, code_block, HighWaterMark, high_water_mark);

value_t code_block_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofCodeBlock, value);
  VALIDATE_VALUE_FAMILY(ofBlob, get_code_block_bytecode(value));
  VALIDATE_VALUE_FAMILY(ofArray, get_code_block_value_pool(value));
  return success();
}

void code_block_print_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofCodeBlock, value);
  string_buffer_printf(buf, "#<code block: bc@%i, vp@%i>",
      get_blob_length(get_code_block_bytecode(value)),
      get_array_length(get_code_block_value_pool(value)));
}

void code_block_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofCodeBlock, value);
  string_buffer_printf(buf, "#<code block>");
}


// --- P r o t o c o l ---

GET_FAMILY_PROTOCOL_IMPL(protocol);
NO_BUILTIN_METHODS(protocol);

UNCHECKED_ACCESSORS_IMPL(Protocol, protocol, DisplayName, display_name);

value_t protocol_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofProtocol, value);
  return success();
}

void protocol_print_on(value_t value, string_buffer_t *buf) {
  protocol_print_atomic_on(value, buf);
}

void protocol_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofProtocol, value);
  value_t display_name = get_protocol_display_name(value);
  if (is_null(display_name) || in_family(ofProtocol, display_name)) {
    string_buffer_printf(buf, "#<protocol>");
  } else {
    // We print the display name even though it's strictly against the rules
    // for an atomic print function, but since we've checked that it isn't
    // a protocol itself it shouldn't be possible to end up in a cycle and it
    // makes debugging easier.
    string_buffer_printf(buf, "#<protocol: ");
    value_print_atomic_on(display_name, buf);
    string_buffer_printf(buf, ">");
  }
}


// --- A r g u m e n t   m a p   t r i e ---

NO_BUILTIN_METHODS(argument_map_trie);
TRIVIAL_PRINT_ON_IMPL(ArgumentMapTrie, argument_map_trie);

CHECKED_ACCESSORS_IMPL(ArgumentMapTrie, argument_map_trie, Array, Value, value);
CHECKED_ACCESSORS_IMPL(ArgumentMapTrie, argument_map_trie, ArrayBuffer, Children, children);

value_t argument_map_trie_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArgumentMapTrie, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_argument_map_trie_value(value));
  VALIDATE_VALUE_FAMILY(ofArrayBuffer, get_argument_map_trie_children(value));
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
    TRY(add_to_array_buffer(runtime, children, runtime_null(runtime)));
  // Create the new child.
  value_t old_value = get_argument_map_trie_value(self);
  size_t old_length = get_array_length(old_value);
  size_t new_length = old_length + 1;
  TRY_DEF(new_value, new_heap_array(runtime, new_length));
  for (size_t i = 0; i < old_length; i++)
    set_array_at(new_value, i, get_array_at(old_value, i));
  set_array_at(new_value, old_length, key);
  TRY_DEF(new_child, new_heap_argument_map_trie(runtime, new_value));
  set_array_buffer_at(children, index, new_child);
  return new_child;
}


// --- L a m b d a ---

GET_FAMILY_PROTOCOL_IMPL(lambda);
TRIVIAL_PRINT_ON_IMPL(Lambda, lambda);

UNCHECKED_ACCESSORS_IMPL(Lambda, lambda, Methods, methods);

value_t lambda_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLambda, value);
  return success();
}

value_t emit_lambda_call_trampoline(assembler_t *assm) {
  TRY(assembler_emit_delegate_lambda_call(assm));
  return success();
}

value_t add_lambda_builtin_methods(runtime_t *runtime, value_t space) {
  TRY(add_method_space_custom_method(runtime, space, runtime->roots.lambda_protocol,
      "()", 0, true, emit_lambda_call_trampoline));
  return success();
}


// --- O r d e r i n g ---

int ordering_to_int(value_t value) {
  return get_integer_value(value);
}

value_t int_to_ordering(int value) {
  return new_integer(value);
}


// --- D e b u g ---

void value_print_ln(value_t value) {
  // Write the value on a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  value_print_on(value, &buf);
  string_t result;
  string_buffer_flush(&buf, &result);
  // Print it on stdout.
  printf("%s\n", result.chars);
  fflush(stdout);
  // Done!
  string_buffer_dispose(&buf);
}

const char *value_to_string(value_to_string_t *data, value_t value) {
  string_buffer_init(&data->buf);
  value_print_on(value, &data->buf);
  string_buffer_flush(&data->buf, &data->str);
  return data->str.chars;
}

void dispose_value_to_string(value_to_string_t *data) {
  string_buffer_dispose(&data->buf);
}
