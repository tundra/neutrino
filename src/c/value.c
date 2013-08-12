// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "heap.h"
#include "runtime.h"
#include "value-inl.h"

const char *signal_cause_name(signal_cause_t cause) {
  switch (cause) {
#define GEN_CASE(Name) case sc##Name: return #Name;
    ENUM_SIGNAL_CAUSES(GEN_CASE)
#undef GEN_CASE
    default:
      return "?";
  }
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


// --- S p e c i e s ---

OBJECT_IDENTITY_IMPL(species);
CANT_SET_CONTENTS(species);
TRIVIAL_PRINT_ON_IMPL(Species, species);
NO_FAMILY_PROTOCOL_IMPL(species);

void set_species_instance_family(value_t value,
    object_family_t instance_family) {
  *access_object_field(value, kSpeciesInstanceFamilyOffset) =
      new_integer(instance_family);
}

object_family_t get_species_instance_family(value_t value) {
  value_t family = *access_object_field(value, kSpeciesInstanceFamilyOffset);
  return (object_family_t) get_integer_value(family);
}

// Casts a pointer to a value_t.
#define PTR_TO_VAL(EXPR) ((encoded_value_t) (address_arith_t) (EXPR))

// Casts a value_t to a void*.
#define VAL_TO_PTR(EXPR) ((void*) (address_arith_t) (EXPR))

void set_species_family_behavior(value_t value, family_behavior_t *behavior) {
  access_object_field(value, kSpeciesFamilyBehaviorOffset)->encoded = PTR_TO_VAL(behavior);
}

family_behavior_t *get_species_family_behavior(value_t value) {
  return VAL_TO_PTR(access_object_field(value, kSpeciesFamilyBehaviorOffset)->encoded);
}

void set_species_division_behavior(value_t value, division_behavior_t *behavior) {
  access_object_field(value, kSpeciesDivisionBehaviorOffset)->encoded = PTR_TO_VAL(behavior);
}

division_behavior_t *get_species_division_behavior(value_t value) {
  return VAL_TO_PTR(access_object_field(value, kSpeciesDivisionBehaviorOffset)->encoded);
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

CANT_SET_CONTENTS(string);
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

value_t string_transient_identity_hash(value_t value) {
  string_t contents;
  get_string_contents(value, &contents);
  size_t hash = string_hash(&contents);
  return new_integer(hash);
}

bool string_are_identical(value_t a, value_t b) {
  CHECK_FAMILY(ofString, a);
  CHECK_FAMILY(ofString, b);
  string_t a_contents;
  get_string_contents(a, &a_contents);
  string_t b_contents;
  get_string_contents(b, &b_contents);
  return string_equals(&a_contents, &b_contents);
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


// --- B l o b ---

OBJECT_IDENTITY_IMPL(blob);
CANT_SET_CONTENTS(blob);
GET_FAMILY_PROTOCOL_IMPL(blob);

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

OBJECT_IDENTITY_IMPL(void_p);
CANT_SET_CONTENTS(void_p);
TRIVIAL_PRINT_ON_IMPL(VoidP, void_p);
NO_FAMILY_PROTOCOL_IMPL(void_p);

void set_void_p_value(value_t value, void *ptr) {
  CHECK_FAMILY(ofVoidP, value);
  access_object_field(value, kVoidPValueOffset)->encoded = PTR_TO_VAL(ptr);
}

void *get_void_p_value(value_t value) {
  CHECK_FAMILY(ofVoidP, value);
  return VAL_TO_PTR(access_object_field(value, kVoidPValueOffset)->encoded);
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

OBJECT_IDENTITY_IMPL(array);
CANT_SET_CONTENTS(array);
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
  CHECK_TRUE("array index out of bounds", index < get_array_length(value));
  get_array_elements(value)[index] = element;
}

value_t *get_array_elements(value_t value) {
  CHECK_FAMILY(ofArray, value);
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


// --- A r r a y   b u f f e r ---

OBJECT_IDENTITY_IMPL(array_buffer);
CANT_SET_CONTENTS(array_buffer);
FIXED_SIZE_PURE_VALUE_IMPL(ArrayBuffer, array_buffer);
TRIVIAL_PRINT_ON_IMPL(ArrayBuffer, array_buffer);
GET_FAMILY_PROTOCOL_IMPL(array_buffer);

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


// --- I d e n t i t y   h a s h   m a p ---

OBJECT_IDENTITY_IMPL(id_hash_map);
CANT_SET_CONTENTS(id_hash_map);
FIXED_SIZE_PURE_VALUE_IMPL(IdHashMap, id_hash_map);
GET_FAMILY_PROTOCOL_IMPL(id_hash_map);

CHECKED_ACCESSORS_IMPL(IdHashMap, id_hash_map, Array, EntryArray, entry_array);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Size, size);
INTEGER_ACCESSORS_IMPL(IdHashMap, id_hash_map, Capacity, capacity);

// Returns a pointer to the start of the index'th entry in the given map.
static value_t *get_id_hash_map_entry(value_t map, size_t index) {
  CHECK_TRUE("map entry out of bounds", index < get_id_hash_map_capacity(map));
  value_t array = get_id_hash_map_entry_array(map);
  return get_array_elements(array) + (index * kIdHashMapEntryFieldCount);
}

// Returns true if the given map entry is not storing a binding.
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

// Finds the appropriate entry to store a mapping for the given key with the
// given hash. If there is already a binding for the key then this function
// stores the index in the index out parameter. If there isn't and a non-null
// was_created parameter is passed then a free index is stored in the out
// parameter and true is stored in was_created. Otherwise false is returned.
static bool find_id_hash_map_entry(value_t map, value_t key, size_t hash,
    value_t **entry_out, bool *was_created) {
  CHECK_FAMILY(ofIdHashMap, map);
  CHECK_TRUE("was_created not initialized", (was_created == NULL) || !*was_created);
  size_t capacity = get_id_hash_map_capacity(map);
  CHECK_TRUE("map overfull", get_id_hash_map_size(map) < capacity);
  size_t current_index = hash % capacity;
  // Loop around until we find the key or an empty entry. Since we know the
  // capacity is at least one greater than the size there must be at least
  // one empty entry so we know the loop will terminate.
  while (true) {
    value_t *entry = get_id_hash_map_entry(map, current_index);
    if (is_id_hash_map_entry_empty(entry)) {
      if (was_created == NULL) {
        // Report that we didn't find the entry.
        return false;
      } else {
        // Found an empty entry which the caller wants us to return.
        *entry_out = entry;
        *was_created = true;
        return true;
      }
    }
    size_t entry_hash = get_id_hash_map_entry_hash(entry);
    if (entry_hash == hash) {
      value_t entry_key = get_id_hash_map_entry_key(entry);
      if (value_are_identical(key, entry_key)) {
        // Found the key; just return it.
        *entry_out = entry;
        return true;
      }
    }
    // Didn't find it here so try the next one.
    current_index = (current_index + 1) % capacity;
  }
  UNREACHABLE("id hash map entry impossible loop");
  return false;
}

value_t try_set_id_hash_map_at(value_t map, value_t key, value_t value) {
  CHECK_FAMILY(ofIdHashMap, map);
  size_t size = get_id_hash_map_size(map);
  size_t capacity = get_id_hash_map_capacity(map);
  bool is_full = (size == (capacity - 1));
  // Calculate the hash.
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  // Locate where the new entry goes in the entry array.
  value_t *entry = NULL;
  bool was_created = false;
  if (!find_id_hash_map_entry(map, key, hash, &entry, is_full ? NULL : &was_created)) {
    // The only way this can return false is if the map is full (since if
    // was_created was non-null we would have created a new entry) and the
    // key couldn't be found. Report this.
    return new_signal(scMapFull);
  }
  set_id_hash_map_entry(entry, key, hash, value);
  // Only increment the size if we created a new entry.
  if (was_created)
    set_id_hash_map_size(map, size + 1);
  return success();
}

value_t get_id_hash_map_at(value_t map, value_t key) {
  CHECK_FAMILY(ofIdHashMap, map);
  // We need to handle this as a special case otherwise the find loop won't
  // terminate.
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  value_t *entry = NULL;
  if (find_id_hash_map_entry(map, key, hash, &entry, NULL)) {
    return get_id_hash_map_entry_value(entry);
  } else {
    return new_signal(scNotFound);
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
      // Found one, store it in current and return success.
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
  while (id_hash_map_iter_advance(&iter)) {
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

CANT_SET_CONTENTS(null);
FIXED_SIZE_PURE_VALUE_IMPL(Null, null);
GET_FAMILY_PROTOCOL_IMPL(null);

value_t null_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNull, value);
  return success();
}

value_t null_transient_identity_hash(value_t value) {
  static const size_t kNullHash = 0x4323;
  return new_integer(kNullHash);
}

bool null_are_identical(value_t a, value_t b) {
  // There is only one null so you should never end up comparing two different
  // ones.
  CHECK_EQ("multiple nulls", a.encoded, b.encoded);
  return true;
}

void null_print_on(value_t value, string_buffer_t *buf) {
  null_print_atomic_on(value, buf);
}

void null_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "null");
}


// --- B o o l ---

CANT_SET_CONTENTS(bool);
FIXED_SIZE_PURE_VALUE_IMPL(Bool, bool);
GET_FAMILY_PROTOCOL_IMPL(bool);

void set_bool_value(value_t value, bool truth) {
  CHECK_FAMILY(ofBool, value);
  *access_object_field(value, kBoolValueOffset) = new_integer(truth ? 1 : 0);
}

bool get_bool_value(value_t value) {
  CHECK_FAMILY(ofBool, value);
  return get_integer_value(*access_object_field(value, kBoolValueOffset));
}

value_t bool_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofBool, value);
  bool which = get_bool_value(value);
  VALIDATE((which == true) || (which == false));
  return success();
}

value_t bool_transient_identity_hash(value_t value) {
  static const size_t kTrueHash = 0x3213;
  static const size_t kFalseHash = 0x5423;
  return new_integer(get_bool_value(value) ? kTrueHash : kFalseHash);
}

bool bool_are_identical(value_t a, value_t b) {
  // There is only one true and false which are both only equal to themselves.
  return (a.encoded == b.encoded);
}

void bool_print_on(value_t value, string_buffer_t *buf) {
  bool_print_atomic_on(value, buf);
}

void bool_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, get_bool_value(value) ? "true" : "false");
}


// --- I n s t a n c e ---

OBJECT_IDENTITY_IMPL(instance);
FIXED_SIZE_PURE_VALUE_IMPL(Instance, instance);

CHECKED_ACCESSORS_IMPL(Instance, instance, IdHashMap, Fields, fields);

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
  string_buffer_printf(buf, "#<instance: ");
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

OBJECT_IDENTITY_IMPL(factory);
CANT_SET_CONTENTS(factory);
FIXED_SIZE_PURE_VALUE_IMPL(Factory, factory);
NO_FAMILY_PROTOCOL_IMPL(factory);

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

OBJECT_IDENTITY_IMPL(code_block);
CANT_SET_CONTENTS(code_block);
FIXED_SIZE_PURE_VALUE_IMPL(CodeBlock, code_block);
NO_FAMILY_PROTOCOL_IMPL(code_block);

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

OBJECT_IDENTITY_IMPL(protocol);
CANT_SET_CONTENTS(protocol);
FIXED_SIZE_PURE_VALUE_IMPL(Protocol, protocol);
GET_FAMILY_PROTOCOL_IMPL(protocol);

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


// --- D e b u g ---

void value_print_ln(value_t value) {
  // Write the value on a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);
  value_print_on(value, &buf);
  string_t result;
  string_buffer_flush(&buf, &result);
  // Print it on stdout.
  printf("%s\n", result.chars);
  // Done!
  string_buffer_dispose(&buf);
}
