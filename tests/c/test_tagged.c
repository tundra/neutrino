// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "tagged-inl.h"
#include "test.h"

TEST(tagged, relation) {
  ASSERT_TRUE(test_relation(less_than(), reLessThan));
  ASSERT_TRUE(test_relation(less_than(), reLessThan | reEqual));
  ASSERT_FALSE(test_relation(less_than(), reEqual));
  ASSERT_FALSE(test_relation(less_than(), reGreaterThan));
  ASSERT_FALSE(test_relation(less_than(), reGreaterThan | reEqual));
  ASSERT_FALSE(test_relation(less_than(), reUnordered));

  ASSERT_FALSE(test_relation(equal(), reLessThan));
  ASSERT_TRUE(test_relation(equal(), reLessThan | reEqual));
  ASSERT_TRUE(test_relation(equal(), reEqual));
  ASSERT_FALSE(test_relation(equal(), reGreaterThan));
  ASSERT_TRUE(test_relation(equal(), reGreaterThan | reEqual));
  ASSERT_FALSE(test_relation(equal(), reUnordered));

  ASSERT_FALSE(test_relation(greater_than(), reLessThan));
  ASSERT_FALSE(test_relation(greater_than(), reLessThan | reEqual));
  ASSERT_FALSE(test_relation(greater_than(), reEqual));
  ASSERT_TRUE(test_relation(greater_than(), reGreaterThan));
  ASSERT_TRUE(test_relation(greater_than(), reGreaterThan | reEqual));
  ASSERT_FALSE(test_relation(greater_than(), reUnordered));

  ASSERT_FALSE(test_relation(unordered(), reLessThan));
  ASSERT_FALSE(test_relation(unordered(), reLessThan | reEqual));
  ASSERT_FALSE(test_relation(unordered(), reEqual));
  ASSERT_FALSE(test_relation(unordered(), reGreaterThan));
  ASSERT_FALSE(test_relation(unordered(), reGreaterThan | reEqual));
  ASSERT_TRUE(test_relation(unordered(), reUnordered));
}

TEST(tagged, integer_comparison) {
  ASSERT_TRUE(test_relation(compare_signed_integers(-1, 1), reLessThan));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, 1), reLessThan));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, 0), reEqual));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, -1), reGreaterThan));
}

TEST(tagged, float_32) {
  ASSERT_EQ(sizeof(uint32_t), sizeof(float32_t));

  value_t one = new_float_32(1.0);
  ASSERT_TRUE(get_float_32_value(one) == 1.0);
  value_t zero = new_float_32(0.0);
  ASSERT_TRUE(get_float_32_value(zero) == 0.0);
  value_t minus_one = new_float_32(-1.0);
  ASSERT_TRUE(get_float_32_value(minus_one) == -1.0);

  ASSERT_VALEQ(equal(), value_ordering_compare(one, one));
  ASSERT_VALEQ(equal(), value_ordering_compare(zero, zero));
  ASSERT_VALEQ(equal(), value_ordering_compare(minus_one, minus_one));
  ASSERT_VALEQ(less_than(), value_ordering_compare(minus_one, zero));
  ASSERT_VALEQ(less_than(), value_ordering_compare(zero, one));
  ASSERT_VALEQ(greater_than(), value_ordering_compare(zero, minus_one));
  ASSERT_VALEQ(greater_than(), value_ordering_compare(one, zero));

  value_t nan = float_32_nan();
  ASSERT_VALEQ(unordered(), value_ordering_compare(nan, nan));
  ASSERT_VALEQ(unordered(), value_ordering_compare(nan, one));
  ASSERT_VALEQ(unordered(), value_ordering_compare(nan, zero));
  ASSERT_VALEQ(unordered(), value_ordering_compare(zero, nan));
  ASSERT_VALEQ(unordered(), value_ordering_compare(one, nan));
  ASSERT_SAME(nan, nan);

  value_t inf = float_32_infinity();
  value_t minf = float_32_minus_infinity();
  ASSERT_VALEQ(unordered(), value_ordering_compare(nan, inf));
  ASSERT_VALEQ(unordered(), value_ordering_compare(inf, nan));
  ASSERT_VALEQ(unordered(), value_ordering_compare(nan, minf));
  ASSERT_VALEQ(unordered(), value_ordering_compare(minf, nan));
  ASSERT_VALEQ(less_than(), value_ordering_compare(one, inf));
  ASSERT_VALEQ(greater_than(), value_ordering_compare(inf, one));
  ASSERT_VALEQ(greater_than(), value_ordering_compare(one, minf));
  ASSERT_VALEQ(less_than(), value_ordering_compare(minf, one));
  ASSERT_VALEQ(equal(), value_ordering_compare(inf, inf));
  ASSERT_VALEQ(greater_than(), value_ordering_compare(inf, minf));
  ASSERT_VALEQ(less_than(), value_ordering_compare(minf, inf));

  ASSERT_TRUE(is_float_32_nan(nan));
  ASSERT_FALSE(is_float_32_nan(minus_one));
  ASSERT_FALSE(is_float_32_nan(zero));
  ASSERT_FALSE(is_float_32_nan(one));
  ASSERT_FALSE(is_float_32_nan(inf));
  ASSERT_FALSE(is_float_32_nan(minf));

  ASSERT_FALSE(is_float_32_finite(nan));
  ASSERT_TRUE(is_float_32_finite(minus_one));
  ASSERT_TRUE(is_float_32_finite(zero));
  ASSERT_TRUE(is_float_32_finite(one));
  ASSERT_FALSE(is_float_32_finite(inf));
  ASSERT_FALSE(is_float_32_finite(minf));
}

TEST(tagged, tiny_bit_set) {
  // Initialization.
  value_t regular = new_flag_set(kFlagSetAllOff);
  for (size_t i = 0; i < kFlagSetMaxSize; i++)
    ASSERT_FALSE(get_flag_set_at(regular, 1 << i));
  value_t inverse = new_flag_set(kFlagSetAllOn);
  for (size_t i = 0; i < kFlagSetMaxSize; i++)
    ASSERT_TRUE(get_flag_set_at(inverse, 1 << i));

  // Setting/getting.
  pseudo_random_t random;
  pseudo_random_init(&random, 42342);
  uint64_t bits = 0;
  for (size_t i = 0; i < 1024; i++) {
    size_t index = pseudo_random_next(&random, kFlagSetMaxSize);
    bool value = pseudo_random_next(&random, 2);
    regular = set_flag_set_at(regular, 1 << index, value);
    inverse = set_flag_set_at(inverse, 1 << index, !value);
    if (value)
      bits = bits | (1LL << index);
    else
      bits = bits & ~(1LL << index);
    for (size_t i = 0; i < kFlagSetMaxSize; i++) {
      bool bit = (bits & (1LL << i)) != 0;
      ASSERT_EQ(bit, get_flag_set_at(regular, 1 << i));
      ASSERT_EQ(!bit, get_flag_set_at(inverse, 1 << i));
    }
  }
}
