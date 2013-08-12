// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"

TEST(method, identity_guard) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t zero = new_integer(0);
  value_t null = runtime_null(&runtime);
  value_t id_zero = new_heap_guard(&runtime, gtId, zero);
  value_t id_null = new_heap_guard(&runtime, gtId, null);

  ASSERT_TRUE(is_score_match(guard_match(id_zero, zero)));
  ASSERT_FALSE(is_score_match(guard_match(id_zero, null)));
  ASSERT_FALSE(is_score_match(guard_match(id_null, zero)));
  ASSERT_TRUE(is_score_match(guard_match(id_null, null)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, any_guard) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t any_guard = runtime.roots.any_guard;

  ASSERT_TRUE(is_score_match(guard_match(any_guard, new_integer(0))));
  ASSERT_TRUE(is_score_match(guard_match(any_guard, new_integer(1))));
  ASSERT_TRUE(is_score_match(guard_match(any_guard, runtime_null(&runtime))));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, method_space) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);


  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
