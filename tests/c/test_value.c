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

TEST(value, id_hash_maps_simple) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Create a map.
  value_t map = new_heap_is_hash_map(&runtime, 4);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  // Add something to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(1)));
  ASSERT_EQ(1, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SIGNAL(scNotFound, get_id_hash_map_at(map, new_integer(1)));
  // Add some more to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(2)));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // Replace an existing value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(3)));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // There's room for one more value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(100), new_integer(5)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // Now the map should refuse to let us add more.
  ASSERT_SIGNAL(scMapFull, try_set_id_hash_map_at(map, new_integer(88),
      new_integer(79)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // However it should still be possible to replace existing mappings.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(9)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_EQ(new_integer(9), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_EQ(new_integer(5), get_id_hash_map_at(map, new_integer(100)));

  runtime_dispose(&runtime);
}


TEST(value, id_hash_maps_strings) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  string_t one_chars;
  string_init(&one_chars, "One");
  value_t one = new_heap_string(&runtime, &one_chars);

  value_t map = new_heap_is_hash_map(&runtime, 4);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, one, new_integer(4)));
  ASSERT_EQ(1, get_id_hash_map_size(map));
  ASSERT_EQ(new_integer(4), get_id_hash_map_at(map, one));

  runtime_dispose(&runtime);
}


TEST(value, large_id_hash_maps) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t map = new_heap_is_hash_map(&runtime, 4);
  for (size_t i = 0; i < 128; i++) {
    value_t key = new_integer(i);
    value_t value = new_integer(1024 - i);
    ASSERT_SUCCESS(set_id_hash_map_at(&runtime, map, key, value));
    ASSERT_SUCCESS(object_validate(map));
    for (size_t j = 0; j <= i; j++) {
      value_t check_key = new_integer(j);
      value_t check_value = get_id_hash_map_at(map, check_key);
      ASSERT_SUCCESS(check_value);
      ASSERT_EQ(1024 - j, get_integer_value(check_value));
    }
  }

  runtime_dispose(&runtime);
}
