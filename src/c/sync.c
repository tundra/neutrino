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


/// ## Foreign service

FIXED_GET_MODE_IMPL(foreign_service, vmDeepFrozen);
GET_FAMILY_PRIMARY_TYPE_IMPL(foreign_service);

FROZEN_ACCESSORS_IMPL(ForeignService, foreign_service, acInFamily, ofIdHashMap, Impls,
    impls);
FROZEN_ACCESSORS_IMPL(ForeignService, foreign_service, acNoCheck, 0, DisplayName,
    display_name);

value_t foreign_service_validate(value_t self) {
  VALIDATE_FAMILY(ofForeignService, self);
  VALIDATE_FAMILY(ofIdHashMap, get_foreign_service_impls(self));
  return success();
}

void foreign_service_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<foreign_service: ");
  print_on_context_t sub_context = *context;
  sub_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
  value_print_inner_on(get_foreign_service_display_name(self), &sub_context, -1);
  string_buffer_printf(context->buf, ">");
}

// Called when a native request succeeds. Note that there is no guarantee of
// which thread will call this.
static opaque_t on_foreign_request_success(opaque_t opaque_state,
    opaque_t opaque_result) {
  foreign_request_state_t *state = (foreign_request_state_t*) o2p(opaque_state);
  state->result = *((pton_variant_t*) o2p(opaque_result));
  process_airlock_complete_foreign_request(state->airlock, state);
  return opaque_null();
}

void foreign_request_state_init(foreign_request_state_t *state,
    unary_callback_t *callback, process_airlock_t *airlock,
    value_t surface_promise, pton_variant_t result) {
  state->callback = callback;
  state->airlock = airlock;
  state->surface_promise = surface_promise;
  state->result = result;
}

value_t foreign_request_state_new(runtime_t *runtime, value_t process,
    foreign_request_state_t **result_out) {
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  memory_block_t memory = allocator_default_malloc(sizeof(foreign_request_state_t));
  if (memory_block_is_empty(memory))
    return new_system_call_failed_condition("malloc");
  foreign_request_state_t *state = (foreign_request_state_t*) memory.memory;
  unary_callback_t *callback = unary_callback_new_1(on_foreign_request_success, p2o(state));
  foreign_request_state_init(state, callback, get_process_airlock(process),
      promise, pton_null());
  native_request_t *request = &state->request;
  opaque_promise_t *impl_promise = opaque_promise_empty();
  pton_arena_t *arena = pton_new_arena();
  native_request_init(request, runtime, impl_promise, arena, pton_null());
  opaque_promise_on_success(impl_promise, state->callback);
  *result_out = state;
  return success();
}

void foreign_request_state_destroy(foreign_request_state_t *state) {
  callback_destroy(state->callback);
  opaque_promise_destroy(state->request.impl_promise);
  pton_dispose_arena(state->request.arena);
  allocator_default_free(new_memory_block(state, sizeof(foreign_request_state_t)));
}

value_t incoming_request_state_new(value_t surface_promise,
    incoming_request_state_t **result_out) {
  memory_block_t memory = allocator_default_malloc(sizeof(incoming_request_state_t));
  if (memory_block_is_empty(memory))
    return new_system_call_failed_condition("malloc");
  incoming_request_state_t *state = (incoming_request_state_t*) memory.memory;
  state->surface_promise = surface_promise;
  *result_out = state;
  return success();
}

void incoming_request_state_destroy(incoming_request_state_t *state) {
  allocator_default_free(new_memory_block(state, sizeof(incoming_request_state_t)));
}

// Create a plankton-ified copy of the raw arguments.
static value_t foreign_service_clone_args(pton_arena_t *arena, value_t raw_args,
    pton_variant_t *args_out) {
  CHECK_FAMILY(ofReifiedArguments, raw_args);
  pton_variant_t args = pton_new_map(arena);
  value_t tags = get_reified_arguments_tags(raw_args);
  value_t values = get_reified_arguments_values(raw_args);
  for (size_t i = 0; i < get_call_tags_entry_count(tags); i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    if (get_value_domain(tag) == vdHeapObject && get_heap_object_family(tag) == ofKey)
      // Skip the keys for now. Figure out how to deal with them later.
      continue;
    size_t offset = get_call_tags_offset_at(tags, i);
    value_t arg = get_array_at(values, offset);
    pton_variant_t key;
    TRY(value_to_plankton(arena, tag, &key));
    pton_variant_t value;
    TRY(value_to_plankton(arena, arg, &value));
    pton_map_set(args, key, value);
  }
  *args_out = args;
  return success();
}

static value_t foreign_service_call_with_args(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofForeignService, self);
  value_t operation = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofOperation, operation);
  value_t reified = get_builtin_argument(args, 1);
  CHECK_FAMILY(ofReifiedArguments, reified);
  // First look up the implementation since this may fail in which case it's
  // convenient to be able to just break out without having to clean up.
  value_t impls = get_foreign_service_impls(self);
  value_t name = get_operation_value(operation);
  value_t method = get_id_hash_map_at(impls, name);
  if (in_condition_cause(ccNotFound, method))
    // Escape with the operation not the name; the part about extracting the
    // string name is an implementation detail.
    ESCAPE_BUILTIN(args, unknown_foreign_method, operation);
  void *impl_ptr = get_void_p_value(method);
  unary_callback_t *impl = (unary_callback_t*) impl_ptr;
  // Got an implementation. Now we can start allocating stuff.
  runtime_t *runtime = get_builtin_runtime(args);
  foreign_request_state_t *state = NULL;
  value_t process = get_builtin_process(args);
  TRY(foreign_request_state_new(runtime, process, &state));
  TRY(foreign_service_clone_args(state->request.arena, reified, &state->request.args));
  get_process_airlock(process)->open_foreign_request_count++;
  unary_callback_call(impl, p2o(&state->request));
  return state->surface_promise;
}

value_t add_foreign_service_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("foreign_service.call_with_args", 2, 1,
      foreign_service_call_with_args);
  return success();
}


/// ## Exported service

FIXED_GET_MODE_IMPL(exported_service, vmMutable);
GET_FAMILY_PRIMARY_TYPE_IMPL(exported_service);
TRIVIAL_PRINT_ON_IMPL(ExportedService, exported_service);

ACCESSORS_IMPL(ExportedService, exported_service, acInFamily, ofVoidP,
    CapsulePtr, capsule_ptr);
ACCESSORS_IMPL(ExportedService, exported_service, acInFamily, ofProcess,
    Process, process);
ACCESSORS_IMPL(ExportedService, exported_service, acNoCheck, 0, Handler,
    handler);

value_t exported_service_validate(value_t self) {
  VALIDATE_FAMILY(ofExportedService, self);
  VALIDATE_FAMILY(ofVoidP, get_exported_service_capsule_ptr(self));
  VALIDATE_FAMILY(ofProcess, get_exported_service_process(self));
  return success();
}

static value_t exported_service_call_with_args(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofExportedService, self);
  value_t reified = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofReifiedArguments, reified);
  runtime_t *runtime = get_builtin_runtime(args);
  incoming_request_state_t *state = NULL;
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  TRY(incoming_request_state_new(promise, &state));
  value_t process = get_exported_service_process(self);
  process_airlock_t *airlock = get_process_airlock(process);
  process_airlock_schedule_incoming_request(airlock, state);
  return promise;
}

value_t add_exported_service_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("exported_service.call_with_args", 1, 1,
      exported_service_call_with_args);
  return success();
}
