#include "behavior.h"
#include "heap.h"
#include "value-inl.h"

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

void set_species_behavior(value_t value, behavior_t *behavior) {
  *access_object_field(value, kSpeciesBehaviorOffset) =
      (value_t) (address_arith_t) behavior;
}

behavior_t *get_species_behavior(value_t value) {
  return (behavior_t*) (address_arith_t)
      *access_object_field(value, kSpeciesBehaviorOffset);
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
  CHECK_TRUE(index < get_array_length(value));
  return *access_object_field(value, kArrayElementsOffset + index);
}

void set_array_at(value_t value, size_t index, value_t element) {
  CHECK_FAMILY(ofArray, value);
  CHECK_TRUE(index < get_array_length(value));
  *access_object_field(value, kArrayElementsOffset + index) = element;
}


// --- M a p ---

value_t get_map_entry_array(value_t value) {
  CHECK_FAMILY(ofMap, value);
  return *access_object_field(value, kMapEntryArrayOffset);
}

void set_map_entry_array(value_t value, value_t entry_array) {
  CHECK_FAMILY(ofMap, value);
  CHECK_FAMILY(ofArray, entry_array);
  *access_object_field(value, kMapEntryArrayOffset) = entry_array;
}

size_t get_map_size(value_t value) {
  CHECK_FAMILY(ofMap, value);
  value_t size = *access_object_field(value, kMapSizeOffset);
  return get_integer_value(size);
}

void set_map_size(value_t value, size_t size) {
  CHECK_FAMILY(ofMap, value);
  *access_object_field(value, kMapSizeOffset) = new_integer(size);  
}

void set_map_capacity(value_t value, size_t capacity) {
  CHECK_FAMILY(ofMap, value);
  *access_object_field(value, kMapCapacityOffset) = new_integer(capacity);
}

size_t get_map_capacity(value_t value) {
  CHECK_FAMILY(ofMap, value);
  value_t capacity = *access_object_field(value, kMapCapacityOffset);
  return get_integer_value(capacity);
}

// Returns a pointer to the start of the index'th entry in the given map.
static value_t *get_map_entry(value_t map, size_t index) {
  return access_object_field(map, kMapEntryArrayOffset) + (index * kMapEntryFieldCount);
}

// Returns true if the given map entry is not storing a binding.
static bool is_map_entry_empty(value_t *entry) {
  return !in_domain(vdInteger, entry[kMapEntryHashOffset]);
}

// Returns the hash value stored in this map entry, which must not be empty.
static size_t get_map_entry_hash(value_t *entry) {
  CHECK_FALSE(is_map_entry_empty(entry));
  return get_integer_value(entry[kMapEntryHashOffset]);
}

// Returns the key stored in this hash map entry, which must not be empty.
static value_t get_map_entry_key(value_t *entry) {
  CHECK_FALSE(is_map_entry_empty(entry));
  return entry[kMapEntryKeyOffset];
}

// Returns the value stored in this hash map entry, which must not be empty.
static value_t get_map_entry_value(value_t *entry) {
  CHECK_FALSE(is_map_entry_empty(entry));
  return entry[kMapEntryValueOffset];
}

// Sets the full contents of a map entry.
static void set_map_entry(value_t *entry, value_t key, size_t hash,
    value_t value) {
  entry[kMapEntryKeyOffset] = key;
  entry[kMapEntryHashOffset] = new_integer(hash);
  entry[kMapEntryValueOffset] = value;
}

// Finds the appropriate entry to store a mapping for the given key with the
// given hash. If there is already a binding for the key then this function
// stores the index in the index out parameter. If there isn't and a non-null
// was_created parameter is passed then a free index is stored in the out
// parameter and true is stored in was_created. Otherwise false is returned.
static bool find_map_entry(value_t map, value_t key, size_t hash,
    value_t **entry_out, bool *was_created) {
  CHECK_FAMILY(ofMap, map);
  CHECK_TRUE((was_created == NULL) || !*was_created);
  size_t capacity = get_map_capacity(map);
  CHECK_TRUE(get_map_size(map) < capacity);
  size_t current_index = hash % capacity;
  // Loop around until we find the key or an empty entry. Since we know the
  // capacity is at least one greater than the size there must be at least
  // one empty entry so we know the loop will terminate.
  while (true) {
    value_t *entry = get_map_entry(map, current_index);
    if (is_map_entry_empty(entry)) {
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
    size_t entry_hash = get_map_entry_hash(entry);
    if (entry_hash == hash) {
      value_t entry_key = get_map_entry_key(entry);
      if (value_are_identical(key, entry_key)) {
        // Found the key; just return it.
        *entry_out = entry;
        return true;
      }
    }
    // Didn't find it here so try the next one.
    current_index = (current_index + 1) % capacity;
  }
}

value_t try_set_map_at(value_t map, value_t key, value_t value) {
  CHECK_FAMILY(ofMap, map);
  size_t size = get_map_size(map);
  size_t capacity = get_map_capacity(map);
  bool is_full = (size == (capacity - 1));
  // Calculate the hash.
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  // Locate where the new entry goes in the entry array.
  value_t *entry = NULL;
  bool was_created = false;
  if (!find_map_entry(map, key, hash, &entry, is_full ? NULL : &was_created)) {
    // The only way this can return false is if the map is full (since if
    // was_created was non-null we would have created a new entry) and the
    // key couldn't be found. Report this.
    return new_signal(scMapFull);
  }
  set_map_entry(entry, key, hash, value);
  // Only increment the size if we created a new entry.
  if (was_created)
    set_map_size(map, size + 1);
  return non_signal();
}

value_t get_map_at(value_t map, value_t key) {
  CHECK_FAMILY(ofMap, map);
  TRY_DEF(hash_value, value_transient_identity_hash(key));
  size_t hash = get_integer_value(hash_value);
  value_t *entry = NULL;
  if (find_map_entry(map, key, hash, &entry, NULL)) {
    return get_map_entry_value(entry);
  } else {
    return new_signal(scNotFound);
  }
}


// --- B o o l ---

void set_bool_value(value_t value, bool truth) {
  CHECK_FAMILY(ofBool, value);
  *access_object_field(value, kBoolValueOffset) = (value_t) truth;
}

bool get_bool_value(value_t value) {
  CHECK_FAMILY(ofBool, value);
  return (bool) *access_object_field(value, kBoolValueOffset);
}
