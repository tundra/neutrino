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

TEST(utils, string_buffer_simple) {
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);

  string_buffer_printf(&buf, "[%s: %i]", "test", 8);
  string_t str;
  string_buffer_flush(&buf, &str);
  DEF_STRING(expected, "[test: 8]");
  ASSERT_STREQ(&expected, &str);

  string_buffer_dispose(&buf);
}

TEST(utils, string_buffer_concat) {
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);

  string_buffer_printf(&buf, "foo");
  string_buffer_printf(&buf, "bar");
  string_buffer_printf(&buf, "baz");
  string_t str;
  string_buffer_flush(&buf, &str);
  DEF_STRING(expected, "foobarbaz");
  ASSERT_STREQ(&expected, &str);

  string_buffer_dispose(&buf);
}

TEST(utils, string_buffer_long) {
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);

  // Cons up a really long string.
  for (size_t i = 0; i < 1024; i++)
    string_buffer_printf(&buf, "0123456789");
  // Check that it's correct.
  string_t str;
  string_buffer_flush(&buf, &str);
  ASSERT_EQ(10240, string_length(&str));
  for (size_t i = 0; i < 10240; i++) {
    CHECK_EQ((char) ('0' + (i % 10)), string_char_at(&str, i));
  }

  string_buffer_dispose(&buf);
}
