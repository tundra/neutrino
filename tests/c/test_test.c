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
