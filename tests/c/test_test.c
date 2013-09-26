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
  ASSERT_VALEQ(ROOT(runtime, null), variant_to_value(runtime, vNull()));

  string_t str = STR("blahblahblah");
  ASSERT_VALEQ(new_heap_string(runtime, &str), variant_to_value(runtime,
      vStr("blahblahblah")));

  value_t arr = variant_to_value(runtime, vArray(3, vInt(0) o vInt(1) o vInt(2)));
  ASSERT_EQ(3, get_array_length(arr));
  ASSERT_VALEQ(new_integer(0), get_array_at(arr, 0));
  ASSERT_VALEQ(new_integer(1), get_array_at(arr, 1));
  ASSERT_VALEQ(new_integer(2), get_array_at(arr, 2));

  value_t val = variant_to_value(runtime, vValue(ROOT(runtime, empty_array)));
  ASSERT_SAME(ROOT(runtime, empty_array), val);

  DISPOSE_RUNTIME();
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
