// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "safe.h"


bool safe_value_is_immediate(safe_value_t s_value) {
  return get_value_domain(s_value.as_value) != vdObject;
}

safe_value_t object_tracker_to_safe_value(object_tracker_t *handle) {
  value_t as_object = new_object((address_t) handle);
  safe_value_t s_result = {.as_value=as_object};
  CHECK_FALSE("cast into signal", safe_value_is_immediate(s_result));
  return s_result;
}

object_tracker_t *safe_value_to_object_tracker(safe_value_t s_value) {
  CHECK_FALSE("using immediate as indirect", safe_value_is_immediate(s_value));
  return (object_tracker_t*) get_object_address(s_value.as_value);
}

safe_value_t protect_immediate(value_t value) {
  CHECK_FALSE("value not immediate", get_value_domain(value) == vdObject);
  safe_value_t s_result = {.as_value=value};
  CHECK_TRUE("cast out of signal", safe_value_is_immediate(s_result));
  return s_result;
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
