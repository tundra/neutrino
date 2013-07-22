#include "alloc.h"
#include "behavior.h"
#include "value-inl.h"

#include <string.h>


// --- B a s i c ---

// Run a couple of sanity checks before returning the value from a constructor.
// Returns a signal if the check fails, otherwise returns the given value.
static value_t post_create_sanity_check(value_t value, size_t size) {
  TRY(object_validate(value));
  if (get_object_heap_size(value) != size)
    return new_signal(scValidationFailed);
  return value;
}

value_t new_heap_string(runtime_t *runtime, string_t *contents) {
  size_t size = calc_string_size(string_length(contents));
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.string_species));
  set_string_length(result, string_length(contents));
  string_copy_to(contents, get_string_chars(result), string_length(contents) + 1);
  return post_create_sanity_check(result, size);
}

value_t new_heap_blob(runtime_t *runtime, size_t length) {
  size_t size = calc_blob_size(length);
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.blob_species));
  set_blob_length(result, length);
  blob_t data;
  get_blob_data(result, &data);
  blob_fill(&data, 0);
  return post_create_sanity_check(result, size);
}

value_t new_heap_compact_species_unchecked(runtime_t *runtime, object_family_t instance_family,
    family_behavior_t *family_behavior) {
  size_t bytes = kCompactSpeciesSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, bytes,
      runtime->roots.species_species));
  set_species_instance_family(result, instance_family);
  set_species_family_behavior(result, family_behavior);
  set_species_division_behavior(result, &kCompactSpeciesBehavior);
  return result;
}

value_t new_heap_compact_species(runtime_t *runtime, object_family_t instance_family,
    family_behavior_t *family_behavior) {
  TRY_DEF(result, new_heap_compact_species_unchecked(runtime, instance_family,
      family_behavior));
  return post_create_sanity_check(result, kCompactSpeciesSize);
}

value_t new_heap_array(runtime_t *runtime, size_t length) {
  size_t size = calc_array_size(length);
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.array_species));
  set_array_length(result, length);
  for (size_t i = 0; i < length; i++)
    set_array_at(result, i, runtime->roots.null);
  return post_create_sanity_check(result, size);
}

static value_t new_heap_id_hash_map_entry_array(runtime_t *runtime, size_t capacity) {
  return new_heap_array(runtime, capacity * kIdHashMapEntryFieldCount);
}

value_t new_heap_id_hash_map(runtime_t *runtime, size_t init_capacity) {
  CHECK_TRUE("invalid initial capacity", init_capacity > 0);
  TRY_DEF(entries, new_heap_id_hash_map_entry_array(runtime, init_capacity));
  size_t size = kIdHashMapSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.id_hash_map_species));
  set_id_hash_map_entry_array(result, entries);
  set_id_hash_map_size(result, 0);
  set_id_hash_map_capacity(result, init_capacity);
  return post_create_sanity_check(result, size);
}

value_t new_heap_null(runtime_t *runtime) {
  size_t size = kNullSize;
  return alloc_heap_object(&runtime->heap, size, runtime->roots.null_species);
}

value_t new_heap_bool(runtime_t *runtime, bool value) {
  size_t size = kBoolSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.bool_species));
  set_bool_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_instance(runtime_t *runtime) {
  TRY_DEF(fields, new_heap_id_hash_map(runtime, 16));
  size_t size = kInstanceSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.instance_species));
  set_instance_fields(result, fields);
  return post_create_sanity_check(result, size);
}

value_t new_heap_void_p(runtime_t *runtime, void *value) {
  size_t size = kVoidPSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.void_p_species));
  set_void_p_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_factory(runtime_t *runtime, factory_constructor_t *constr) {
  TRY_DEF(constr_wrapper, new_heap_void_p(runtime, (void*) (intptr_t) constr));
  size_t size = kFactorySize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.factory_species));
  set_factory_constructor(result, constr_wrapper);
  return post_create_sanity_check(result, size);
}


// --- S y n t a x ---

value_t new_heap_literal_ast(runtime_t *runtime, value_t value) {
  size_t size = kLiteralAstSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.literal_ast_species));
  set_literal_ast_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t alloc_heap_object(heap_t *heap, size_t bytes, value_t species) {
  address_t addr;
  if (!heap_try_alloc(heap, bytes, &addr))
    return new_signal(scHeapExhausted);
  value_t result = new_object(addr);
  set_object_species(result, species);
  return result;
}

static value_t extend_id_hash_map(runtime_t *runtime, value_t map) {
  // Create the new entry array first so that if it fails we bail out asap.
  size_t old_capacity = get_id_hash_map_capacity(map);
  size_t new_capacity = old_capacity * 2;
  TRY_DEF(new_entry_array, new_heap_id_hash_map_entry_array(runtime, new_capacity));
  // Capture the relevant old state in an iterator before resetting the map.
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, map);
  // Reset the map.
  set_id_hash_map_capacity(map, new_capacity);
  set_id_hash_map_size(map, 0);
  set_id_hash_map_entry_array(map, new_entry_array);
  // Scan through and add the old data.
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    value_t extension = try_set_id_hash_map_at(map, key, value);
    // Since we were able to successfully add these pairs to the old smaller
    // map it can't fail this time around.
    CHECK_TRUE("rehashing failed", get_value_domain(extension) != vdSignal);
  }
  return success();
}

value_t set_id_hash_map_at(runtime_t *runtime, value_t map, value_t key, value_t value) {
  value_t first_try = try_set_id_hash_map_at(map, key, value);
  if (is_signal(scMapFull, first_try)) {
    TRY(extend_id_hash_map(runtime, map));
    value_t second_try = try_set_id_hash_map_at(map, key, value);
    // It should be impossible for the second try to fail if the first try could
    // hash the key and extending was successful.
    CHECK_TRUE("second try failure", get_value_domain(second_try) != vdSignal);
    return second_try;
  } else {
    return first_try;
  }
}

value_t set_instance_field(runtime_t *runtime, value_t instance, value_t key,
    value_t value) {
  value_t fields = get_instance_fields(instance);
  return set_id_hash_map_at(runtime, fields, key, value);
}
