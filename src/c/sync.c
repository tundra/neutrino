//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#include "alloc.h"
#include "builtin.h"
#include "freeze.h"
#include "io.h"
#include "plugin.h"
#include "sentry.h"
#include "sync.h"
#include "tagged-inl.h"
#include "value-inl.h"

/// ## Promise

GET_FAMILY_PRIMARY_TYPE_IMPL(promise);
FIXED_GET_MODE_IMPL(promise, vmMutable);

ACCESSORS_IMPL(Promise, promise, snInPhylum(tpPromiseState), State, state);
ACCESSORS_IMPL(Promise, promise, snNoCheck, Payload, payload);

bool is_promise_settled(value_t self) {
  CHECK_FAMILY(ofPromise, self);
  return is_promise_state_settled(get_promise_state(self));
}

bool is_promise_fulfilled(value_t self) {
  CHECK_FAMILY(ofPromise, self);
  return get_promise_state_value(get_promise_state(self)) == psFulfilled;
}

bool is_promise_rejected(value_t self) {
  CHECK_FAMILY(ofPromise, self);
  return get_promise_state_value(get_promise_state(self)) == psRejected;
}

value_t get_promise_value(value_t self) {
  CHECK_EQ("getting value of unfulfilled", psFulfilled,
      get_promise_state_value(get_promise_state(self)));
  return get_promise_payload(self);
}

value_t get_promise_error(value_t self) {
  CHECK_EQ("getting error of unrejected", psRejected,
      get_promise_state_value(get_promise_state(self)));
  value_t error = get_promise_payload(self);
  CHECK_FAMILY(ofReifiedArguments, error);
  return error;
}

void fulfill_promise(value_t self, value_t value) {
  if (!is_promise_settled(self)) {
    set_promise_state(self, promise_state_fulfilled());
    set_promise_payload(self, value);
  }
}

void reject_promise(value_t self, value_t error) {
  if (!is_promise_settled(self)) {
    set_promise_state(self, promise_state_rejected());
    set_promise_payload(self, error);
  }
}

value_t schedule_promise_fulfill_atomic(runtime_t *runtime, value_t self,
    value_t value, value_t process) {
  process_airlock_t *airlock = get_process_airlock(process);
  fulfill_promise_state_t *state = allocator_default_malloc_struct(fulfill_promise_state_t);
  if (state == NULL)
    return new_system_error_condition(seAllocationFailed);
  undertaking_init(UPCAST_UNDERTAKING(state), &kFulfillPromiseController);
  state->s_promise = runtime_protect_value(runtime, self);
  state->s_value = runtime_protect_value(runtime, value);
  process_airlock_begin_undertaking(airlock, UPCAST_UNDERTAKING(state));
  process_airlock_deliver_undertaking(airlock, UPCAST_UNDERTAKING(state));
  return success();
}

value_t fulfill_promise_undertaking_finish(fulfill_promise_state_t *state,
    value_t process, process_airlock_t *airlock) {
  fulfill_promise(deref(state->s_promise), deref(state->s_value));
  return success();
}

void fulfill_promise_undertaking_destroy(runtime_t *runtime, fulfill_promise_state_t *state) {
  safe_value_destroy(runtime, state->s_promise);
  safe_value_destroy(runtime, state->s_value);
  allocator_default_free_struct(fulfill_promise_state_t, state);
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
    string_buffer_printf(context->buf, "#<pending promise ~%w>",
        canonicalize_value_for_print(self, context));
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

static value_t promise_is_settled(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  return new_boolean(is_promise_settled(self));
}

static value_t promise_is_fulfilled(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  return new_boolean(is_promise_fulfilled(self));
}

static value_t promise_fulfilled_value(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  if (!is_promise_fulfilled(self))
    return new_condition(ccInvalidUseOfBuiltin);
  return get_promise_value(self);
}

static value_t promise_rejected_error(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  if (!is_promise_rejected(self))
    return new_condition(ccInvalidUseOfBuiltin);
  return get_promise_error(self);
}

static value_t promise_fulfill(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  value_t value = get_builtin_argument(args, 0);
  fulfill_promise(self, value);
  return value;
}

static value_t promise_reject(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPromise, self);
  value_t error = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofReifiedArguments, error);
  reject_promise(self, error);
  return error;
}

value_t add_promise_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("promise.state", 0, promise_state);
  ADD_BUILTIN_IMPL("promise.is_settled?", 0, promise_is_settled);
  ADD_BUILTIN_IMPL("promise.is_fulfilled?", 0, promise_is_fulfilled);
  ADD_BUILTIN_IMPL("promise.fulfilled_value", 0, promise_fulfilled_value);
  ADD_BUILTIN_IMPL("promise.rejected_error", 0, promise_rejected_error);
  ADD_BUILTIN_IMPL("promise.fulfill!", 1, promise_fulfill);
  ADD_BUILTIN_IMPL("promise.reject!", 1, promise_reject);
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
///
/// A foreign service is an object that is backed by some mechanism outside
/// the runtime so requests are serialized and delivered asynchronously. This is
/// different from an exported service in that exported services receive
/// requests from the outside, a foreign service delivers requests to the
/// outside from within the runtime.
///
/// Some rules of thumb. Sending a request through a foreign service must be
/// deterministic within the same turn. Requests may be delivered synchronously
/// and it's fine for them to fail or succeed immediately, but that result must
/// not become visible until the next turn at the earliest. In particular,
/// issuing a request must not fail synchronously for any reason, it must always
/// succeed even if you know right then and there that it won't succeed and only
/// fail in a later turn. Otherwise you get nondeterminism bleeding into the
/// same turn and we only allow nondeterminism that happens, or modulo cheating
/// appears to happen, between turns.
///
/// With regard to throttling and backpressure that should happen at the point
/// where requests are issued since that's where the initiative to creating the
/// request lies. In particular, it doesn't make sense to throttle delivery of
/// responses since what do you do in that case -- exponential backoff? If the
/// requests keep coming that'll only make things worse. No, throttle at the
/// point where requests are issued and always, at least if at all possible,
/// accept responses as they come in.
///
/// Since requests may be issued to the underlying implementation synchronously,
/// and it may deliver responses synchronously too, response delivery should
/// also not block since otherwise that opens you up to deadlocks. So really,
/// be sure only to issue a request if you know the response can be delivered,
/// and delivered without blocking more than a constant short amount.

FIXED_GET_MODE_IMPL(foreign_service, vmDeepFrozen);
GET_FAMILY_PRIMARY_TYPE_IMPL(foreign_service);

FROZEN_ACCESSORS_IMPL(ForeignService, foreign_service, snInFamily(ofIdHashMap), Impls,
    impls);
FROZEN_ACCESSORS_IMPL(ForeignService, foreign_service, snNoCheck, DisplayName,
    display_name);

value_t foreign_service_validate(value_t self) {
  VALIDATE_FAMILY(ofForeignService, self);
  VALIDATE_FAMILY(ofIdHashMap, get_foreign_service_impls(self));
  return success();
}

void foreign_service_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<foreign_service: ");
  print_on_context_t sub_context = *context;
  sub_context.flags |= pfUnquote;
  value_print_inner_on(get_foreign_service_display_name(self), &sub_context, -1);
  string_buffer_printf(context->buf, ">");
}

// Called when a native request succeeds. Note that there is no guarantee of
// which thread will call this.
static opaque_t on_foreign_request_success(opaque_t opaque_state,
    opaque_t opaque_result) {
  foreign_request_state_t *state = (foreign_request_state_t*) o2p(opaque_state);
  pton_variant_t result = *((pton_variant_t*) o2p(opaque_result));
  pton_assembler_t *assm = pton_new_assembler();
  pton_binary_writer_write(assm, result);
  pton_assembler_peek_code(assm);
  state->result = pton_assembler_release_code(assm);
  pton_dispose_assembler(assm);
  process_airlock_deliver_undertaking(state->airlock, UPCAST_UNDERTAKING(state));
  return o0();
}

void foreign_request_state_init(foreign_request_state_t *state, process_airlock_t *airlock,
    safe_value_t s_surface_promise) {
  undertaking_init(UPCAST_UNDERTAKING(state), &kOutgoingRequestController);
  state->airlock = airlock;
  state->s_surface_promise = s_surface_promise;
  state->result = blob_empty();
  state->request.args = blob_empty();
  process_airlock_begin_undertaking(airlock, UPCAST_UNDERTAKING(state));
}

value_t foreign_request_state_new(runtime_t *runtime, value_t process,
    foreign_request_state_t **result_out) {
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  safe_value_t s_promise = runtime_protect_value(runtime, promise);
  foreign_request_state_t *state = allocator_default_malloc_struct(foreign_request_state_t);
  if (state == NULL)
    return new_system_call_failed_condition("malloc");
  foreign_request_state_init(state, get_process_airlock(process),
      s_promise);
  native_request_t *request = &state->request;
  opaque_promise_t *impl_promise = opaque_promise_pending();
  pton_arena_t *arena = pton_new_arena();
  native_request_init(request, runtime, impl_promise, arena, blob_empty());
  opaque_promise_on_fulfill(impl_promise,
      unary_callback_new_1(on_foreign_request_success, p2o(state)), omTakeOwnership);
  *result_out = state;
  return success();
}

value_t outgoing_request_undertaking_finish(foreign_request_state_t *state,
    value_t process, process_airlock_t *airlock) {
  TRY_DEF(result, plankton_deserialize_data(airlock->runtime, NULL, state->result));
  fulfill_promise(deref(state->s_surface_promise), result);
  return success();
}

void outgoing_request_undertaking_destroy(runtime_t *runtime, foreign_request_state_t *state) {
  opaque_promise_destroy(state->request.impl_promise);
  pton_dispose_arena(state->request.arena);
  safe_value_destroy(runtime, state->s_surface_promise);
  pton_assembler_dispose_code(state->request.args);
  pton_assembler_dispose_code(state->result);
  allocator_default_free_struct(foreign_request_state_t, state);
}

void incoming_request_state_init(incoming_request_state_t *state,
    exported_service_capsule_t *capsule, safe_value_t s_request,
    safe_value_t s_surface_promise, size_t request_count_delta) {
  undertaking_init(UPCAST_UNDERTAKING(state), &kIncomingRequestController);
  state->capsule = capsule;
  state->s_request = s_request;
  state->s_surface_promise = s_surface_promise;
  state->request_count_delta = request_count_delta;
  if (request_count_delta > 0)
    capsule->request_count += request_count_delta;
}

value_t incoming_request_state_new(exported_service_capsule_t *capsule,
    safe_value_t s_request, safe_value_t s_surface_promise,
    size_t request_count_delta, incoming_request_state_t **result_out) {
  incoming_request_state_t *state = allocator_default_malloc_struct(incoming_request_state_t);
  if (state == NULL)
    return new_system_call_failed_condition("malloc");
  incoming_request_state_init(state, capsule, s_request, s_surface_promise,
      request_count_delta);
  *result_out = state;
  return success();
}

value_t incoming_request_undertaking_finish(incoming_request_state_t *state,
    value_t process, process_airlock_t *airlock) {
  CHECK_FAMILY(ofProcess, process);
  runtime_t *runtime = airlock->runtime;
  TRY_DEF(thunk, new_heap_incoming_request_thunk(runtime,
      deref(state->capsule->s_service), deref(state->s_request),
      deref(state->s_surface_promise)));
  job_t job;
  job_init(&job, ROOT(runtime, call_thunk_code_block), thunk, nothing());
  TRY(offer_process_job(runtime, process, &job));
  return success();
}

void incoming_request_undertaking_destroy(runtime_t *runtime,
    incoming_request_state_t *state) {
  if (state->request_count_delta > 0)
    state->capsule->request_count -= state->request_count_delta;
  safe_value_destroy(runtime, state->s_request);
  safe_value_destroy(runtime, state->s_surface_promise);
  allocator_default_free_struct(incoming_request_state_t, state);
}

// Create a plankton-ified copy of the raw arguments.
static value_t foreign_service_clone_args(runtime_t *runtime, value_t raw_args,
    blob_t *args_out) {
  CHECK_FAMILY(ofReifiedArguments, raw_args);
  blob_t data = blob_empty();
  pton_assembler_t *assm = NULL;
  TRY(plankton_serialize_to_data(runtime, raw_args, &data, &assm));
  *args_out = pton_assembler_release_code(assm);
  pton_dispose_assembler(assm);
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
  TRY(foreign_service_clone_args(state->request.runtime, reified,
      &state->request.args));
  unary_callback_call(impl, p2o(&state->request));
  return deref(state->s_surface_promise);
}

value_t add_foreign_service_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("foreign_service.call_with_args", 2, 1,
      foreign_service_call_with_args);
  return success();
}


/// ## Exported service
///
/// An exported service is a neutrino object made accessible outside the
/// runtime. Request can be issued asynchronously from there and will be added
/// to the process' worklist and eventually resolved.

FIXED_GET_MODE_IMPL(exported_service, vmMutable);
GET_FAMILY_PRIMARY_TYPE_IMPL(exported_service);
TRIVIAL_PRINT_ON_IMPL(ExportedService, exported_service);

ACCESSORS_IMPL(ExportedService, exported_service, snInFamily(ofVoidP),
    CapsulePtr, capsule_ptr);
ACCESSORS_IMPL(ExportedService, exported_service, snInFamily(ofProcess),
    Process, process);
ACCESSORS_IMPL(ExportedService, exported_service, snNoCheck, Handler,
    handler);
ACCESSORS_IMPL(ExportedService, exported_service,
    snInFamily(ofModuleFragmentPrivate), Module, module);

exported_service_capsule_t *get_exported_service_capsule(value_t self) {
  value_t ptr = get_exported_service_capsule_ptr(self);
  return (exported_service_capsule_t*) get_void_p_value(ptr);
}

value_t exported_service_validate(value_t self) {
  VALIDATE_FAMILY(ofExportedService, self);
  VALIDATE_FAMILY(ofVoidP, get_exported_service_capsule_ptr(self));
  VALIDATE_FAMILY(ofProcess, get_exported_service_process(self));
  VALIDATE_FAMILY(ofModuleFragmentPrivate, get_exported_service_module(self));
  return success();
}

value_t finalize_exported_service(garbage_value_t dead_self) {
  // Because this deals with a dead object during gc there are hardly any
  // implicit type checks, instead this has to be done with raw offsets and
  // explicit checks. Errors in this code are likely to be a nightmare to debug
  // so extra effort to sanity check everything is worthwhile.
  CHECK_EQ("running exported finalizer on non-exported", ofExportedService,
      get_garbage_object_family(dead_self));
  garbage_value_t dead_capsule_ptr = get_garbage_object_field(dead_self,
      kExportedServiceCapsulePtrOffset);
  CHECK_EQ("invalid exported during finalization", ofVoidP,
      get_garbage_object_family(dead_capsule_ptr));
  garbage_value_t capsule_value = get_garbage_object_field(dead_capsule_ptr,
      kVoidPValueOffset);
  void *raw_capsule_ptr = value_to_pointer_bit_cast(capsule_value.value);
  exported_service_capsule_t *capsule = (exported_service_capsule_t*) raw_capsule_ptr;
  if (capsule == NULL)
    // A null capsule can happen if we run out of memory right in the middle of
    // construction so don't crash on that.
    return success();
  if (!exported_service_capsule_destroy(capsule))
    return new_system_call_failed_condition("free");
  return success();
}

void exported_service_capsule_init(exported_service_capsule_t *capsule,
    safe_value_t s_service) {
  capsule->s_service = s_service;
  capsule->request_count = 0;
}

exported_service_capsule_t *exported_service_capsule_new(runtime_t *runtime,
    safe_value_t s_service) {
  exported_service_capsule_t *capsule = allocator_default_malloc_struct(exported_service_capsule_t);
  if (capsule == NULL)
    return NULL;
  exported_service_capsule_init(capsule, s_service);
  return capsule;
}

bool exported_service_capsule_destroy(exported_service_capsule_t *capsule) {
  CHECK_TRUE("destroying capsule in use", capsule->request_count == 0);
  allocator_default_free_struct(exported_service_capsule_t, capsule);
  return true;
}

bool is_exported_service_weak(value_t self, void *data) {
  CHECK_FAMILY(ofExportedService, self);
  exported_service_capsule_t *capsule = get_exported_service_capsule(self);
  return capsule->request_count == 0;
}

static value_t exported_service_call_with_args(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofExportedService, self);
  value_t reified = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofReifiedArguments, reified);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  exported_service_capsule_t *capsule = get_exported_service_capsule(self);
  incoming_request_state_t *state = NULL;
  safe_value_t s_request = runtime_protect_value(runtime, reified);
  safe_value_t s_promise = runtime_protect_value(runtime, promise);
  // Creating this request increases the request count by 1 since there is
  // nothing else guaranteed to keep the service alive until the request has
  // completed, so we need it to happen explicitly.
  TRY(incoming_request_state_new(capsule, s_request, s_promise, 1, &state));
  value_t process = get_exported_service_process(self);
  process_airlock_t *airlock = get_process_airlock(process);
  process_airlock_begin_undertaking(airlock, UPCAST_UNDERTAKING(state));
  process_airlock_deliver_undertaking(airlock, UPCAST_UNDERTAKING(state));
  return promise;
}

value_t add_exported_service_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("exported_service.call_with_args", 1, 1,
      exported_service_call_with_args);
  return success();
}
