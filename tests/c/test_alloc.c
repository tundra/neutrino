#include "alloc.h"
#include "test.h"
#include "value-inl.h"

TEST(alloc, heap_string) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  DEF_STRING(chars, "Hut!");
  value_t str = new_heap_string(&runtime, &chars);
  ASSERT_DOMAIN(vdObject, str);
  ASSERT_FAMILY(ofString, str);
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_STREQ(&chars, &out);

  runtime_dispose(&runtime);
}

TEST(alloc, heap_blob) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t blob = new_heap_blob(&runtime, 9);
  ASSERT_DOMAIN(vdObject, blob);
  ASSERT_FAMILY(ofBlob, blob);
  ASSERT_EQ(9, get_blob_length(blob));
  blob_t data;
  get_blob_data(blob, &data);
  ASSERT_EQ(9, blob_length(&data));
  for (size_t i = 0; i < 9; i++)
    ASSERT_EQ(0, blob_byte_at(&data, i));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_species) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t species = new_heap_species(&runtime, ofString, &kStringBehavior);
  ASSERT_DOMAIN(vdObject, species);
  ASSERT_FAMILY(ofSpecies, species);
  ASSERT_EQ(ofString, get_species_instance_family(species));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_array) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Check initial state.
  value_t array = new_heap_array(&runtime, 3);
  ASSERT_EQ(3, get_array_length(array));
  value_t null = runtime_null(&runtime);
  ASSERT_EQ(null, get_array_at(array, 0));
  ASSERT_EQ(null, get_array_at(array, 1));
  ASSERT_EQ(null, get_array_at(array, 2));

  // Update the array, then check its state.
  set_array_at(array, 0, array);
  set_array_at(array, 2, array);
  ASSERT_EQ(3, get_array_length(array));
  ASSERT_EQ(array, get_array_at(array, 0));
  ASSERT_EQ(null, get_array_at(array, 1));
  ASSERT_EQ(array, get_array_at(array, 2));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_map) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t map = new_heap_id_hash_map(&runtime, 16);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_EQ(16, get_id_hash_map_capacity(map));

  runtime_dispose(&runtime);
}
