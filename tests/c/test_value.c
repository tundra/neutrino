#include "heap.h"
#include "test.h"
#include "value.h"


// Really simple value tagging stuff.
TEST(value, tagged_integers) {
  value_ptr_t v0 = new_integer(10);
  ASSERT_EQ(vtInteger, get_value_tag(v0));
  ASSERT_EQ(10, get_integer_value(v0));
  value_ptr_t v1 = new_integer(-10);
  ASSERT_EQ(vtInteger, get_value_tag(v1));
  ASSERT_EQ(-10, get_integer_value(v1));
  value_ptr_t v2 = new_integer(0);
  ASSERT_EQ(vtInteger, get_value_tag(v2));
  ASSERT_EQ(0, get_integer_value(v2));
}

TEST(value, signals) {
  value_ptr_t v0 = new_signal(scHeapExhausted);
  ASSERT_EQ(vtSignal, get_value_tag(v0));
  ASSERT_EQ(scHeapExhausted, get_signal_cause(v0));  
}

// Initializes a heap with default values such that it can be used for testing.
static void heap_init_for_test(heap_t *heap) {
  // Configure the space.
  space_config_t config;
  space_config_init_defaults(&config);
  heap_init(heap, &config);
}

TEST(value, objects) {
  // Set up the test heap.
  heap_t heap;
  heap_init_for_test(&heap);

  address_t addr;
  ASSERT_TRUE(heap_try_alloc(&heap, 16, &addr));
  value_ptr_t v0 = new_object(addr);
  ASSERT_EQ(vtObject, get_value_tag(v0));
  ASSERT_EQ(addr, get_object_address(v0));

  heap_dispose(&heap);
}
