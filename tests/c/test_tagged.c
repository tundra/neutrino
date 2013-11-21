// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "tagged.h"
#include "test.h"

TEST(tagged, relation) {
  ASSERT_TRUE(test_relation(less_than(), reLessThan));
  ASSERT_TRUE(test_relation(less_than(), reLessThan | reEqual));
  ASSERT_FALSE(test_relation(less_than(), reEqual));
  ASSERT_FALSE(test_relation(less_than(), reGreaterThan));
  ASSERT_FALSE(test_relation(less_than(), reGreaterThan | reEqual));

  ASSERT_FALSE(test_relation(equal(),  reLessThan));
  ASSERT_TRUE(test_relation(equal(),  reLessThan | reEqual));
  ASSERT_TRUE(test_relation(equal(), reEqual));
  ASSERT_FALSE(test_relation(equal(), reGreaterThan));
  ASSERT_TRUE(test_relation(equal(), reGreaterThan | reEqual));

  ASSERT_FALSE(test_relation(greater_than(),  reLessThan));
  ASSERT_FALSE(test_relation(greater_than(),  reLessThan | reEqual));
  ASSERT_FALSE(test_relation(greater_than(), reEqual));
  ASSERT_TRUE(test_relation(greater_than(), reGreaterThan));
  ASSERT_TRUE(test_relation(greater_than(), reGreaterThan | reEqual));

  ASSERT_FALSE(test_relation(unordered(),  reLessThan));
  ASSERT_FALSE(test_relation(unordered(),  reLessThan | reEqual));
  ASSERT_FALSE(test_relation(unordered(), reEqual));
  ASSERT_FALSE(test_relation(unordered(), reGreaterThan));
  ASSERT_FALSE(test_relation(unordered(), reGreaterThan | reEqual));
}

TEST(tagged, integer_comparison) {
  ASSERT_TRUE(test_relation(compare_signed_integers(-1, 1), reLessThan));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, 1), reLessThan));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, 0), reEqual));
  ASSERT_TRUE(test_relation(compare_signed_integers(0, -1), reGreaterThan));
}
