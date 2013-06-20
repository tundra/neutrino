#include "behavior.h"
#include "heap.h"
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

void set_object_species(value_t value, value_t species) {
  *access_object_field(value, kObjectSpeciesOffset) = species;
}

value_t get_object_species(value_t value) {
  return *access_object_field(value, kObjectSpeciesOffset);
}

object_family_t get_object_family(value_t value) {
  value_t species = get_object_species(value);
  return get_species_instance_family(species);
}


// --- S p e c i e s ---

void set_species_instance_family(value_t value,
    object_family_t instance_family) {
  *access_object_field(value, kSpeciesInstanceFamilyOffset) =
      new_integer(instance_family);
}

object_family_t get_species_instance_family(value_t value) {
  value_t family = *access_object_field(value, kSpeciesInstanceFamilyOffset);
  return (object_family_t) get_integer_value(family);
}

void set_species_family_behavior(value_t value, value_t behavior) {
  *access_object_field(value, kSpeciesFamilyBehaviorOffset) = behavior;
}

family_behavior_t *get_species_family_behavior(value_t value) {
  value_t ptr = *access_object_field(value, kSpeciesFamilyBehaviorOffset);
  return get_void_p_value(ptr);
}

void set_species_division_behavior(value_t value, value_t behavior) {
  *access_object_field(value, kSpeciesDivisionBehaviorOffset) = behavior;
}

division_behavior_t *get_species_division_behavior(value_t value) {
  value_t ptr = *access_object_field(value, kSpeciesDivisionBehaviorOffset);
  return get_void_p_value(ptr);
}


// --- S t r i n g ---

size_t calc_string_size(size_t char_count) {
  // We need to fix one extra byte, the terminating null.
  size_t bytes = char_count + 1;
  return kObjectHeaderSize               // header
       + kValueSize                      // length
       + align_size(kValueSize, bytes);  // contents
}

void set_string_length(value_t value, size_t length) {
  CHECK_FAMILY(ofString, value);
  *access_object_field(value, kStringLengthOffset) = new_integer(length);
}

size_t get_string_length(value_t value) {
  CHECK_FAMILY(ofString, value);
  return get_integer_value(*access_object_field(value, kStringLengthOffset));
}

char *get_string_chars(value_t value) {
  CHECK_FAMILY(ofString, value);
  return (char*) access_object_field(value, kStringCharsOffset);
}

void get_string_contents(value_t value, string_t *out) {
  out->length = get_string_length(value);
  out->chars = get_string_chars(value);
}


// --- B l o b ---

size_t calc_blob_size(size_t size) {
  return kObjectHeaderSize              // header
       + kValueSize                     // length
       + align_size(kValueSize, size);  // contents
}

void set_blob_length(value_t value, size_t length) {
  CHECK_FAMILY(ofBlob, value);
  *access_object_field(value, kBlobLengthOffset) = new_integer(length);
}

size_t get_blob_length(value_t value) {
  CHECK_FAMILY(ofBlob, value);
  return get_integer_value(*access_object_field(value, kBlobLengthOffset));
}

void get_blob_data(value_t value, blob_t *blob_out) {
  CHECK_FAMILY(ofBlob, value);
  size_t length = get_blob_length(value);
  byte_t *data = (byte_t*) access_object_field(value, kBlobDataOffset);
  blob_init(blob_out, data, length);
}


// --- V o i d   P ---

void set_void_p_value(value_t value, void *ptr) {
  CHECK_FAMILY(ofVoidP, value);
  *access_object_field(value, kVoidPValueOffset) = (value_t) (address_arith_t) ptr;
}

void *get_void_p_value(value_t value) {
  CHECK_FAMILY(ofVoidP, value);
  return (void*) (address_arith_t) *access_object_field(value, kVoidPValueOffset);
}


// --- A r r a y ---

size_t calc_array_size(size_t length) {
  return kObjectHeaderSize       // header
       + kValueSize              // length
       + (length * kValueSize);  // contents
}

size_t get_array_length(value_t value) {
  CHECK_FAMILY(ofArray, value);
  return get_integer_value(*access_object_field(value, kArrayLengthOffset));
}

void set_array_length(value_t value, size_t length) {
  CHECK_FAMILY(ofArray, value);
  *access_object_field(value, kArrayLengthOffset) = new_integer(length);
}

value_t get_array_at(value_t value, size_t index) {
  CHECK_FAMILY(ofArray, value);
  CHECK_TRUE("array index out of bounds", index < get_array_length(value));
  return *access_object_field(value, kArrayElementsOffset + index);
}

void set_array_at(value_t value, size_t index, value_t element) {
  CHECK_FAMILY(ofArray, value);
  CHECK_TRUE("array index out of bounds", index < get_array_length(value));
  *access_object_field(value, kArrayElementsOffset + index) = element;
}

value_t *get_array_elements(value_t value) {
  CHECK_FAMILY(ofArray, value);
  return access_object_field(value, kArrayElementsOffset);
}


// --- I d e n t i t y   h a s h   m a p ---

value_t get_id_hash_map_entry_array(value_t value) {
  CHECK_FAMILY(ofIdHashMap, value);
  return *access_object_field(value, kIdHashMapEntryArrayOffset);
}

void set_id_hash_map_entry_array(value_t value, value_t entry_array) {
  CHECK_FAMILY(ofIdHashMap, value);
  CHECK_FAMILY(ofArray, entry_array);
  *access_object_field(value, kIdHashMapEntryArrayOffset) = entry_array;
}

size_t get_id_hash_map_size(value_t value) {
  CHECK_FAMILY(ofIdHashMap, value);
  value_t size = *access_object_field(value, kIdHashMapSizeOffset);
  return get_integer_value(size);
}

void set_id_hash_map_size(value_t value, size_t size) {
  CHECK_FAMILY(ofIdHashMap, value);
  *access_object_field(value, kIdHashMapSizeOffset) = new_integer(size);
}

void set_id_hash_map_capacity(value_t value, size_t capacity) {
  CHECK_FAMILY(ofIdHashMap, value);
  *access_object_field(value, kIdHashMapCapacityOffset) = new_integer(capacity);
}

size_t get_id_hash_map_capacity(value_t value) {
  CHECK_FAMILY(ofIdHashMap, value);
  value_t capacity = *access_object_field(value, kIdHashMapCapacityOffset);
  return get_integer_value(capacity);
}

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


// --- B o o l ---

void set_bool_value(value_t value, bool truth) {
  CHECK_FAMILY(ofBool, value);
  *access_object_field(value, kBoolValueOffset) = new_integer(truth ? 1 : 0);
}

bool get_bool_value(value_t value) {
  CHECK_FAMILY(ofBool, value);
  return get_integer_value(*access_object_field(value, kBoolValueOffset));
}


// --- I n s t a n c e ---

void set_instance_fields(value_t value, value_t fields) {
  CHECK_FAMILY(ofInstance, value);
  CHECK_FAMILY(ofIdHashMap, fields);
  *access_object_field(value, kInstanceFieldsOffset) = fields;
}

value_t get_instance_fields(value_t value) {
  CHECK_FAMILY(ofInstance, value);
  return *access_object_field(value, kInstanceFieldsOffset);
}

value_t get_instance_field(value_t value, value_t key) {
  value_t fields = get_instance_fields(value);
  return get_id_hash_map_at(fields, key);
}

value_t try_set_instance_field(value_t instance, value_t key, value_t value) {
  value_t fields = get_instance_fields(instance);
  return try_set_id_hash_map_at(fields, key, value);
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
