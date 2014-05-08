//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "value-inl.h"
END_C_INCLUDES

TEST(alloc, heap_string) {
  CREATE_RUNTIME();

  string_t chars = new_string("Hut!");
  value_t str = new_heap_string(runtime, &chars);
  ASSERT_DOMAIN(vdHeapObject, str);
  ASSERT_FAMILY(ofString, str);
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_STREQ(&chars, &out);

  DISPOSE_RUNTIME();
}

TEST(alloc, heap_blob) {
  CREATE_RUNTIME();

  value_t blob = new_heap_blob(runtime, 9);
  ASSERT_DOMAIN(vdHeapObject, blob);
  ASSERT_FAMILY(ofBlob, blob);
  ASSERT_EQ(9, get_blob_length(blob));
  blob_t data;
  get_blob_data(blob, &data);
  ASSERT_EQ(9, blob_byte_length(&data));
  for (size_t i = 0; i < 9; i++)
    ASSERT_EQ(0, blob_byte_at(&data, i));

  blob_t contents;
  byte_t content_array[3] = {6, 5, 4};
  blob_init(&contents, content_array, 3);
  blob = new_heap_blob_with_data(runtime, &contents);
  blob_t heap_blob;
  get_blob_data(blob, &heap_blob);
  ASSERT_EQ(3, blob_byte_length(&heap_blob));
  ASSERT_EQ(6, blob_byte_at(&heap_blob, 0));
  ASSERT_EQ(5, blob_byte_at(&heap_blob, 1));
  ASSERT_EQ(4, blob_byte_at(&heap_blob, 2));

  DISPOSE_RUNTIME();
}

TEST(alloc, heap_species) {
  CREATE_RUNTIME();

  value_t species = new_heap_compact_species(runtime, &kStringBehavior);
  ASSERT_DOMAIN(vdHeapObject, species);
  ASSERT_FAMILY(ofSpecies, species);
  ASSERT_EQ(ofString, get_species_instance_family(species));

  DISPOSE_RUNTIME();
}

TEST(alloc, heap_array) {
  CREATE_RUNTIME();

  // Check initial state.
  value_t array = new_heap_array(runtime, 3);
  ASSERT_EQ(3, get_array_length(array));
  ASSERT_SAME(null(), get_array_at(array, 0));
  ASSERT_SAME(null(), get_array_at(array, 1));
  ASSERT_SAME(null(), get_array_at(array, 2));

  // Update the array, then check its state.
  set_array_at(array, 0, array);
  set_array_at(array, 2, array);
  ASSERT_EQ(3, get_array_length(array));
  ASSERT_SAME(array, get_array_at(array, 0));
  ASSERT_SAME(null(), get_array_at(array, 1));
  ASSERT_SAME(array, get_array_at(array, 2));

  DISPOSE_RUNTIME();
}

TEST(alloc, heap_map) {
  CREATE_RUNTIME();

  value_t map = new_heap_id_hash_map(runtime, 16);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_EQ(16, get_id_hash_map_capacity(map));

  DISPOSE_RUNTIME();
}

TEST(alloc, instance) {
  CREATE_RUNTIME();

  value_t instance = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  ASSERT_FAMILY(ofInstance, instance);
  value_t key = new_integer(0);
  ASSERT_CONDITION(ccNotFound, get_instance_field(instance, key));
  ASSERT_SUCCESS(try_set_instance_field(instance, key, new_integer(3)));
  ASSERT_VALEQ(new_integer(3), get_instance_field(instance, key));

  DISPOSE_RUNTIME();
}

TEST(alloc, void_p) {
  CREATE_RUNTIME();

  value_t vp = new_heap_void_p(runtime, NULL);
  ASSERT_PTREQ(NULL, get_void_p_value(vp));
  set_void_p_value(vp, runtime);
  ASSERT_PTREQ(runtime, get_void_p_value(vp));

  DISPOSE_RUNTIME();
}

TEST(alloc, literal) {
  CREATE_RUNTIME();

  value_t lit = new_heap_literal_ast(runtime, new_integer(0));
  ASSERT_FAMILY(ofLiteralAst, lit);
  ASSERT_VALEQ(new_integer(0), get_literal_ast_value(lit));

  DISPOSE_RUNTIME();
}
