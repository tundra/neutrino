// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "safe-inl.h"
#include "test.h"
#include "try-inl.h"

TEST(safe, simple_safe_value) {
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

TEST(safe, simple_safe_signals) {
  CREATE_RUNTIME();

  safe_value_t s_sig = runtime_protect_value(runtime, new_heap_exhausted_signal(43));
  ASSERT_TRUE(safe_value_is_immediate(s_sig));
  ASSERT_EQ(43, get_signal_details(deref(s_sig)));
  safe_value_t s_int = runtime_protect_value(runtime, new_integer(8));
  ASSERT_TRUE(safe_value_is_immediate(s_int));
  ASSERT_EQ(8, get_integer_value(deref(s_int)));
  value_t obj = new_heap_array(runtime, 3);
  safe_value_t s_obj = runtime_protect_value(runtime, obj);
  ASSERT_FALSE(safe_value_is_immediate(s_obj));

  dispose_safe_value(runtime, s_sig);
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

TEST(safe, simple_try) {
  CREATE_RUNTIME();

  bool succeeded = false;
  simple_try_helper(runtime, new_signal(scNotFound), &succeeded);
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

TEST(safe, simple_try_set) {
  CREATE_RUNTIME();

  bool succeeded = false;
  simple_try_set_helper(runtime, new_signal(scNotFound), &succeeded);
  ASSERT_FALSE(succeeded);
  simple_try_set_helper(runtime, new_integer(8), &succeeded);
  ASSERT_TRUE(succeeded);
  succeeded = false;
  value_t arr = new_heap_array(runtime, 3);
  simple_try_set_helper(runtime, arr, &succeeded);
  ASSERT_TRUE(succeeded);

  DISPOSE_RUNTIME();
}

TEST(safe, simple_pool) {
  CREATE_RUNTIME();

  CREATE_SAFE_VALUE_POOL(runtime, 3, pool);
  protect(pool, new_heap_array(runtime, 4));
  protect(pool, new_heap_array(runtime, 5));
  protect(pool, new_heap_array(runtime, 6));
  DISPOSE_SAFE_VALUE_POOL(pool);

  DISPOSE_RUNTIME();
}

TEST(safe, pool_overflow) {
  CREATE_RUNTIME();

  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);
  protect(pool, new_heap_array(runtime, 4));
  ASSERT_CHECK_FAILURE_NO_VALUE(scSafePoolFull, protect(pool, new_heap_array(runtime, 4)));
  DISPOSE_SAFE_VALUE_POOL(pool);

  DISPOSE_RUNTIME();
}
