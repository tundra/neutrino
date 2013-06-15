#include "test.h"
#include "utils.h"

TEST(utils, string_simple) {
  string_t str;
  string_init(&str, "Hello, World!");
  ASSERT_EQ(13, string_length(&str));
  ASSERT_EQ('H', string_char_at(&str, 0));
  ASSERT_EQ('!', string_char_at(&str, 12));
}

TEST(utils, string_equality) {
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

  ASSERT_TRUE(string_equals(&s0, &s0));
  ASSERT_TRUE(string_equals(&s0, &s1));
  ASSERT_FALSE(string_equals(&s0, &s2));
  ASSERT_FALSE(string_equals(&s0, &s3));
  ASSERT_TRUE(string_equals(&s1, &s0));
  ASSERT_TRUE(string_equals(&s1, &s1));
  ASSERT_FALSE(string_equals(&s1, &s2));
  ASSERT_FALSE(string_equals(&s1, &s3));
  ASSERT_FALSE(string_equals(&s2, &s0));
  ASSERT_FALSE(string_equals(&s2, &s1));
  ASSERT_TRUE(string_equals(&s2, &s2));
  ASSERT_FALSE(string_equals(&s2, &s3));
  ASSERT_FALSE(string_equals(&s3, &s0));
  ASSERT_FALSE(string_equals(&s3, &s1));
  ASSERT_FALSE(string_equals(&s3, &s2));
  ASSERT_TRUE(string_equals(&s3, &s3));
}
