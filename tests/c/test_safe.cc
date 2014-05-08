//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "derived.h"
#include "runtime.h"
#include "safe-inl.h"
#include "try-inl.h"
END_C_INCLUDES

NEW_TEST(safe, simple_safe_value) {
  CREATE_RUNTIME();

  value_t array_before = new_heap_array(runtime, 2);
  set_array_at(array_before, 0, yes());
  set_array_at(array_before, 1, no());
  safe_value_t s_array = runtime_protect_value(runtime, array_before);
  ASSERT_SAME(array_before, deref(s_array));
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t array_after = deref(s_array);
  ASSERT_NSAME(array_before, array_after);
  ASSERT_VALEQ(yes(), get_array_at(array_after, 0));
  ASSERT_VALEQ(no(), get_array_at(array_after, 1));

  dispose_safe_value(runtime, s_array);

  DISPOSE_RUNTIME();
}

NEW_TEST(safe, simple_safe_conditions) {
  CREATE_RUNTIME();

  safe_value_t s_cond = runtime_protect_value(runtime, new_heap_exhausted_condition(43));
  ASSERT_TRUE(safe_value_is_immediate(s_cond));
  ASSERT_EQ(43, get_condition_details(deref(s_cond)));
  safe_value_t s_int = runtime_protect_value(runtime, new_integer(8));
  ASSERT_TRUE(safe_value_is_immediate(s_int));
  ASSERT_EQ(8, get_integer_value(deref(s_int)));
  value_t obj = new_heap_array(runtime, 3);
  safe_value_t s_obj = runtime_protect_value(runtime, obj);
  ASSERT_FALSE(safe_value_is_immediate(s_obj));

  dispose_safe_value(runtime, s_cond);
  dispose_safe_value(runtime, s_int);
  dispose_safe_value(runtime, s_obj);

  DISPOSE_RUNTIME();
}

static safe_value_t simple_try_helper(runtime_t *runtime, value_t value,
    bool *succeeded) {
  S_TRY(runtime_protect_value(runtime, value));
  *succeeded = true;
  return protect_immediate(success());
}

NEW_TEST(safe, simple_try) {
  CREATE_RUNTIME();

  bool succeeded = false;
  simple_try_helper(runtime, new_condition(ccNotFound), &succeeded);
  ASSERT_FALSE(succeeded);
  simple_try_helper(runtime, new_integer(8), &succeeded);
  ASSERT_TRUE(succeeded);

  DISPOSE_RUNTIME();
}

static safe_value_t simple_try_set_helper(runtime_t *runtime, value_t value,
    bool *succeeded) {
  safe_value_t target;
  S_TRY_SET(target, runtime_protect_value(runtime, value));
  *succeeded = true;
  dispose_safe_value(runtime, target);
  return protect_immediate(success());
}

NEW_TEST(safe, simple_try_set) {
  CREATE_RUNTIME();

  bool succeeded = false;
  simple_try_set_helper(runtime, new_condition(ccNotFound), &succeeded);
  ASSERT_FALSE(succeeded);
  simple_try_set_helper(runtime, new_integer(8), &succeeded);
  ASSERT_TRUE(succeeded);
  succeeded = false;
  value_t arr = new_heap_array(runtime, 3);
  simple_try_set_helper(runtime, arr, &succeeded);
  ASSERT_TRUE(succeeded);

  DISPOSE_RUNTIME();
}

NEW_TEST(safe, simple_pool) {
  CREATE_RUNTIME();

  CREATE_SAFE_VALUE_POOL(runtime, 3, pool);
  protect(pool, new_heap_array(runtime, 4));
  protect(pool, new_heap_array(runtime, 5));
  protect(pool, new_heap_array(runtime, 6));
  DISPOSE_SAFE_VALUE_POOL(pool);

  DISPOSE_RUNTIME();
}

NEW_TEST(safe, pool_overflow) {
  CREATE_RUNTIME();

  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);
  protect(pool, new_heap_array(runtime, 4));
  ASSERT_CHECK_FAILURE_NO_VALUE(ccSafePoolFull, protect(pool, new_heap_array(runtime, 4)));
  DISPOSE_SAFE_VALUE_POOL(pool);

  DISPOSE_RUNTIME();
}

NEW_TEST(safe, derived) {
  CREATE_RUNTIME();

  // Create some derived objects.
  value_t before_array = new_heap_array(runtime, 2);
  value_t p0 = new_derived_stack_pointer(runtime, alloc_array_block(before_array, 0, 1),
      before_array);
  ASSERT_GENUS(dgStackPointer, p0);
  value_t p1 = new_derived_stack_pointer(runtime, alloc_array_block(before_array, 1, 1),
      before_array);
  ASSERT_GENUS(dgStackPointer, p1);

  // GC protect them.
  safe_value_t s_p0 = runtime_protect_value(runtime, p0);
  ASSERT_GENUS(dgStackPointer, deref(s_p0));
  ASSERT_FALSE(safe_value_is_immediate(s_p0));
  safe_value_t s_p1 = runtime_protect_value(runtime, p1);
  ASSERT_GENUS(dgStackPointer, deref(s_p1));
  ASSERT_FALSE(safe_value_is_immediate(s_p1));

  // Run gc.
  runtime_garbage_collect(runtime);

  // They should have moved during gc.
  ASSERT_FALSE(is_same_value(p0, deref(s_p0)));
  ASSERT_GENUS(dgStackPointer, deref(s_p0));
  ASSERT_FALSE(is_same_value(p1, deref(s_p1)));
  ASSERT_GENUS(dgStackPointer, deref(s_p1));

  value_t after_array = get_derived_object_host(deref(s_p0));
  ASSERT_FALSE(is_same_value(after_array, before_array));
  ASSERT_EQ(2, get_array_length(after_array));
  ASSERT_TRUE(is_same_value(after_array, get_derived_object_host(deref(s_p1))));

  dispose_safe_value(runtime, s_p0);
  dispose_safe_value(runtime, s_p1);

  DISPOSE_RUNTIME();
}
