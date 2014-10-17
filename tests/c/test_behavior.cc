//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "runtime.h"
#include "value-inl.h"
END_C_INCLUDES

TEST(behavior, string_validation) {
  CREATE_RUNTIME();

  utf8_t chars = new_c_string("Hut!");
  value_t str = new_heap_utf8(runtime, chars);

  // String starts out validating.
  ASSERT_SUCCESS(heap_object_validate(str));
  // Zap the null terminator.
  get_utf8_chars(str)[4] = 'x';
  // Now the string no longer terminates.
  ASSERT_CHECK_FAILURE(ccValidationFailed, heap_object_validate(str));
  get_utf8_chars(str)[4] = '\0';

  DISPOSE_RUNTIME();
}

TEST(behavior, identity) {
  CREATE_RUNTIME();

  // Convenient shorthands.
  value_t (*hash)(value_t value) = value_transient_identity_hash;
  bool (*equal)(value_t a, value_t b) = value_identity_compare;

  // Integers
  ASSERT_SUCCESS(hash(new_integer(0)));
  ASSERT_SAME(hash(new_integer(0)), hash(new_integer(0)));
  ASSERT_TRUE(equal(new_integer(0), new_integer(0)));
  ASSERT_FALSE(hash(new_integer(1)).encoded == hash(new_integer(0)).encoded);
  ASSERT_FALSE(equal(new_integer(1), new_integer(0)));

  // Strings
  value_t foo = new_heap_utf8(runtime, new_c_string("foo"));
  value_t bar = new_heap_utf8(runtime, new_c_string("bar"));
  ASSERT_SUCCESS(hash(foo));
  ASSERT_NSAME(hash(foo), hash(bar));
  ASSERT_TRUE(equal(foo, foo));
  ASSERT_FALSE(equal(foo, bar));

  // Bools
  value_t thrue = yes();
  value_t fahlse = no();
  ASSERT_SUCCESS(hash(thrue));
  ASSERT_NSAME(hash(thrue), hash(fahlse));
  ASSERT_TRUE(equal(thrue, thrue));
  ASSERT_FALSE(equal(thrue, fahlse));
  ASSERT_TRUE(equal(fahlse, fahlse));

  // Null
  ASSERT_SUCCESS(hash(null()));
  ASSERT_TRUE(equal(null(), null()));

  DISPOSE_RUNTIME();
}

// Check that printing the given value yields the expected string.
static void check_print_on(const char *expected_chars, value_t value) {
  string_buffer_t buf;
  string_buffer_init(&buf);

  value_print_default_on(value, &buf);
  utf8_t result = string_buffer_flush(&buf);
  ASSERT_STREQ(new_c_string(expected_chars), result);

  string_buffer_dispose(&buf);
}

TEST(behavior, print_on) {
  CREATE_RUNTIME();

  // Integers
  check_print_on("0", new_integer(0));
  check_print_on("413", new_integer(413));
  check_print_on("-1231", new_integer(-1231));

  // Singletons
  check_print_on("null", null());
  check_print_on("true", yes());
  check_print_on("false", no());

  // Strings
  value_t foo = new_heap_utf8(runtime, new_c_string("foo"));
  check_print_on("\"foo\"", foo);
  value_t empty = new_heap_utf8(runtime, new_c_string(""));
  check_print_on("\"\"", empty);

  // Arrays.
  value_t arr = new_heap_array(runtime, 3);
  check_print_on("[null, null, null]", arr);
  set_array_at(arr, 1, new_integer(4));
  check_print_on("[null, 4, null]", arr);
  set_array_at(arr, 2, foo);
  check_print_on("[null, 4, \"foo\"]", arr);
  set_array_at(arr, 0, arr);
  check_print_on("[[#<array[3]>, 4, \"foo\"], 4, \"foo\"]", arr);

  // Maps
  value_t map = new_heap_id_hash_map(runtime, 16);
  set_array_at(arr, 0, map);
  check_print_on("{}", map);
  check_print_on("[{}, 4, \"foo\"]", arr);
  ASSERT_SUCCESS(try_set_id_hash_map_at(map, new_integer(3), new_integer(5), false));
  check_print_on("{3: 5}", map);
  check_print_on("[{3: 5}, 4, \"foo\"]", arr);

  // Blobs
  value_t blob = new_heap_blob(runtime, 9);
  set_array_at(arr, 0, blob);
  check_print_on("[#<blob: [0000000000000000...]>, 4, \"foo\"]", arr);

  DISPOSE_RUNTIME();
}

static value_t dummy_constructor(runtime_t *runtime) {
  return new_integer(434);
}

static value_t condition_constructor(runtime_t *runtime) {
  return new_condition(ccNothing);
}

TEST(behavior, new_instance) {
  CREATE_RUNTIME();

  value_t dummy_fact = new_heap_factory(runtime, dummy_constructor);
  ASSERT_SUCCESS(dummy_fact);
  value_t instance = new_heap_object_with_type(runtime, dummy_fact);
  ASSERT_VALEQ(new_integer(434), instance);

  value_t condition_fact = new_heap_factory(runtime, condition_constructor);
  ASSERT_SUCCESS(condition_fact);
  value_t cond = new_heap_object_with_type(runtime, condition_fact);
  ASSERT_CONDITION(ccNothing, cond);

  DISPOSE_RUNTIME();
}
