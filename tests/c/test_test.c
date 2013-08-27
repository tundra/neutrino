// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"

TEST(test, variant) {
  CREATE_RUNTIME();

  ASSERT_VALEQ(new_integer(1), variant_to_value(runtime, vInt(1)));
  ASSERT_VALEQ(new_integer(-1), variant_to_value(runtime, vInt(-1)));
  ASSERT_VALEQ(runtime_bool(runtime, true), variant_to_value(runtime, vBool(true)));
  ASSERT_VALEQ(runtime_bool(runtime, false), variant_to_value(runtime, vBool(false)));
  ASSERT_VALEQ(runtime_null(runtime), variant_to_value(runtime, vNull()));

  string_t str = STR("blahblahblah");
  ASSERT_VALEQ(new_heap_string(runtime, &str), variant_to_value(runtime,
      vStr("blahblahblah")));

  value_t arr = variant_to_value(runtime, vArray(3, vInt(0) o vInt(1) o vInt(2)));
  ASSERT_EQ(3, get_array_length(arr));
  ASSERT_VALEQ(new_integer(0), get_array_at(arr, 0));
  ASSERT_VALEQ(new_integer(1), get_array_at(arr, 1));
  ASSERT_VALEQ(new_integer(2), get_array_at(arr, 2));

  DISPOSE_RUNTIME();
}

TEST(test, pseudo_random) {
  pseudo_random_t rand;
  pseudo_random_init(&rand, 123456);
  static const size_t kBucketCount = 257;
  size_t buckets[1027];
  for (size_t i = 0; i < kBucketCount; i++)
    buckets[i] = 0;
  size_t tries = 1048576;
  for (size_t i = 0; i < tries; i++) {
    size_t index = pseudo_random_next(&rand, kBucketCount);
    ASSERT_TRUE(index < kBucketCount);
    buckets[index]++;
  }
  double mid = tries / kBucketCount;
  size_t min = tries;
  size_t max = 0;
  for (size_t i = 0; i < kBucketCount; i++) {
    size_t bucket = buckets[i];
    if (bucket < min)
      min = bucket;
    if (bucket > max)
      max = bucket;
  }
  double min_dev = (mid - min) / mid;
  double max_dev = (max - mid) / mid;
  ASSERT_TRUE(min_dev <= 0.05);
  ASSERT_TRUE(max_dev <= 0.05);
}

TEST(test, shuffle) {
  pseudo_random_t rand;
  pseudo_random_init(&rand, 654322);

  static const size_t kElemCount = 513;
  size_t elems[513];
  for (size_t i = 0; i < kElemCount; i++)
    elems[i] = i;

  bit_vector_t moved;
  bit_vector_init(&moved, kElemCount, false);
  size_t moved_count = 0;
  for (size_t t = 0; t < 65; t++) {
    pseudo_random_shuffle(&rand, elems, kElemCount, sizeof(size_t));
    bit_vector_t seen;
    bit_vector_init(&seen, kElemCount, false);
    size_t seen_count = 0;
    for (size_t i = 0; i < kElemCount; i++) {
      size_t elem = elems[i];
      if (elem != i && !bit_vector_get_at(&moved, i)) {
        bit_vector_set_at(&moved, i, true);
        moved_count++;
      }
      ASSERT_FALSE(bit_vector_get_at(&seen, elem));
      bit_vector_set_at(&seen, elem, true);
      seen_count++;
    }
    // Check that we saw each element once.
    ASSERT_EQ(kElemCount, seen_count);
    bit_vector_dispose(&seen);
  }
  // Check that all elements have been moved at least once. This is more of a
  // sanity check than anything, the distribution might still be awful but it'll
  // do.
  ASSERT_EQ(kElemCount, moved_count);
  bit_vector_dispose(&moved);
}

// Returns a unique hash of the given permutation.
int64_t calc_permutation_hash(int64_t *entries, size_t entc) {
  int64_t result = 0;
  for (size_t i = 0; i < entc; i++)
    result = (result * entc) + entries[i];
  return result;
}

static int64_t factorial(int64_t n) {
  return (n == 1) ? 1 : n * factorial(n - 1);
}

// Tests the permutation generator for 'count' entries.
static void test_permutations(int64_t *entries, size_t count) {
  for (size_t i = 0; i < count; i++)
    entries[i] = i;
  bit_vector_t seen;
  bit_vector_init(&seen, (1 << (3 * count)), false);
  size_t seen_count = 0;
  do {
    int64_t hash = calc_permutation_hash(entries, count);
    ASSERT_FALSE(bit_vector_get_at(&seen, hash));
    seen_count++;
    bit_vector_set_at(&seen, hash, true);
  } while (advance_lexical_permutation(entries, count));
  ASSERT_EQ(factorial(count), seen_count);
  bit_vector_dispose(&seen);
}

TEST(test, permutations) {
  int64_t entries[9];
  for (size_t i = 2; i < 9; i++)
    test_permutations(entries, i);
}
