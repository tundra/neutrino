#include "alloc.h"
#include "behavior.h"
#include "value-inl.h"

#include <string.h>

value_t new_heap_string(runtime_t *runtime, string_t *contents) {
  size_t bytes = calc_string_size(string_length(contents));
  TRY_DEF(result, alloc_heap_object(&runtime->heap, bytes,
      runtime->roots.string_species));
  set_string_length(result, string_length(contents));
  string_copy_to(contents, get_string_chars(result), string_length(contents) + 1);
  return result;
}

value_t new_heap_blob(runtime_t *runtime, size_t length) {
  size_t size = calc_blob_size(length);
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.blob_species));
  set_blob_length(result, length);
  memset(get_blob_data(result), 0, length);
  return result;
}

value_t new_heap_species(runtime_t *runtime, object_family_t instance_family,
    behavior_t *behavior) {
  size_t bytes = kSpeciesSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, bytes,
      runtime->roots.species_species));
  set_species_instance_family(result, instance_family);
  set_species_behavior(result, behavior);
  return result;
}

value_t new_heap_array(runtime_t *runtime, size_t length) {
  size_t size = calc_array_size(length);
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.array_species));
  set_array_length(result, length);
  for (size_t i = 0; i < length; i++)
    set_array_at(result, i, runtime->roots.null);
  return result;
}

value_t new_heap_map(runtime_t *runtime, size_t init_capacity) {
  CHECK_TRUE(init_capacity > 0);
  TRY_DEF(entries, new_heap_array(runtime, init_capacity * kMapEntryFieldCount));
  size_t size = kMapSize;
  TRY_DEF(result, alloc_heap_object(&runtime->heap, size,
      runtime->roots.map_species));
  set_map_entry_array(result, entries);
  set_map_size(result, 0);
  set_map_capacity(result, init_capacity);
  return result;
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
  return result;
}

value_t alloc_heap_object(heap_t *heap, size_t bytes, value_t species) {
  address_t addr;
  if (!heap_try_alloc(heap, bytes, &addr))
    return new_signal(scHeapExhausted);
  value_t result = new_object(addr);
  set_object_species(result, species);
  return result;  
}
