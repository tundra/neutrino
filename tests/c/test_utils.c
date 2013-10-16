// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "runtime.h"
#include "test.h"
#include "utils.h"
#include "value-inl.h"

TEST(utils, string_simple) {
  string_t str;
  string_init(&str, "Hello, World!");
  ASSERT_EQ(13, string_length(&str));
  ASSERT_EQ('H', string_char_at(&str, 0));
  ASSERT_EQ('!', string_char_at(&str, 12));
}

TEST(utils, string_comparison) {
  // Use char arrays to ensure that the strings are all stored in different
  // parts of memory.
  char c0[4] = {'f', 'o', 'o', '\0'};
  char c1[4] = {'f', 'o', 'o', '\0'};
  char c2[3] = {'f', 'o', '\0'};
  char c3[4] = {'f', 'o', 'f', '\0'};

  string_t s0;
  string_init(&s0, c0);
  string_t s1;
  string_init(&s1, c1);
  string_t s2;
  string_init(&s2, c2);
  string_t s3;
  string_init(&s3, c3);

#define ASSERT_STR_COMPARE(A, REL, B)                                          \
  ASSERT_TRUE(string_compare(&A, &B) REL 0)

  ASSERT_TRUE(string_equals(&s0, &s0));
  ASSERT_STR_COMPARE(s0, ==, s0);
  ASSERT_TRUE(string_equals(&s0, &s1));
  ASSERT_STR_COMPARE(s0, ==, s1);
  ASSERT_FALSE(string_equals(&s0, &s2));
  ASSERT_STR_COMPARE(s0, >, s2);
  ASSERT_FALSE(string_equals(&s0, &s3));
  ASSERT_STR_COMPARE(s0, >, s3);
  ASSERT_TRUE(string_equals(&s1, &s0));
  ASSERT_STR_COMPARE(s1, ==, s0);
  ASSERT_TRUE(string_equals(&s1, &s1));
  ASSERT_STR_COMPARE(s1, ==, s1);
  ASSERT_FALSE(string_equals(&s1, &s2));
  ASSERT_STR_COMPARE(s1, >, s2);
  ASSERT_FALSE(string_equals(&s1, &s3));
  ASSERT_STR_COMPARE(s1, >, s3);
  ASSERT_FALSE(string_equals(&s2, &s0));
  ASSERT_STR_COMPARE(s2, <, s0);
  ASSERT_FALSE(string_equals(&s2, &s1));
  ASSERT_STR_COMPARE(s2, <, s1);
  ASSERT_TRUE(string_equals(&s2, &s2));
  ASSERT_STR_COMPARE(s2, ==, s2);
  ASSERT_FALSE(string_equals(&s2, &s3));
  ASSERT_STR_COMPARE(s2, <, s3);
  ASSERT_FALSE(string_equals(&s3, &s0));
  ASSERT_STR_COMPARE(s3, <, s0);
  ASSERT_FALSE(string_equals(&s3, &s1));
  ASSERT_STR_COMPARE(s3, <, s1);
  ASSERT_FALSE(string_equals(&s3, &s2));
  ASSERT_STR_COMPARE(s3, >, s2);
  ASSERT_TRUE(string_equals(&s3, &s3));
  ASSERT_STR_COMPARE(s3, ==, s3);

#undef ASSERT_STR_COMPARE
}

TEST(utils, string_buffer_simple) {
  string_buffer_t buf;
  string_buffer_init(&buf);

  string_buffer_printf(&buf, "[%s: %i]", "test", 8);
  string_t str;
  string_buffer_flush(&buf, &str);
  string_t expected = STR("[test: 8]");
  ASSERT_STREQ(&expected, &str);

  string_buffer_dispose(&buf);
}

TEST(utils, string_buffer_concat) {
  string_buffer_t buf;
  string_buffer_init(&buf);

  string_buffer_printf(&buf, "foo");
  string_buffer_printf(&buf, "bar");
  string_buffer_printf(&buf, "baz");
  string_t str;
  string_buffer_flush(&buf, &str);
  string_t expected = STR("foobarbaz");
  ASSERT_STREQ(&expected, &str);

  string_buffer_dispose(&buf);
}

TEST(utils, string_buffer_long) {
  string_buffer_t buf;
  string_buffer_init(&buf);

  // Cons up a really long string.
  for (size_t i = 0; i < 1024; i++)
    string_buffer_printf(&buf, "0123456789");
  // Check that it's correct.
  string_t str;
  string_buffer_flush(&buf, &str);
  ASSERT_EQ(10240, string_length(&str));
  for (size_t i = 0; i < 10240; i++) {
    ASSERT_EQ((char) ('0' + (i % 10)), string_char_at(&str, i));
  }

  string_buffer_dispose(&buf);
}

// Runs a test of a bit vector of the given size.
static void test_bit_vector(size_t size) {
  bit_vector_t false_bits;
  ASSERT_SUCCESS(bit_vector_init(&false_bits, size, false));
  bit_vector_t true_bits;
  ASSERT_SUCCESS(bit_vector_init(&true_bits, size, true));

  // Check that the vectors have been initialized as expected.
  for (size_t i = 0; i < size; i++) {
    ASSERT_FALSE(bit_vector_get_at(&false_bits, i));
    ASSERT_TRUE(bit_vector_get_at(&true_bits, i));
  }

  // Check that setting and getting works as expected.
  for (size_t i = 0; i < size; i++) {
    bit_vector_set_at(&false_bits, i, (i % 7) == 3);
    bit_vector_set_at(&true_bits, i, (i % 5) != 1);
  }
  for (size_t i = 0; i < size; i++) {
    ASSERT_EQ((i % 7) == 3, bit_vector_get_at(&false_bits, i));
    ASSERT_EQ((i % 5) != 1, bit_vector_get_at(&true_bits, i));
  }

  bit_vector_dispose(&false_bits);
  bit_vector_dispose(&true_bits);

}

TEST(utils, bit_vectors) {
  test_bit_vector(8);
  test_bit_vector(62);
  test_bit_vector(64);
  test_bit_vector(66);
  test_bit_vector(1022);
  test_bit_vector(1024);
  test_bit_vector(1026);
}

TEST(utils, pseudo_random) {
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

TEST(utils, shuffle) {
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

// Enter values to the given depth.
static void test_deep_entering(cycle_detector_t *outer, size_t depth) {
  if (depth == 0)
    return;
  cycle_detector_t inner;
  ASSERT_SUCCESS(cycle_detector_enter(outer, &inner, new_integer(depth)));
  test_deep_entering(&inner, depth - 1);
}

// Enter values to the given depth and return whether a cycle was ever detected.
static bool test_eventual_detection(cycle_detector_t *outer, size_t depth) {
  if (depth == 0)
    return false;
  cycle_detector_t inner;
  value_t entered = cycle_detector_enter(outer, &inner, new_integer(depth % 17));
  if (get_value_domain(entered) == vdSignal)
    return true;
  return test_eventual_detection(&inner, depth - 1);
}

TEST(utils, cycle_detector) {
  CREATE_RUNTIME();

  {
    cycle_detector_t outer;
    cycle_detector_init_bottom(&outer);
    test_deep_entering(&outer, 1024);
  }

  {
    cycle_detector_t outer;
    cycle_detector_init_bottom(&outer);
    ASSERT_TRUE(test_eventual_detection(&outer, 1024));
  }

  DISPOSE_RUNTIME();
}
