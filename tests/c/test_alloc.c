// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "test.h"
#include "value-inl.h"

TEST(alloc, heap_string) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  string_t chars = STR("Hut!");
  value_t str = new_heap_string(&runtime, &chars);
  ASSERT_DOMAIN(vdObject, str);
  ASSERT_FAMILY(ofString, str);
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_STREQ(&chars, &out);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
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

  blob_t contents;
  byte_t content_array[3] = {6, 5, 4};
  blob_init(&contents, content_array, 3);
  blob = new_heap_blob_with_data(&runtime, &contents);
  blob_t heap_blob;
  get_blob_data(blob, &heap_blob);
  ASSERT_EQ(3, blob_length(&heap_blob));
  ASSERT_EQ(6, blob_byte_at(&heap_blob, 0));
  ASSERT_EQ(5, blob_byte_at(&heap_blob, 1));
  ASSERT_EQ(4, blob_byte_at(&heap_blob, 2));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, heap_species) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t species = new_heap_compact_species(&runtime, ofString, &kStringBehavior);
  ASSERT_DOMAIN(vdObject, species);
  ASSERT_FAMILY(ofSpecies, species);
  ASSERT_EQ(ofString, get_species_instance_family(species));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, heap_array) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Check initial state.
  value_t array = new_heap_array(&runtime, 3);
  ASSERT_EQ(3, get_array_length(array));
  value_t null = runtime_null(&runtime);
  ASSERT_SAME(null, get_array_at(array, 0));
  ASSERT_SAME(null, get_array_at(array, 1));
  ASSERT_SAME(null, get_array_at(array, 2));

  // Update the array, then check its state.
  set_array_at(array, 0, array);
  set_array_at(array, 2, array);
  ASSERT_EQ(3, get_array_length(array));
  ASSERT_SAME(array, get_array_at(array, 0));
  ASSERT_SAME(null, get_array_at(array, 1));
  ASSERT_SAME(array, get_array_at(array, 2));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, heap_map) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t map = new_heap_id_hash_map(&runtime, 16);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_EQ(16, get_id_hash_map_capacity(map));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, instance) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t instance = new_heap_instance(&runtime, runtime.roots.empty_instance_species);
  ASSERT_FAMILY(ofInstance, instance);
  value_t key = new_integer(0);
  ASSERT_SIGNAL(scNotFound, get_instance_field(instance, key));
  ASSERT_SUCCESS(try_set_instance_field(instance, key, new_integer(3)));
  ASSERT_VALEQ(new_integer(3), get_instance_field(instance, key));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, void_p) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t vp = new_heap_void_p(&runtime, NULL);
  ASSERT_EQ(NULL, get_void_p_value(vp));
  set_void_p_value(vp, &runtime);
  ASSERT_EQ(&runtime, get_void_p_value(vp));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(alloc, literal) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t lit = new_heap_literal_ast(&runtime, new_integer(0));
  ASSERT_FAMILY(ofLiteralAst, lit);
  ASSERT_VALEQ(new_integer(0), get_literal_ast_value(lit));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
