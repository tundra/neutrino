#include "alloc.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"

TEST(behavior, string_validation) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  DEF_STRING(chars, "Hut!");
  value_t str = new_heap_string(&runtime, &chars);

  // String starts out validating.
  ASSERT_FALSE(in_domain(vdSignal, object_validate(str)));
  // Zap the null terminator.
  get_string_chars(str)[4] = 'x';
  // Now the string no longer terminates.
  ASSERT_SIGNAL(scValidationFailed, object_validate(str));

  runtime_dispose(&runtime);
}

TEST(behavior, identity) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Convenient shorthands.
  value_t (*hash)(value_t value) = value_transient_identity_hash;
  bool (*equal)(value_t a, value_t b) = value_are_identical;

  // Integers
  ASSERT_SUCCESS(hash(new_integer(0)));
  ASSERT_EQ(hash(new_integer(0)), hash(new_integer(0)));
  ASSERT_TRUE(equal(new_integer(0), new_integer(0)));
  ASSERT_FALSE(hash(new_integer(1)) == hash(new_integer(0)));
  ASSERT_FALSE(equal(new_integer(1), new_integer(0)));

  // Strings
  DEF_STRING(foo_chars, "foo");
  value_t foo = new_heap_string(&runtime, &foo_chars);
  DEF_STRING(bar_chars, "bar");
  value_t bar = new_heap_string(&runtime, &bar_chars);
  ASSERT_SUCCESS(hash(foo));
  ASSERT_FALSE(hash(foo) == hash(bar));
  ASSERT_TRUE(equal(foo, foo));
  ASSERT_FALSE(equal(foo, bar));

  // Bools
  value_t thrue = runtime_bool(&runtime, true);
  value_t fahlse = runtime_bool(&runtime, false);
  ASSERT_SUCCESS(hash(thrue));
  ASSERT_FALSE(hash(thrue) == hash(fahlse));
  ASSERT_TRUE(equal(thrue, thrue));
  ASSERT_FALSE(equal(thrue, fahlse));
  ASSERT_TRUE(equal(fahlse, fahlse));

  // Null
  value_t null = runtime_null(&runtime);
  ASSERT_SUCCESS(hash(null));
  ASSERT_TRUE(equal(null, null));

  runtime_dispose(&runtime);
}

// Check that printing the given value yields the expected string.
static void check_print_on(const char *expected_chars, value_t value) {
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);

  value_print_on(value, &buf);
  string_t result;
  string_buffer_flush(&buf, &result);
  DEF_STRING(expected, expected_chars);
  ASSERT_STREQ(&expected, &result);

  string_buffer_dispose(&buf);
}

TEST(behavior, print_on) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Integers
  check_print_on("0", new_integer(0));
  check_print_on("413", new_integer(413));
  check_print_on("-1231", new_integer(-1231));

  // Singletons
  check_print_on("null", runtime_null(&runtime));
  check_print_on("true", runtime_bool(&runtime, true));
  check_print_on("false", runtime_bool(&runtime, false));

  // Strings
  DEF_STRING(foo_chars, "foo");
  value_t foo = new_heap_string(&runtime, &foo_chars);
  check_print_on("\"foo\"", foo);
  DEF_STRING(empty_chars, "");
  value_t empty = new_heap_string(&runtime, &empty_chars);
  check_print_on("\"\"", empty);

  // Arrays.
  value_t arr = new_heap_array(&runtime, 3);
  check_print_on("[null, null, null]", arr);
  set_array_at(arr, 1, new_integer(4));
  check_print_on("[null, 4, null]", arr);  
  set_array_at(arr, 2, foo);
  check_print_on("[null, 4, \"foo\"]", arr);  
  set_array_at(arr, 0, arr);
  check_print_on("[#<array[3]>, 4, \"foo\"]", arr);

  // Maps
  value_t map = new_heap_map(&runtime, 16);
  set_array_at(arr, 0, map);
  check_print_on("[#<map{0}>, 4, \"foo\"]", arr);

  runtime_dispose(&runtime);
}
