#include "alloc.h"
#include "test.h"
#include "value.h"

TEST(alloc, heap_string) {
  runtime_t runtime;
  runtime_init(&runtime, NULL);

  string_t chars;
  string_init(&chars, "Hut!");
  value_t str = new_heap_string(&runtime, &chars);
  ASSERT_EQ(vtObject, get_value_tag(str));
  ASSERT_EQ(otString, get_object_type(str));
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_TRUE(string_equals(&chars, &out));

  runtime_dispose(&runtime);
}

TEST(alloc, heap_species) {
  runtime_t runtime;
  runtime_init(&runtime, NULL);

  value_t species = new_heap_species(&runtime, otString);
  ASSERT_EQ(vtObject, get_value_tag(species));
  ASSERT_EQ(otSpecies, get_object_type(species));
  ASSERT_EQ(otString, get_species_instance_type(species));

  runtime_dispose(&runtime);
}
