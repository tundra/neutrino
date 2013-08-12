// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "method.h"
#include "value-inl.h"


// --- G u a r d ---

OBJECT_IDENTITY_IMPL(guard);
CANT_SET_CONTENTS(guard);
FIXED_SIZE_PURE_VALUE_IMPL(guard, Guard);

ENUM_ACCESSORS_IMPL(Guard, guard, guard_type_t, Type, type);
UNCHECKED_ACCESSORS_IMPL(Guard, guard, Value, value);

value_t guard_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofGuard, value);
  return success();
}

void guard_print_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofGuard, value);
  string_buffer_printf(buf, "#<guard>");
}

void guard_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofGuard, value);
  string_buffer_printf(buf, "#<guard>");
}

bool is_score_match(score_t score) {
  return score != gsNoMatch;
}

score_t guard_match(value_t guard, value_t value) {
  CHECK_FAMILY(ofGuard, guard);
  switch (get_guard_type(guard)) {
    case gtId: {
      value_t guard_value = get_guard_value(guard);
      bool match = value_are_identical(guard_value, value);
      return match ? gsIdenticalMatch : gsNoMatch;
    }
    case gtAny:
      return gsAnyMatch;
    default:
      return gsNoMatch;
  }
}
