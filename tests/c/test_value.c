#include "alloc.h"
#include "heap.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"


// Really simple value tagging stuff.
TEST(value, tagged_integers) {
  value_t v0 = new_integer(10);
  ASSERT_DOMAIN(vdInteger, v0);
  ASSERT_EQ(10, get_integer_value(v0));
  value_t v1 = new_integer(-10);
  ASSERT_DOMAIN(vdInteger, v1);
  ASSERT_EQ(-10, get_integer_value(v1));
  value_t v2 = new_integer(0);
  ASSERT_DOMAIN(vdInteger, v2);
  ASSERT_EQ(0, get_integer_value(v2));
}

TEST(value, signals) {
  value_t v0 = new_signal(scHeapExhausted);
  ASSERT_DOMAIN(vdSignal, v0);
  ASSERT_EQ(scHeapExhausted, get_signal_cause(v0));  
}

TEST(value, objects) {
  heap_t heap;
  ASSERT_SUCCESS(heap_init(&heap, NULL));

  address_t addr;
  ASSERT_TRUE(heap_try_alloc(&heap, 16, &addr));
  value_t v0 = new_object(addr);
  ASSERT_DOMAIN(vdObject, v0);
  ASSERT_EQ(addr, get_object_address(v0));

  heap_dispose(&heap);
}

TEST(value, maps_simple) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Create a map.
  value_t map = new_heap_map(&runtime, 4);
  ASSERT_FAMILY(ofMap, map);
  ASSERT_EQ(0, get_map_size(map));
  // Add something to it.
  ASSERT_SUCCESS(try_set_map_at(map, new_integer(0), new_integer(1)));
  ASSERT_EQ(1, get_map_size(map));
  ASSERT_EQ(new_integer(1), get_map_at(map, new_integer(0)));
  ASSERT_SIGNAL(scNotFound, get_map_at(map, new_integer(1)));
  // Add some more to it.
  ASSERT_SUCCESS(try_set_map_at(map, new_integer(1), new_integer(2)));
  ASSERT_EQ(2, get_map_size(map));
  ASSERT_EQ(new_integer(1), get_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_map_at(map, new_integer(1)));
  // Replace an existing value.
  ASSERT_SUCCESS(try_set_map_at(map, new_integer(0), new_integer(3)));
  ASSERT_EQ(2, get_map_size(map));
  ASSERT_EQ(new_integer(3), get_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_map_at(map, new_integer(1)));
  // There's room for one more value.
  ASSERT_SUCCESS(try_set_map_at(map, new_integer(100), new_integer(5)));
  ASSERT_EQ(3, get_map_size(map));
  ASSERT_EQ(new_integer(3), get_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_map_at(map, new_integer(100)));
  // Now the map should refuse to let us add more.
  ASSERT_SIGNAL(scMapFull, try_set_map_at(map, new_integer(88),
      new_integer(79)));
  ASSERT_EQ(3, get_map_size(map));
  ASSERT_EQ(new_integer(3), get_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_map_at(map, new_integer(100)));
  // However it should still be possible to replace existing mappings.
  ASSERT_SUCCESS(try_set_map_at(map, new_integer(1), new_integer(9)));
  ASSERT_EQ(3, get_map_size(map));
  ASSERT_EQ(new_integer(3), get_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(9), get_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_map_at(map, new_integer(100)));

  runtime_dispose(&runtime);
}


TEST(value, maps_strings) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  string_t one_chars;
  string_init(&one_chars, "One");
  value_t one = new_heap_string(&runtime, &one_chars);

  value_t map = new_heap_map(&runtime, 4);
  ASSERT_EQ(0, get_map_size(map));
  ASSERT_SUCCESS(try_set_map_at(map, one, new_integer(4)));
  ASSERT_EQ(1, get_map_size(map));
  ASSERT_EQ(new_integer(4), get_map_at(map, one));

  runtime_dispose(&runtime);
}
