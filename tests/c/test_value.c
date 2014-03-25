// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "heap.h"
#include "log.h"
#include "runtime.h"
#include "test.h"
#include "try-inl.h"
#include "value-inl.h"

// Checks whether the value fits in a tagged integer by actually storing it,
// getting the value back out, and testing whether it could be restored. This is
// an extra sanity check.
static bool try_tagging_as_integer(int64_t value) {
  int64_t encoded = (value << 3);
  int64_t decoded = (encoded >> 3);
  return decoded == value;
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
  ASSERT_EQ(sizeof(unknown_value_t), sizeof(encoded_value_t));
  ASSERT_EQ(sizeof(integer_value_t), sizeof(encoded_value_t));
  ASSERT_EQ(sizeof(condition_value_t), sizeof(encoded_value_t));
  ASSERT_EQ(sizeof(custom_tagged_value_t), sizeof(encoded_value_t));
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

// Creates a new integer value using NEW_STATIC_INTEGER.
static value_t new_static_integer(int64_t value) {
  value_t result;
  result.encoded = NEW_STATIC_INTEGER(value);
  return result;
}

TEST(value, static_tagged_integers) {
  value_t v0 = new_static_integer(10);
  ASSERT_DOMAIN(vdInteger, v0);
  ASSERT_EQ(10, get_integer_value(v0));
  value_t v1 = new_static_integer(-10);
  ASSERT_DOMAIN(vdInteger, v1);
  ASSERT_EQ(-10, get_integer_value(v1));
  value_t v2 = new_static_integer(0);
  ASSERT_DOMAIN(vdInteger, v2);
  ASSERT_EQ(0, get_integer_value(v2));
}

TEST(value, family_values) {
  // Test that the integer values of the family enums are integers when viewed
  // as encoded value_ts.
  value_t value;
  value.encoded = ofAmbience;
  ASSERT_DOMAIN(vdInteger, value);
  value.encoded = ofGlobalField;
  ASSERT_DOMAIN(vdInteger, value);
  value.encoded = ofLambda;
  ASSERT_DOMAIN(vdInteger, value);
  value.encoded = ofReference;
  ASSERT_DOMAIN(vdInteger, value);
  value.encoded = ofWithEscapeAst;
  ASSERT_DOMAIN(vdInteger, value);
}

TEST(value, conditions) {
  value_t v0 = new_condition(ccHeapExhausted);
  ASSERT_DOMAIN(vdCondition, v0);
  ASSERT_EQ(ccHeapExhausted, get_condition_cause(v0));
}

TEST(value, custom_tagged) {
  value_t n0 = new_custom_tagged(tpNull, 0);
  ASSERT_EQ(0, get_custom_tagged_payload(n0));
  value_t n1 = new_custom_tagged(tpNull, 255);
  ASSERT_EQ(255, get_custom_tagged_payload(n1));
  value_t n2 = new_custom_tagged(tpNull, (1LL << 46));
  ASSERT_EQ((1LL << 46), get_custom_tagged_payload(n2));
  value_t n3 = new_custom_tagged(tpNull, -(1LL << 46));
  ASSERT_EQ(-(1LL << 46), get_custom_tagged_payload(n3));
}

TEST(value, objects) {
  heap_t heap;
  ASSERT_SUCCESS(heap_init(&heap, NULL));

  address_t addr;
  ASSERT_TRUE(heap_try_alloc(&heap, 16, &addr));
  value_t v0 = new_heap_object(addr);
  ASSERT_DOMAIN(vdHeapObject, v0);
  ASSERT_PTREQ(addr, get_heap_object_address(v0));

  heap_dispose(&heap);
}

TEST(value, id_hash_maps_simple) {
  CREATE_RUNTIME();

  // Create a map.
  value_t map = new_heap_id_hash_map(runtime, 4);
  ASSERT_FAMILY(ofIdHashMap, map);
  ASSERT_EQ(0, get_id_hash_map_size(map));
  ASSERT_CONDITION(ccNotFound, get_id_hash_map_at(map, new_integer(0)));
  // Add something to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(1), false));
  ASSERT_EQ(1, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_CONDITION(ccNotFound, get_id_hash_map_at(map, new_integer(1)));
  // Add some more to it.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(2), false));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(1), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // Replace an existing value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(0), new_integer(3), false));
  ASSERT_EQ(2, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  // There's room for one more value.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(100), new_integer(5), false));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_SAME(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // Now the map should refuse to let us add more.
  ASSERT_CONDITION(ccMapFull, try_set_id_hash_map_at(map, new_integer(88),
      new_integer(79), false));
  ASSERT_EQ(3, get_id_hash_map_size(map));
  ASSERT_SAME(new_integer(3), get_id_hash_map_at(map, new_integer(0)));
  ASSERT_SAME(new_integer(2), get_id_hash_map_at(map, new_integer(1)));
  ASSERT_SAME(new_integer(5), get_id_hash_map_at(map, new_integer(100)));
  // However it should still be possible to replace existing mappings.
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(1), new_integer(9), false));
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
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, one, new_integer(4), false));
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
    ASSERT_SUCCESS(heap_object_validate(map));
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
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  config.semispace_size_bytes = 65536;
  runtime_t *runtime = NULL;
  ASSERT_SUCCESS(new_runtime(&config, &runtime));

  value_t map = new_heap_id_hash_map(runtime, 4);
  for (size_t i = 0; true; i++) {
    value_t key = new_integer(i);
    value_t value = new_integer(1024 - i);
    value_t result = set_id_hash_map_at(runtime, map, key, value);
    ASSERT_SUCCESS(heap_object_validate(map));
    if (in_condition_cause(ccHeapExhausted, result))
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
  ASSERT_CHECK_FAILURE(ccOutOfBounds, get_array_at(arr, 4));

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
    ASSERT_CHECK_FAILURE(ccOutOfBounds, get_array_buffer_at(buf, i + 1));
  }

  ASSERT_EQ(16, get_array_buffer_length(buf));
  ASSERT_FALSE(try_add_to_array_buffer(buf, new_integer(16)));
  ASSERT_EQ(16, get_array_buffer_length(buf));

  for (size_t i = 16; i < 1024; i++) {
    ASSERT_EQ(i, get_array_buffer_length(buf));
    ASSERT_SUCCESS(add_to_array_buffer(runtime, buf, new_integer(i)));
    ASSERT_VALEQ(new_integer(i / 2), get_array_buffer_at(buf, i / 2));
    ASSERT_CHECK_FAILURE(ccOutOfBounds, get_array_buffer_at(buf, i + 1));
  }

  DISPOSE_RUNTIME();
}

TEST(value, array_buffer_empty) {
  CREATE_RUNTIME();

  value_t buf = new_heap_array_buffer_with_contents(runtime, ROOT(runtime, empty_array));
  ASSERT_SUCCESS(buf);
  ASSERT_SUCCESS(add_to_array_buffer(runtime, buf, new_integer(9)));

  DISPOSE_RUNTIME();
}

TEST(value, get_primary_type) {
  CREATE_RUNTIME();

  value_t int_proto = get_primary_type(new_integer(2), runtime);
  ASSERT_VALEQ(int_proto, ROOT(runtime, integer_type));
  ASSERT_VALEQ(int_proto, get_primary_type(new_integer(6), runtime));
  value_t null_proto = get_primary_type(null(), runtime);
  ASSERT_FALSE(value_structural_equal(int_proto, null_proto));
  ASSERT_VALEQ(null_proto, ROOT(runtime, null_type));

  DISPOSE_RUNTIME();
}


TEST(value, instance_division) {
  CREATE_RUNTIME();

  value_t proto = new_heap_type(runtime, afFreeze, nothing(), null());
  value_t species = new_heap_instance_species(runtime, proto, nothing());
  value_t instance = new_heap_instance(runtime, species);
  ASSERT_VALEQ(proto, get_instance_species_primary_type_field(species));
  ASSERT_VALEQ(proto, get_instance_primary_type_field(instance));
  ASSERT_VALEQ(proto, get_primary_type(instance, runtime));

  DISPOSE_RUNTIME();
}


TEST(value, integer_comparison) {
#define ASSERT_INT_COMPARE(A, B, REL)                                          \
  ASSERT_TRUE(test_relation(value_ordering_compare(new_integer(A), new_integer(B)), REL))

  ASSERT_INT_COMPARE(0, 1, reLessThan);
  ASSERT_INT_COMPARE(0, 0, reEqual);
  ASSERT_INT_COMPARE(2, 1, reGreaterThan);

#undef ASSERT_INT_COMPARE
}

TEST(value, string_comparison) {
  CREATE_RUNTIME();

  // Checks that the string with contents A compares to B as the given operator.
#define ASSERT_STR_COMPARE(A, B, REL) do {                                     \
  string_t a_str = new_string(A);                                                     \
  value_t a = new_heap_string(runtime, &a_str);                                \
  string_t b_str = new_string(B);                                                     \
  value_t b = new_heap_string(runtime, &b_str);                                \
  ASSERT_TRUE(test_relation(value_ordering_compare(a, b), REL));               \
} while (false)

  ASSERT_STR_COMPARE("", "", reEqual);
  ASSERT_STR_COMPARE("", "x", reLessThan);
  ASSERT_STR_COMPARE("", "xx", reLessThan);
  ASSERT_STR_COMPARE("x", "xx", reLessThan);
  ASSERT_STR_COMPARE("xx", "xx", reEqual);
  ASSERT_STR_COMPARE("xxx", "xx", reGreaterThan);
  ASSERT_STR_COMPARE("xy", "xx", reGreaterThan);
  ASSERT_STR_COMPARE("yx", "xx", reGreaterThan);
  ASSERT_STR_COMPARE("yx", "x", reGreaterThan);
  ASSERT_STR_COMPARE("wx", "x", reGreaterThan);

#undef ASSERT_STR_COMPARE

  DISPOSE_RUNTIME();
}

TEST(value, bool_comparison) {
  CREATE_RUNTIME();

  value_t t = yes();
  value_t f = no();

  ASSERT_TRUE(test_relation(value_ordering_compare(t, t), reEqual));
  ASSERT_TRUE(test_relation(value_ordering_compare(f, f), reEqual));
  ASSERT_TRUE(test_relation(value_ordering_compare(t, f), reGreaterThan));
  ASSERT_TRUE(test_relation(value_ordering_compare(f, t), reLessThan));

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
  ASSERT_TRUE(is_array_sorted(ROOT(runtime, empty_array)));
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
      ASSERT_CONDITION(ccNotFound, found);
    }
  }

  DISPOSE_RUNTIME();
}

static const size_t kMapCount = 8;
static const size_t kInstanceCount = 128;

// Checks that the instances are present in the maps as expected, skipping the
// first skip_first entries. This is such that we can gradually dispose the
// maps.
static void assert_strings_present(size_t skip_first, safe_value_t *s_maps,
    safe_value_t *s_insts) {
  for (size_t inst_i = 0; inst_i < kInstanceCount; inst_i++) {
    value_t inst = deref(s_insts[inst_i]);
    for (size_t map_i = skip_first; map_i < kMapCount; map_i++) {
      value_t map = deref(s_maps[map_i]);
      bool should_be_present = ((inst_i % (map_i + 1)) == 0);
      value_t value = get_id_hash_map_at(map, inst);
      if (should_be_present) {
        ASSERT_SAME(inst, value);
        value_t field = get_instance_field(value, new_integer(0));
        ASSERT_VALEQ(new_integer(inst_i), field);
      } else {
        ASSERT_CONDITION(ccNotFound, value);
      }
    }
  }
}

TEST(value, rehash_map) {
  CREATE_RUNTIME();

  // Create and retain a number of maps.
  safe_value_t s_maps[8];
  for (size_t i = 0; i < kMapCount; i++) {
    value_t map = new_heap_id_hash_map(runtime, 16);
    s_maps[i] = runtime_protect_value(runtime, map);
  }

  // Build and retain a number of strings. We'll use these as keys.
  safe_value_t s_insts[128];
  for (size_t i = 0; i < kInstanceCount; i++) {
    value_t inst = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
    ASSERT_SUCCESS(set_instance_field(runtime, inst, new_integer(0), new_integer(i)));
    s_insts[i] = runtime_protect_value(runtime, inst);
  }

  // Store the strings sort-of randomly in the maps.
  for (size_t inst_i = 0; inst_i < kInstanceCount; inst_i++) {
    value_t inst = deref(s_insts[inst_i]);
    for (size_t map_i = 0; map_i < kMapCount; map_i++) {
      if ((inst_i % (map_i + 1)) == 0) {
        // If the map's index (plus 1 to avoid 0) is a divisor in the string's
        // index we add it to the map. This means that the 0th map gets all
        // strings whereas the 15th get 1/15.
        value_t map = deref(s_maps[map_i]);
        ASSERT_SUCCESS(set_id_hash_map_at(runtime, map, inst, inst));
      }
    }
  }

  assert_strings_present(0, s_maps, s_insts);
  runtime_garbage_collect(runtime);
  assert_strings_present(0, s_maps, s_insts);

  for (size_t i = 0; i < kMapCount; i++) {
    // Dispose the maps one at a time and then garbage collect to get them
    // to move around.
    dispose_safe_value(runtime, s_maps[i]);
    runtime_garbage_collect(runtime);
    assert_strings_present(i + 1, s_maps, s_insts);
  }

  // Give back the instance handles.
  for (size_t i = 0; i < kInstanceCount; i++)
    dispose_safe_value(runtime, s_insts[i]);

  DISPOSE_RUNTIME();
}

TEST(value, map_delete) {
  CREATE_RUNTIME();

  // Bit set to keep track of which entries are set in the map.
  static const size_t kRange = 129;
  bit_vector_t bits;
  bit_vector_init(&bits, kRange, false);
  size_t bits_set = 0;

  pseudo_random_t rand;
  pseudo_random_init(&rand, 35234);

  value_t map = new_heap_id_hash_map(runtime, kRange + 5);
  for (size_t t = 0; t <= 1024; t++) {
    ASSERT_EQ(bits_set, get_id_hash_map_size(map));
    // Pick a random element to change.
    size_t index = pseudo_random_next(&rand, kRange);
    value_t key = new_integer(index);
    if (bit_vector_get_at(&bits, index)) {
      ASSERT_SUCCESS(delete_id_hash_map_at(runtime, map, key));
      bit_vector_set_at(&bits, index, false);
      bits_set--;
    } else {
      ASSERT_CONDITION(ccNotFound, delete_id_hash_map_at(runtime, map, key));
      ASSERT_SUCCESS(try_set_id_hash_map_at(map, key, key, false));
      bit_vector_set_at(&bits, index, true);
      bits_set++;
    }
    if ((t % 64) == 0) {
      // Check that getting the values directly works.
      for (size_t i = 0; i < kRange; i++) {
        value_t key = new_integer(i);
        bool in_map = !in_condition_cause(ccNotFound, get_id_hash_map_at(map, key));
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

// The description of an argument map.
typedef struct {
  size_t length;
  size_t *values;
} test_argument_map_t;

// Creates a new random test argument map. Remember to dispose it after use.
void new_test_argument_map(test_argument_map_t *map, pseudo_random_t *random) {
  size_t length = map->length = 4 + pseudo_random_next(random, 8);
  size_t *values = map->values = (size_t*) malloc(length * sizeof(size_t));
  for (size_t i = 0; i < length; i++)
    values[i] = i;
  pseudo_random_shuffle(random, values, length, sizeof(size_t));
}

// Disposes a test argument map.
void dispose_test_argument_map(test_argument_map_t *map) {
  free(map->values);
}

// Returns an argument map that matches the given test data, looking it up using
// the given argument trie.
value_t get_argument_map(runtime_t *runtime, value_t root, test_argument_map_t *data) {
  value_t current = root;
  for (size_t i = 0; i < data->length; i++)
    TRY_SET(current, get_argument_map_trie_child(runtime, current,
        new_integer(data->values[i])));
  return get_argument_map_trie_value(current);
}

TEST(value, argument_map_tries) {
  CREATE_RUNTIME();

  pseudo_random_t random;
  pseudo_random_init(&random, 4234523);
  value_t root = new_heap_argument_map_trie(runtime, ROOT(runtime, empty_array));

  // Build a set of test data.
  static const size_t kSampleSize = 129;
  test_argument_map_t test_maps[129];
  for (size_t i = 0; i < kSampleSize; i++)
    new_test_argument_map(&test_maps[i], &random);

  // Read out all the maps we're going to test but only test them afterwards to
  // ensure that they stay valid after more maps have been returned.
  value_t maps[129];
  for (size_t i = 0; i < kSampleSize; i++) {
    value_t map = get_argument_map(runtime, root, &test_maps[i]);
    ASSERT_SUCCESS(map);
    maps[i] = map;
  }

  // Check that we got back the expected results.
  for (size_t i = 0; i < kSampleSize; i++) {
    test_argument_map_t *test_map = &test_maps[i];
    value_t map = maps[i];
    for (size_t i = 0; i < test_map->length; i++)
      ASSERT_EQ(test_map->values[i], get_integer_value(get_array_at(map, i)));
  }

  // Check that calling again gets back the exact same maps.
  for (size_t i = 0; i < kSampleSize; i++) {
    value_t map = get_argument_map(runtime, root, &test_maps[i]);
    ASSERT_SAME(maps[i], map);
  }

  // Free the test data.
  for (size_t i = 0; i < kSampleSize; i++)
    dispose_test_argument_map(&test_maps[i]);

  DISPOSE_RUNTIME();
}

typedef struct {
  bool called;
} try_finally_data_t;

static value_t try_finally_condition(try_finally_data_t *data) {
  E_BEGIN_TRY_FINALLY();
    E_TRY(new_condition(ccNothing));
    E_RETURN(success());
  E_FINALLY();
    data->called = true;
  E_END_TRY_FINALLY();
}

static value_t try_finally_return(try_finally_data_t *data) {
  E_BEGIN_TRY_FINALLY();
    E_TRY(success());
    E_RETURN(new_integer(4));
  E_FINALLY();
    data->called = true;
  E_END_TRY_FINALLY();
}

TEST(value, try_finally) {
  try_finally_data_t data = {false};

  ASSERT_CONDITION(ccNothing, try_finally_condition(&data));
  ASSERT_TRUE(data.called);
  data.called = false;

  value_t value = try_finally_return(&data);
  ASSERT_VALEQ(new_integer(4), value);
  ASSERT_TRUE(data.called);
}

TEST(value, array_identity) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t v_nn_0 = C(vArray(vNull(), vNull()));
  value_t v_nn_1 = C(vArray(vNull(), vNull()));
  ASSERT_TRUE(value_identity_compare(v_nn_0, v_nn_1));
  size_t h_nn_0 = get_integer_value(value_transient_identity_hash(v_nn_0));
  size_t h_nn_1 = get_integer_value(value_transient_identity_hash(v_nn_1));
  ASSERT_EQ(h_nn_0, h_nn_1);

  value_t v_1n = C(vArray(vInt(1), vNull()));
  ASSERT_FALSE(value_identity_compare(v_1n, v_nn_0));
  size_t h_1n = get_integer_value(value_transient_identity_hash(v_1n));
  ASSERT_FALSE(h_nn_0 == h_1n);

  value_t v_12 = C(vArray(vInt(1), vInt(2)));
  ASSERT_FALSE(value_identity_compare(v_12, v_nn_0));
  ASSERT_FALSE(value_identity_compare(v_12, v_1n));
  size_t h_12 = get_integer_value(value_transient_identity_hash(v_12));
  ASSERT_FALSE(h_nn_0 == h_12);
  ASSERT_FALSE(h_1n == h_12);

  value_t v_21_0 = C(vArray(vInt(2), vInt(1)));
  ASSERT_FALSE(value_identity_compare(v_21_0, v_nn_0));
  ASSERT_FALSE(value_identity_compare(v_21_0, v_1n));
  ASSERT_FALSE(value_identity_compare(v_21_0, v_12));
  size_t h_21_0 = get_integer_value(value_transient_identity_hash(v_21_0));
  ASSERT_FALSE(h_21_0 == h_nn_0);
  ASSERT_FALSE(h_21_0 == h_1n);
  ASSERT_FALSE(h_21_0 == h_12);

  value_t v_21_1 = C(vArray(vInt(2), vInt(1)));
  ASSERT_TRUE(value_identity_compare(v_21_1, v_21_0));
  size_t h_21_1 = get_integer_value(value_transient_identity_hash(v_21_1));
  ASSERT_EQ(h_21_1, h_21_0);

  value_t v_nv_0 = new_heap_array(runtime, 2);
  set_array_at(v_nv_0, 1, v_nv_0);
  ASSERT_CONDITION(ccCircular, value_transient_identity_hash(v_nv_0));
  ASSERT_TRUE(value_identity_compare(v_nv_0, v_nv_0));

  value_t v_nv_1 = new_heap_array(runtime, 2);
  set_array_at(v_nv_1, 1, v_nv_1);
  ASSERT_CONDITION(ccCircular, value_transient_identity_hash(v_nv_1));
  ASSERT_TRUE(value_identity_compare(v_nv_1, v_nv_1));

  ASSERT_FALSE(value_identity_compare(v_nv_0, v_nv_1));

  value_t deep = new_heap_array(runtime, 1);
  for (size_t i = 0; i < 1024; i++) {
    value_t new_deep = new_heap_array(runtime, 1);
    set_array_at(new_deep, 0, deep);
    deep = new_deep;
  }
  ASSERT_TRUE(value_identity_compare(deep, deep));
  value_t hdeep = value_transient_identity_hash(deep);
  ASSERT_SUCCESS(hdeep);

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

#undef V2V

TEST(value, set_value_mode) {
  CREATE_RUNTIME();

  value_t arr = new_heap_array(runtime, 3);
  ASSERT_TRUE(is_mutable(arr));
  ASSERT_FALSE(is_frozen(arr));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, arr, vmFluid));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, arr));
  ASSERT_TRUE(is_frozen(arr));
  ASSERT_FALSE(is_mutable(arr));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, arr, vmFluid));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, arr, vmMutable));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, arr));
  ASSERT_TRUE(is_frozen(arr));

  ASSERT_TRUE(is_frozen(null()));
  ASSERT_FALSE(is_mutable(null()));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, null(), vmFluid));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, null(), vmMutable));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, null()));

  value_t zero = new_integer(0);
  ASSERT_TRUE(is_frozen(zero));
  ASSERT_FALSE(is_mutable(zero));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, zero, vmFluid));
  ASSERT_CONDITION(ccInvalidModeChange, set_value_mode(runtime, zero, vmMutable));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, zero));

  DISPOSE_RUNTIME();
}

#define CHECK_UNSUPPORTED(vdDomain, ofFamily, ubCause, EXPECTED) do {          \
  value_t condition = new_unsupported_behavior_condition(vdDomain, ofFamily, ubCause);\
  value_to_string_t to_string;                                                 \
  const char *found = value_to_string(&to_string, condition);                     \
  ASSERT_C_STREQ(EXPECTED, found);                                             \
  dispose_value_to_string(&to_string);                                         \
} while (false)

TEST(value, unsupported) {
  CHECK_UNSUPPORTED(vdInteger, __ofUnknown__, ubUnspecified,
      "%<condition: UnsupportedBehavior(Unspecified of Integer)>");
  CHECK_UNSUPPORTED(vdHeapObject, __ofUnknown__, ubSetContents,
      "%<condition: UnsupportedBehavior(SetContents of HeapObject)>");
  CHECK_UNSUPPORTED(vdHeapObject, ofArray, ubPlanktonSerialize,
      "%<condition: UnsupportedBehavior(PlanktonSerialize of HeapObject/Array)>");
}

TEST(value, invalid_input) {
  string_hint_t halp = STRING_HINT_INIT("halp!");
  value_t condition = new_invalid_input_condition_with_hint(halp);
  value_to_string_t to_string;
  ASSERT_C_STREQ("%<condition: InvalidInput(ha..p!)>", value_to_string(&to_string, condition));
  dispose_value_to_string(&to_string);
}


TEST(value, paths) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t empty = ROOT(runtime, empty_path);
  ASSERT_CHECK_FAILURE(ccEmptyPath, get_path_head(empty));
  ASSERT_CHECK_FAILURE(ccEmptyPath, get_path_tail(empty));
  ASSERT_SAME(nothing(), get_path_raw_head(empty));
  ASSERT_SAME(nothing(), get_path_raw_tail(empty));

  value_t segments = variant_to_value(runtime, vArray(vStr("a"), vStr("b"),
      vStr("c")));
  value_t path = new_heap_path_with_names(runtime, segments, 0);
  ASSERT_VAREQ(vStr("a"), get_path_head(path));
  ASSERT_VAREQ(vStr("b"), get_path_head(get_path_tail(path)));
  ASSERT_VAREQ(vStr("c"), get_path_head(get_path_tail(get_path_tail(path))));
  ASSERT_TRUE(is_path_empty(get_path_tail(get_path_tail(get_path_tail(path)))));

  value_to_string_t to_string;
  const char *found = value_to_string(&to_string, path);
  ASSERT_C_STREQ(":a:b:c", found);
  dispose_value_to_string(&to_string);

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

value_t new_flag_element(runtime_t *runtime, value_t key, value_t value) {
  value_t payload = new_heap_id_hash_map(runtime, 16);
  set_id_hash_map_at(runtime, payload, RSTR(runtime, key), key);
  set_id_hash_map_at(runtime, payload, RSTR(runtime, value), value);
  value_t path = new_heap_array(runtime, 2);
  set_array_at(path, 0, RSTR(runtime, options));
  set_array_at(path, 1, RSTR(runtime, FlagElement));
  return new_heap_unknown(runtime,
      new_heap_unknown(runtime,
          RSTR(runtime, environment_reference),
          path),
      payload);
}

value_t new_options(runtime_t *runtime, size_t count, value_t* elements) {
  value_t array = new_heap_array(runtime, count);
  for (size_t i = 0; i < count; i++)
    set_array_at(array, i, elements[i]);
  return new_heap_options(runtime, array);
}

TEST(value, options) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t x = variant_to_value(runtime, vStr("x"));
  value_t y = variant_to_value(runtime, vStr("y"));
  value_t z = variant_to_value(runtime, vStr("z"));

  value_t empty = new_options(runtime, 0, NULL);
  ASSERT_VAREQ(vInt(9), get_options_flag_value(runtime, empty, x, new_integer(9)));
  ASSERT_VAREQ(vInt(10), get_options_flag_value(runtime, empty, y, new_integer(10)));
  ASSERT_VAREQ(vInt(11), get_options_flag_value(runtime, empty, z, new_integer(11)));

  value_t has_x_elms[1] = {new_flag_element(runtime, x, new_integer(60))};
  value_t has_x = new_options(runtime, 1, has_x_elms);
  ASSERT_VAREQ(vInt(60), get_options_flag_value(runtime, has_x, x, new_integer(12)));
  ASSERT_VAREQ(vInt(13), get_options_flag_value(runtime, has_x, y, new_integer(13)));
  ASSERT_VAREQ(vInt(14), get_options_flag_value(runtime, has_x, z, new_integer(14)));

  value_t has_xy_elms[2] = {
    new_flag_element(runtime, x, new_integer(61)),
    new_flag_element(runtime, y, new_integer(62))
  };
  value_t has_xy = new_options(runtime, 2, has_xy_elms);
  ASSERT_VAREQ(vInt(61), get_options_flag_value(runtime, has_xy, x, new_integer(15)));
  ASSERT_VAREQ(vInt(62), get_options_flag_value(runtime, has_xy, y, new_integer(16)));
  ASSERT_VAREQ(vInt(17), get_options_flag_value(runtime, has_xy, z, new_integer(17)));

  value_t has_xyz_elms[3] = {
    new_flag_element(runtime, x, new_integer(63)),
    new_flag_element(runtime, y, new_integer(64)),
    new_flag_element(runtime, z, new_integer(65))
  };
  value_t has_xyz = new_options(runtime, 3, has_xyz_elms);
  ASSERT_VAREQ(vInt(63), get_options_flag_value(runtime, has_xyz, x, new_integer(18)));
  ASSERT_VAREQ(vInt(64), get_options_flag_value(runtime, has_xyz, y, new_integer(19)));
  ASSERT_VAREQ(vInt(65), get_options_flag_value(runtime, has_xyz, z, new_integer(20)));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}


TEST(value, reference) {
  CREATE_RUNTIME();

  value_t ref = new_heap_reference(runtime, null());
  ASSERT_VALEQ(null(), get_reference_value(ref));
  set_reference_value(ref, new_integer(0));
  ASSERT_VALEQ(new_integer(0), get_reference_value(ref));
  ASSERT_SUCCESS(ensure_frozen(runtime, ref));

  DISPOSE_RUNTIME();
}


TEST(value, ambience) {
  CREATE_RUNTIME();

  ASSERT_PTREQ(runtime, get_ambience_runtime(ambience));
  value_t redirect = get_ambience_present_core_fragment_redirect();
  ASSERT_VALEQ(nothing(), get_ambience_present_core_fragment(ambience));
  ASSERT_VALEQ(nothing(), follow_ambience_redirect(ambience, redirect));
  ASSERT_VALEQ(nothing(), get_type_origin(ROOT(runtime, string_type), ambience));

  value_t frag = new_heap_module_fragment(runtime, present_stage(), nothing(),
      nothing(), nothing(), nothing(), nothing());
  set_ambience_present_core_fragment(ambience, frag);
  ASSERT_VALEQ(frag, get_ambience_present_core_fragment(ambience));
  ASSERT_VALEQ(frag, follow_ambience_redirect(ambience, redirect));
  ASSERT_VALEQ(frag, get_type_origin(ROOT(runtime, string_type), ambience));

  DISPOSE_RUNTIME();
}
