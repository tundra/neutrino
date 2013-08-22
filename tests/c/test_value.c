// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "heap.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"

// Checks whether the value fits in a tagged integer by actually storing it,
// getting the value back out, and testing whether it could be restored. This is
// an extra sanity check.
static bool try_tagging_as_integer(int64_t value) {
  union {
    int8_t : 3;
    int64_t payload : 61;
  } data = {value};
  return data.payload == value;
}

TEST(value, fits_as_tagged_integer) {
  struct test_case_t {
    int64_t value;
    bool fits;
  };
#define kTestCaseCount 17
  struct test_case_t cases[kTestCaseCount] = {
      {0x0000000000000000, true},
      {0x0000000000000001, true},
      {0xFFFFFFFFFFFFFFFF, true},
      {0x0000000080000000, true},
      {0xFFFFFFFF7FFFFFFF, true},
      {0x7FFFFFFFFFFFFFFF, false},
      {0x3FFFFFFFFFFFFFFF, false},
      {0x1FFFFFFFFFFFFFFF, false},
      {0x1000000000000000, false},
      {0x0FFFFFFFFFFFFFFF, true},
      {0x0FFFFFFFFFFFFFFE, true},
      {0x8000000000000000, false},
      {0xC000000000000000, false},
      {0xE000000000000000, false},
      {0xEFFFFFFFFFFFFFFF, false},
      {0xF000000000000000, true},
      {0xF000000000000001, true}
  };
  for (size_t i = 0; i < kTestCaseCount; i++) {
    struct test_case_t test_case = cases[i];
    ASSERT_EQ(test_case.fits, try_tagging_as_integer(test_case.value));
    ASSERT_EQ(test_case.fits, fits_as_tagged_integer(test_case.value));
  }
#undef kTestCaseCount
}

TEST(value, encoding) {
  ASSERT_EQ(sizeof(encoded_value_t), sizeof(value_t));
  value_t v0 = new_integer(0);
  ASSERT_EQ(vdInteger, v0.encoded & 0x7);
}

TEST(value, sizes) {
  ASSERT_TRUE(sizeof(void*) <= sizeof(encoded_value_t));
}

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
  ASSERT_PTREQ(addr, get_object_address(v0));

  heap_dispose(&heap);
}

TEST(value, id_hash_maps_simple) {
  CREATE_RUNTIME();

  // Create a map.
  value_t map = new_heap_id_hash_map(runtime, 4);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_SIGNAL(scNotFound, get_id_hash_map_at(map, new_integer(0)));
  // Add something to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(1)));
  ASSERT_EQ(1, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SIGNAL(scNotFound, get_id_hash_map_at(map, new_integer(1)));
  // Add some more to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(2)));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // Replace an existing value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(3)));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // There's room for one more value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(100), new_integer(5)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_SAME(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // Now the map should refuse to let us add more.
  ASSERT_SIGNAL(scMapFull, try_set_id_hash_map_at(map, new_integer(88),
      new_integer(79)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_SAME(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // However it should still be possible to replace existing mappings.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(9)));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(9), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_SAME(new_integer(5), get_id_hash_map_at(map, new_integer(100)));

  DISPOSE_RUNTIME();
}


TEST(value, id_hash_maps_strings) {
  CREATE_RUNTIME();

  string_t one_chars;
  string_init(&one_chars, "One");
  value_t one = new_heap_string(runtime, &one_chars);

  value_t map = new_heap_id_hash_map(runtime, 4);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, one, new_integer(4)));
  ASSERT_EQ(1, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(4), get_id_hash_map_at(map, one));

  DISPOSE_RUNTIME();
}


TEST(value, large_id_hash_maps) {
  CREATE_RUNTIME();

  value_t map = new_heap_id_hash_map(runtime, 4);
  for (size_t i = 0; i < 128; i++) {
    value_t key = new_integer(i);
    value_t value = new_integer(1024 - i);
    ASSERT_SUCCESS(set_id_hash_map_at(runtime, map, key, value));
    ASSERT_SUCCESS(object_validate(map));
    for (size_t j = 0; j <= i; j++) {
      value_t check_key = new_integer(j);
      value_t check_value = get_id_hash_map_at(map, check_key);
      ASSERT_SUCCESS(check_value);
      int64_t expected = 1024 - j;
      ASSERT_EQ(expected, get_integer_value(check_value));
    }
  }

  DISPOSE_RUNTIME();
}


TEST(value, exhaust_id_hash_map) {
  space_config_t config;
  space_config_init_defaults(&config);
  config.size_bytes = 8192;
  runtime_t stack_runtime;
  runtime_t *runtime = &stack_runtime;
  ASSERT_SUCCESS(runtime_init(runtime, &config));

  value_t map = new_heap_id_hash_map(runtime, 4);
  for (size_t i = 0; true; i++) {
    value_t key = new_integer(i);
    value_t value = new_integer(1024 - i);
    value_t result = set_id_hash_map_at(runtime, map, key, value);
    ASSERT_SUCCESS(object_validate(map));
    if (is_signal(scHeapExhausted, result))
      break;
    ASSERT_SUCCESS(result);
  }

  DISPOSE_RUNTIME();
}


TEST(value, array_bounds) {
  CREATE_RUNTIME();

  value_t arr = new_heap_array(runtime, 4);
  ASSERT_SUCCESS(get_array_at(arr, 0));
  ASSERT_SUCCESS(get_array_at(arr, 1));
  ASSERT_SUCCESS(get_array_at(arr, 2));
  ASSERT_SUCCESS(get_array_at(arr, 3));
  ASSERT_CHECK_FAILURE(scOutOfBounds, get_array_at(arr, 4));

  DISPOSE_RUNTIME();
}


TEST(value, array_buffer) {
  CREATE_RUNTIME();

  value_t buf = new_heap_array_buffer(runtime, 16);
  ASSERT_SUCCESS(buf);
  for (size_t i = 0; i < 16; i++) {
    ASSERT_EQ(i, get_array_buffer_length(buf));
    ASSERT_TRUE(try_add_to_array_buffer(buf, new_integer(i)));
    ASSERT_VALEQ(new_integer(i / 2), get_array_buffer_at(buf, i / 2));
    ASSERT_CHECK_FAILURE(scOutOfBounds, get_array_buffer_at(buf, i + 1));
  }

  ASSERT_EQ(16, get_array_buffer_length(buf));
  ASSERT_FALSE(try_add_to_array_buffer(buf, new_integer(16)));
  ASSERT_EQ(16, get_array_buffer_length(buf));

  for (size_t i = 16; i < 1024; i++) {
    ASSERT_EQ(i, get_array_buffer_length(buf));
    ASSERT_SUCCESS(add_to_array_buffer(runtime, buf, new_integer(i)));
    ASSERT_VALEQ(new_integer(i / 2), get_array_buffer_at(buf, i / 2));
    ASSERT_CHECK_FAILURE(scOutOfBounds, get_array_buffer_at(buf, i + 1));
  }

  DISPOSE_RUNTIME();
}


TEST(value, get_protocol) {
  CREATE_RUNTIME();

  value_t int_proto = get_protocol(new_integer(2), runtime);
  ASSERT_VALEQ(int_proto, runtime->roots.integer_protocol);
  ASSERT_VALEQ(int_proto, get_protocol(new_integer(6), runtime));
  value_t null_proto = get_protocol(runtime_null(runtime), runtime);
  ASSERT_FALSE(value_structural_equal(int_proto, null_proto));
  ASSERT_VALEQ(null_proto, runtime->roots.null_protocol);

  DISPOSE_RUNTIME();
}


TEST(value, instance_division) {
  CREATE_RUNTIME();

  value_t proto = new_heap_protocol(runtime, runtime_null(runtime));
  value_t species = new_heap_instance_species(runtime, proto);
  value_t instance = new_heap_instance(runtime, species);
  ASSERT_VALEQ(proto, get_instance_species_primary_protocol(species));
  ASSERT_VALEQ(proto, get_instance_primary_protocol(instance));
  ASSERT_VALEQ(proto, get_protocol(instance, runtime));

  DISPOSE_RUNTIME();
}


TEST(value, ordering) {
  ASSERT_EQ(0, ordering_to_int(int_to_ordering(0)));
  ASSERT_EQ(-1, ordering_to_int(int_to_ordering(-1)));
  ASSERT_EQ(1, ordering_to_int(int_to_ordering(1)));
  ASSERT_EQ(-65536, ordering_to_int(int_to_ordering(-65536)));
  ASSERT_EQ(1024, ordering_to_int(int_to_ordering(1024)));
}


TEST(value, integer_comparison) {
#define ASSERT_INT_COMPARE(A, REL, B)                                          \
  ASSERT_TRUE(ordering_to_int(value_ordering_compare(new_integer(A), new_integer(B))) REL 0)

  ASSERT_INT_COMPARE(0, <, 1);
  ASSERT_INT_COMPARE(0, ==, 0);
  ASSERT_INT_COMPARE(2, >, 1);

#undef ASSERT_INT_COMPARE
}

TEST(value, string_comparison) {
  CREATE_RUNTIME();

  // Checks that the string with contents A compares to B as the given operator.
#define ASSERT_STR_COMPARE(A, REL, B) do {                                     \
  string_t a_str = STR(A);                                                     \
  value_t a = new_heap_string(runtime, &a_str);                               \
  string_t b_str = STR(B);                                                     \
  value_t b = new_heap_string(runtime, &b_str);                               \
  ASSERT_TRUE(ordering_to_int(value_ordering_compare(a, b)) REL 0);            \
} while (false)

  ASSERT_STR_COMPARE("", ==, "");
  ASSERT_STR_COMPARE("", <, "x");
  ASSERT_STR_COMPARE("", <, "xx");
  ASSERT_STR_COMPARE("x", <, "xx");
  ASSERT_STR_COMPARE("xx", ==, "xx");
  ASSERT_STR_COMPARE("xxx", >, "xx");
  ASSERT_STR_COMPARE("xy", >, "xx");
  ASSERT_STR_COMPARE("yx", >, "xx");
  ASSERT_STR_COMPARE("yx", >, "x");
  ASSERT_STR_COMPARE("wx", >, "x");

#undef ASSERT_STR_COMPARE

  DISPOSE_RUNTIME();
}

TEST(value, bool_comparison) {
  CREATE_RUNTIME();

  value_t t = runtime_bool(runtime, true);
  value_t f = runtime_bool(runtime, false);

  ASSERT_TRUE(ordering_to_int(value_ordering_compare(t, t)) == 0);
  ASSERT_TRUE(ordering_to_int(value_ordering_compare(f, f)) == 0);
  ASSERT_TRUE(ordering_to_int(value_ordering_compare(t, f)) > 0);
  ASSERT_TRUE(ordering_to_int(value_ordering_compare(f, t)) < 0);

  DISPOSE_RUNTIME();
}

TEST(value, array_sort) {
  CREATE_RUNTIME();

#define kTestArraySize 32

  static const int kUnsorted[kTestArraySize] = {
      44, 29, 86, 93, 6, 37, 93, 15, 18, 88, 93, 5, 97, 69, 32, 27, 2, 96, 34,
      33, 15, 61, 48, 19, 93, 9, 27, 70, 86, 41, 81, 61
  };
  static const int kSorted[kTestArraySize] = {
      2, 5, 6, 9, 15, 15, 18, 19, 27, 27, 29, 32, 33, 34, 37, 41, 44, 48, 61,
      61, 69, 70, 81, 86, 86, 88, 93, 93, 93, 93, 96, 97
  };

  // Normal sorting
  ASSERT_TRUE(is_array_sorted(runtime->roots.empty_array));
  value_t a0 = new_heap_array(runtime, kTestArraySize);
  for (size_t i = 0; i < kTestArraySize; i++)
    set_array_at(a0, i, new_integer(kUnsorted[i]));
  ASSERT_FALSE(is_array_sorted(a0));
  sort_array(a0);
  for (size_t i = 0; i < kTestArraySize; i++)
    ASSERT_EQ(kSorted[i], get_integer_value(get_array_at(a0, i)));
  ASSERT_TRUE(is_array_sorted(a0));

  // Co-sorting
  value_t a1 = new_heap_pair_array(runtime, kTestArraySize);
  for (size_t i = 0; i < kTestArraySize; i++) {
    set_pair_array_first_at(a1, i, new_integer(kUnsorted[i]));
    set_pair_array_second_at(a1, i, new_integer(i));
  }
  co_sort_pair_array(a1);
  for (size_t i = 0; i < kTestArraySize; i++) {
    // The first values are now in sorted order.
    int value = get_integer_value(get_pair_array_first_at(a1, i));
    ASSERT_EQ(kSorted[i], value);
    // The second value says where in the unsorted order the value was and they
    // should still match.
    int order = get_integer_value(get_pair_array_second_at(a1, i));
    ASSERT_EQ(value, kUnsorted[order]);
  }

  // Binary search.
  for (int i = 0; i < 100; i++) {
    // Check if 'i' is in the array.
    bool is_present = false;
    for (size_t j = 0; j < kTestArraySize && !is_present; j++) {
      if (kUnsorted[j] == i)
        is_present = true;
    }
    value_t found = binary_search_pair_array(a1, new_integer(i));
    if (is_present) {
      ASSERT_SUCCESS(found);
      ASSERT_EQ(i, kUnsorted[get_integer_value(found)]);
    } else {
      ASSERT_SIGNAL(scNotFound, found);
    }
  }

  DISPOSE_RUNTIME();
}

static const size_t kMapCount = 8;
static const size_t kInstanceCount = 256;

// Checks that the instances are present in the maps as expected, skipping the
// first skip_first entries. This is such that we can gradually dispose the
// maps.
static void assert_strings_present(size_t skip_first, gc_safe_t **maps,
    gc_safe_t **insts) {
  for (size_t inst_i = 0; inst_i < kInstanceCount; inst_i++) {
    value_t inst = gc_safe_get_value(insts[inst_i]);
    for (size_t map_i = skip_first; map_i < kMapCount; map_i++) {
      value_t map = gc_safe_get_value(maps[map_i]);
      bool should_be_present = ((inst_i % (map_i + 1)) == 0);
      value_t value = get_id_hash_map_at(map, inst);
      if (should_be_present) {
        ASSERT_SAME(inst, value);
        value_t field = get_instance_field(value, new_integer(0));
        ASSERT_VALEQ(new_integer(inst_i), field);
      } else {
        ASSERT_SIGNAL(scNotFound, value);
      }
    }
  }
}

TEST(value, rehash_map) {
  CREATE_RUNTIME();

  // Create and retain a number of maps.
  gc_safe_t *maps[8];
  for (size_t i = 0; i < kMapCount; i++) {
    value_t map = new_heap_id_hash_map(runtime, 16);
    maps[i] = runtime_new_gc_safe(runtime, map);
  }

  // Build and retain a number of strings. We'll use these as keys.
  gc_safe_t *insts[256];
  for (size_t i = 0; i < kInstanceCount; i++) {
    value_t inst = new_heap_instance(runtime, runtime->roots.empty_instance_species);
    ASSERT_SUCCESS(set_instance_field(runtime, inst, new_integer(0), new_integer(i)));
    insts[i] = runtime_new_gc_safe(runtime, inst);
  }

  // Store the strings sort-of randomly in the maps.
  for (size_t inst_i = 0; inst_i < kInstanceCount; inst_i++) {
    value_t inst = gc_safe_get_value(insts[inst_i]);
    for (size_t map_i = 0; map_i < kMapCount; map_i++) {
      if ((inst_i % (map_i + 1)) == 0) {
        // If the map's index (plus 1 to avoid 0) is a divisor in the string's
        // index we add it to the map. This means that the 0th map gets all
        // strings whereas the 15th get 1/15.
        value_t map = gc_safe_get_value(maps[map_i]);
        ASSERT_SUCCESS(set_id_hash_map_at(runtime, map, inst, inst));
      }
    }
  }

  assert_strings_present(0, maps, insts);
  runtime_garbage_collect(runtime);
  assert_strings_present(0, maps, insts);

  for (size_t i = 0; i < kMapCount; i++) {
    // Dispose the maps one at a time and then garbage collect to get them
    // to move around.
    runtime_dispose_gc_safe(runtime, maps[i]);
    runtime_garbage_collect(runtime);
    assert_strings_present(i + 1, maps, insts);
  }

  // Give back the instance handles.
  for (size_t i = 0; i < kInstanceCount; i++)
    runtime_dispose_gc_safe(runtime, insts[i]);

  DISPOSE_RUNTIME();
}

TEST(value, delete) {
  CREATE_RUNTIME();

  // Bit set to keep track of which entries are set in the map.
  static const size_t kRange = 257;
  bit_vector_t bits;
  bit_vector_init(&bits, kRange, false);
  size_t bits_set = 0;

  pseudo_random_t rand;
  pseudo_random_init(&rand, 35234);

  value_t map = new_heap_id_hash_map(runtime, kRange + 5);
  for (size_t t = 0; t <= 4096; t++) {
    ASSERT_EQ(bits_set, get_id_hash_map_size(map));
    // Pick a random element to change.
    size_t index = pseudo_random_next(&rand, kRange);
    value_t key = new_integer(index);
    if (bit_vector_get_at(&bits, index)) {
      ASSERT_SUCCESS(delete_id_hash_map_at(runtime, map, key));
      bit_vector_set_at(&bits, index, false);
      bits_set--;
    } else {
      ASSERT_SIGNAL(scNotFound, delete_id_hash_map_at(runtime, map, key));
      ASSERT_SUCCESS(try_set_id_hash_map_at(map, key, key));
      bit_vector_set_at(&bits, index, true);
      bits_set++;
    }
    if ((t % 128) == 0) {
      // Check that getting the values directly works.
      for (size_t i = 0; i < kRange; i++) {
        value_t key = new_integer(i);
        bool in_map = !is_signal(scNotFound, get_id_hash_map_at(map, key));
        ASSERT_EQ(bit_vector_get_at(&bits, i), in_map);
      }
      // Check that iteration works.
      id_hash_map_iter_t iter;
      id_hash_map_iter_init(&iter, map);
      size_t seen = 0;
      while (id_hash_map_iter_advance(&iter)) {
        value_t key;
        value_t value;
        id_hash_map_iter_get_current(&iter, &key, &value);
        ASSERT_TRUE(bit_vector_get_at(&bits, get_integer_value(key)));
        seen++;
      }
      ASSERT_EQ(get_id_hash_map_size(map), seen);
      ASSERT_EQ(bits_set, seen);
    }
  }

  bit_vector_dispose(&bits);

  DISPOSE_RUNTIME();
}
