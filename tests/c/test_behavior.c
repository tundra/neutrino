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
  unary_map_t *hash = value_transient_identity_hash;
  binary_predicate_t *equal = value_are_identical;

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
