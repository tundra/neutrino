#include "alloc.h"
#include "test.h"
#include "value.h"

// Initializes a heap with default values such that it can be used for testing.
static void heap_init_for_test(heap_t *heap) {
  // Configure the space.
  space_config_t config;
  space_config_init_defaults(&config);
  heap_init(heap, &config);
}

TEST(alloc, heap_string) {
  heap_t heap;
  heap_init_for_test(&heap);

  string_t chars;
  string_init(&chars, "Hut!");
  value_ptr_t str = new_heap_string(&heap, &chars);
  ASSERT_EQ(vtObject, get_value_tag(str));
  ASSERT_EQ(otString, get_object_type(str));
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_TRUE(string_equals(&chars, &out));

  heap_dispose(&heap);
}
