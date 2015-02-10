//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#include "alloc.h"
#include "builtin.h"
#include "freeze.h"
#include "plugin.h"
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

void fulfill_promise(value_t self, value_t value) {
  if (!is_promise_resolved(self)) {
    set_promise_state(self, promise_state_fulfilled());
    set_promise_value(self, value);
  }
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
  fulfill_promise(self, value);
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


/// ## Native remote

FIXED_GET_MODE_IMPL(native_remote, vmDeepFrozen);
GET_FAMILY_PRIMARY_TYPE_IMPL(native_remote);

FROZEN_ACCESSORS_IMPL(NativeRemote, native_remote, acInFamily, ofVoidP, Impl,
    impl);
FROZEN_ACCESSORS_IMPL(NativeRemote, native_remote, acNoCheck, 0, DisplayName,
    display_name);

value_t native_remote_validate(value_t self) {
  VALIDATE_FAMILY(ofNativeRemote, self);
  VALIDATE_FAMILY(ofVoidP, get_native_remote_impl(self));
  return success();
}

void native_remote_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<native_remote: ");
  print_on_context_t sub_context = *context;
  sub_context.flags |= pfUnquote;
  value_print_inner_on(get_native_remote_display_name(self), &sub_context, -1);
  string_buffer_printf(context->buf, ">");
}

// Extra state maintained around a native request.
typedef struct {
  native_request_t request;
  unary_callback_t *callback;
  value_t promise;
} native_request_state_t;

// Called when a native request succeeds. Note that there is no guaranteed of
// which thread will call this so any access to the runtime needs to be
// explicitly synchronized.
static opaque_t on_native_request_success(opaque_t opaque_state,
    opaque_t opaque_result) {
  // Start out by cleaning up the state.
  native_request_state_t *state = (native_request_state_t*) o2p(opaque_state);
  value_t promise = state->promise;
  callback_dispose(state->callback);
  opaque_promise_dispose(state->request.promise);
  allocator_default_free(new_memory_block(state, sizeof(native_request_state_t)));
  // TODO: the promise must not be fulfilled in the middle of a turn, it should
  //   be scheduled now but only done after the end of the turn.
  value_t result = o2v(opaque_result);
  fulfill_promise(promise, result);
  return opaque_null();
}

static value_t native_remote_call_with_args(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofNativeRemote, self);
  value_t reified = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofReifiedArguments, reified);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  // Allocate and initialize the request state.
  memory_block_t memory = allocator_default_malloc(sizeof(native_request_state_t));
  native_request_state_t *state = (native_request_state_t*) memory.memory;
  state->promise = promise;
  native_request_t *request = &state->request;
  request->promise = opaque_promise_empty();
  // Ensure that we get notified when the remote responds.
  state->callback = new_unary_callback_1(on_native_request_success, p2o(state));
  opaque_promise_on_success(request->promise, state->callback);
  // Pass the request on to the remote.
  void *impl_ptr = get_void_p_value(get_native_remote_impl(self));
  native_remote_t *impl = (native_remote_t*) impl_ptr;
  impl->schedule_request(request);
  return promise;
}

value_t add_native_remote_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL("native_remote.call_with_args", 1, native_remote_call_with_args);
  return success();
}
