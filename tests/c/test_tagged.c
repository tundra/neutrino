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

// Checks that a score created with the given attributes works as expected.
// Returns the score.
static value_t test_new_score(score_category_t category, uint32_t subscore) {
  value_t score = new_score(category, subscore);
  ASSERT_EQ(category, get_score_category(score));
  ASSERT_EQ(subscore, get_score_subscore(score));
  return score;
}

TEST(tagged, new_score) {
  test_new_score(scEq, 0);
  test_new_score(scAny, 0);
  test_new_score(scIs, 0);
  test_new_score(scEq, 100);
  test_new_score(scAny, 100);
  test_new_score(scIs, 100);
  test_new_score(scEq, 0xFFFFFFFF);
  test_new_score(scAny, 0xFFFFFFFF);
  test_new_score(scIs, 0xFFFFFFFF);
}

// Tests that the different score comparison functions give the expected
// results on the given score values.
static void test_score_compare(score_category_t cat_a, uint32_t sub_a,
    unsigned rel, score_category_t cat_b, uint32_t sub_b) {
  // Run the values through the new score test for good measure.
  value_t a = test_new_score(cat_a, sub_a);
  value_t b = test_new_score(cat_b, sub_b);
  ASSERT_TRUE(test_relation(value_ordering_compare(a, b), rel));
  int compared = compare_tagged_scores(a, b);
  ASSERT_TRUE(test_relation(integer_to_relation(compared), rel));
}

TEST(tagged, compare_scores) {
  // Scores compare in the opposite order of what you might expect -- lower
  // values compare greater than. See score_ordering_compare for details.

  test_score_compare(scEq, 1, reLessThan, scEq, 0);
  test_score_compare(scEq, 0, reGreaterThan, scEq, 1);
  test_score_compare(scEq, 1, reEqual, scEq, 1);

  test_score_compare(scIs, 0, reLessThan, scEq, 0);
  test_score_compare(scEq, 0, reGreaterThan, scIs, 0);
  test_score_compare(scIs, 0, reEqual, scIs, 0);

  test_score_compare(scIs, 1, reLessThan, scEq, 0);
  test_score_compare(scIs, 0, reLessThan, scEq, 1);
  test_score_compare(scEq, 1, reGreaterThan, scIs, 0);
  test_score_compare(scEq, 0, reGreaterThan, scIs, 1);

  test_score_compare(scIs, 0xFFFFFFFF, reLessThan, scEq, 0);
  test_score_compare(scIs, 0, reLessThan, scEq, 0xFFFFFFFF);
  test_score_compare(scEq, 0xFFFFFFFF, reGreaterThan, scIs, 0);
  test_score_compare(scEq, 0, reGreaterThan, scIs, 0xFFFFFFFF);

  test_score_compare(scAny, 0, reLessThan, scIs, 0);
  test_score_compare(scAny, 0, reLessThan, scEq, 0);
  test_score_compare(scAny, 0xFFFFFFFF, reLessThan, scIs, 0);
  test_score_compare(scAny, 0xFFFFFFFF, reLessThan, scEq, 0);
}

TEST(tagged, is_score_match) {
  ASSERT_TRUE(is_score_match(test_new_score(scEq, 0)));
  ASSERT_TRUE(is_score_match(test_new_score(scIs, 0)));
  ASSERT_TRUE(is_score_match(test_new_score(scAny, 0)));
  ASSERT_TRUE(is_score_match(test_new_score(scExtra, 0)));
  ASSERT_FALSE(is_score_match(test_new_score(scNone, 0)));
  ASSERT_TRUE(is_score_match(test_new_score(scEq, 0xFFFFFFFF)));
  ASSERT_TRUE(is_score_match(test_new_score(scIs, 0xFFFFFFFF)));
  ASSERT_TRUE(is_score_match(test_new_score(scAny, 0xFFFFFFFF)));
  ASSERT_TRUE(is_score_match(test_new_score(scExtra, 0xFFFFFFFF)));
  ASSERT_FALSE(is_score_match(test_new_score(scNone, 0xFFFFFFFF)));
}

TEST(tagged, score_successor) {
  ASSERT_SAME(new_score(scEq, 1), get_score_successor(new_score(scEq, 0)));
  ASSERT_SAME(new_score(scEq, 2), get_score_successor(new_score(scEq, 1)));
  ASSERT_SAME(new_score(scEq, 0xFFFFFFFF), get_score_successor(new_score(scEq, 0xFFFFFFFE)));
  ASSERT_SAME(new_score(scIs, 1), get_score_successor(new_score(scIs, 0)));
  ASSERT_SAME(new_score(scIs, 2), get_score_successor(new_score(scIs, 1)));
  ASSERT_SAME(new_score(scIs, 0xFFFFFFFF), get_score_successor(new_score(scIs, 0xFFFFFFFE)));
}

TEST(tagged, nothing) {
  value_t not = nothing();
  ASSERT_EQ(ENCODED_NOTHING, not.encoded);
  ASSERT_TRUE(is_nothing(not));
}

TEST(tagged, payload) {
  int64_t v0 = (1LLU << (kCustomTaggedPayloadSize - 1)) - 1;
  value_t t0 = new_custom_tagged(0, v0);
  ASSERT_EQ(v0, get_custom_tagged_payload(t0));

  int64_t v1 = -v0;
  value_t t1 = new_custom_tagged(0, v1);
  ASSERT_EQ(v1, get_custom_tagged_payload(t1));
}
