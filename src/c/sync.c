//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#include "builtin.h"
#include "sync.h"
#include "tagged-inl.h"
#include "value-inl.h"

/// ## Promise

GET_FAMILY_PRIMARY_TYPE_IMPL(promise);
FIXED_GET_MODE_IMPL(promise, vmMutable);

ACCESSORS_IMPL(Promise, promise, acInPhylum, tpPromiseState, State, state);
ACCESSORS_IMPL(Promise, promise, acNoCheck, 0, Value, value);

bool is_promise_resolved(value_t self) {
  CHECK_FAMILY(ofPromise, self);
  return is_promise_state_resolved(get_promise_state(self));
}

value_t promise_validate(value_t self) {
  VALIDATE_FAMILY(ofPromise, self);
  VALIDATE_PHYLUM(tpPromiseState, get_promise_state(self));
  return success();
}

void promise_print_on(value_t self, print_on_context_t *context) {
  value_t state_value = get_promise_state(self);
  promise_state_t state = get_promise_state_value(state_value);
  if (state == psPending) {
    string_buffer_printf(context->buf, "#<pending promise ~%w>", self);
  } else {
    string_buffer_printf(context->buf, "#<%v promise ", state_value);
    value_print_inner_on(get_promise_value(self), context, -1);
    string_buffer_printf(context->buf, ">");
  }
}

static value_t promise_state(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  return get_promise_state(self);
}

static value_t promise_is_resolved(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  return new_boolean(is_promise_resolved(self));
}

static value_t promise_get(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  if (is_promise_resolved(self)) {
    return get_promise_value(self);
  } else {
    ESCAPE_BUILTIN(args, promise_not_resolved, self);
  }
}

static value_t promise_fulfill(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  value_t value = get_builtin_argument(args, 0);
  if (!is_promise_resolved(self)) {
    set_promise_state(self, promise_state_fulfilled());
    set_promise_value(self, value);
  }
  return value;
}

value_t add_promise_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("promise.state", 0, promise_state);
  ADD_BUILTIN_IMPL("promise.is_resolved?", 0, promise_is_resolved);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("promise.get", 0, 1, promise_get);
  ADD_BUILTIN_IMPL("promise.fulfill!", 1, promise_fulfill);
  return success();
}


/// ## Promise state

void promise_state_print_on(value_t value, print_on_context_t *context) {
  const char *str = NULL;
  switch (get_promise_state_value(value)) {
    case psPending:
      str = "pending";
      break;
    case psFulfilled:
      str = "fulfilled";
      break;
    case psRejected:
      str = "rejected";
      break;
  }
  string_buffer_printf(context->buf, "#<promise state %s>", str);
}
