// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "condition.h"
#include "runtime.h"
#include "safe-inl.h"
#include "try-inl.h"


bool safe_value_is_immediate(safe_value_t s_value) {
  return !is_heap_object(s_value.as_value);
}

safe_value_t object_tracker_to_safe_value(object_tracker_t *handle) {
  value_t as_object = new_heap_object((address_t) handle);
  safe_value_t s_result;
  s_result.as_value = as_object;;
  CHECK_FALSE("cast into condition", safe_value_is_immediate(s_result));
  return s_result;
}

object_tracker_t *safe_value_to_object_tracker(safe_value_t s_value) {
  CHECK_FALSE("using immediate as indirect", safe_value_is_immediate(s_value));
  return (object_tracker_t*) get_heap_object_address(s_value.as_value);
}

safe_value_t protect_immediate(value_t value) {
  CHECK_FALSE("value not immediate", get_value_domain(value) == vdHeapObject);
  safe_value_t s_result;
  s_result.as_value = value;
  CHECK_TRUE("cast out of condition", safe_value_is_immediate(s_result));
  return s_result;
}

safe_value_t empty_safe_value() {
  return protect_immediate(new_integer(0));
}

value_t deref_immediate(safe_value_t s_value) {
  CHECK_TRUE("using indirect as immediate", safe_value_is_immediate(s_value));
  return s_value.as_value;
}

value_t deref(safe_value_t s_value) {
  return safe_value_is_immediate(s_value)
      ? deref_immediate(s_value)
      : safe_value_to_object_tracker(s_value)->value;
}

void safe_value_pool_init(safe_value_pool_t *pool, safe_value_t *values,
    size_t capacity, runtime_t *runtime) {
  pool->values = values;
  pool->capacity = capacity;
  pool->used = 0;
  pool->runtime = runtime;
}

void safe_value_pool_dispose(safe_value_pool_t *pool) {
  for (size_t i = 0; i < pool->used; i++)
    dispose_safe_value(pool->runtime, pool->values[i]);
}

safe_value_t protect(safe_value_pool_t *pool, value_t value) {
  COND_CHECK_TRUE_WITH_VALUE("safe value pool overflow", ccSafePoolFull,
      protect_immediate(new_condition(ccSafePoolFull)),
      pool->used < pool->capacity);
  S_TRY_DEF(result, runtime_protect_value(pool->runtime, value));
  pool->values[pool->used++] = result;
  return result;
}
