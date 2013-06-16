#include "alloc.h"
#include "test.h"
#include "value-inl.h"

TEST(alloc, heap_string) {
  runtime_t runtime;
  runtime_init(&runtime, NULL);

  string_t chars;
  string_init(&chars, "Hut!");
  value_t str = new_heap_string(&runtime, &chars);
  ASSERT_DOMAIN(vdObject, str);
  ASSERT_FAMILY(ofString, str);
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_TRUE(string_equals(&chars, &out));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_species) {
  runtime_t runtime;
  runtime_init(&runtime, NULL);

  value_t species = new_heap_species(&runtime, ofString, &kStringBehavior);
  ASSERT_DOMAIN(vdObject, species);
  ASSERT_FAMILY(ofSpecies, species);
  ASSERT_EQ(ofString, get_species_instance_family(species));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_array) {
  runtime_t runtime;
  runtime_init(&runtime, NULL);

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
