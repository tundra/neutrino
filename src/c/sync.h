//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Higher-level allocation routines that allocate and initialize objects in a
// given heap.


#ifndef _SYNC
#define _SYNC

#include "check.h"
#include "plugin.h"
#include "tagged.h"
#include "undertaking.h"
#include "value.h"

/// ## Promise

static const size_t kPromiseSize = HEAP_OBJECT_SIZE(2);
static const size_t kPromiseStateOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kPromisePayloadOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The current state of this promise.
ACCESSORS_DECL(promise, state);

// For fulfilled promises the value, for rejected the error.
ACCESSORS_DECL(promise, payload);

// Returns true if the given promise is in a settled (non-pending) state.
bool is_promise_settled(value_t self);

// Has this promise been settled by fulfilling?
bool is_promise_fulfilled(value_t self);

// Has this promise been settled by rejecting?
bool is_promise_rejected(value_t self);

// Returns the value of the given promise which must have been fulfilled.
value_t get_promise_value(value_t self);

// Returns the error stored in the given promise which must have been rejected.
value_t get_promise_error(value_t self);

// Fulfill the given promise if it hasn't been already, otherwise this is a
// noop.
void fulfill_promise(value_t self, value_t value);

// Fail the given promise if it hasn't been already, otherwise this is a
// noop.
void reject_promise(value_t self, value_t error);

// Schedule for the given promise to be fulfilled to the given value at some
// point after the end of the current turn.
value_t schedule_promise_fulfill_atomic(runtime_t *runtime, value_t self,
    value_t value, value_t process);


/// ## Promise state

// The possible states of a promise. See
// https://github.com/domenic/promises-unwrapping/blob/master/docs/states-and-fates.md
// which seems like a terribly reasonable terminology.
typedef enum {
  psPending = 0x1,
  psFulfilled = 0x2,
  psRejected = 0x4
} promise_state_t;

// Creates a promise state value representing the given promise state.
static value_t new_promise_state(promise_state_t state) {
  return new_custom_tagged(tpPromiseState, state);
}

// Returns the pending promise state.
static value_t promise_state_pending() {
  return new_promise_state(psPending);
}

// Returns the fulfilled promise state.
static value_t promise_state_fulfilled() {
  return new_promise_state(psFulfilled);
}

// Returns the rejected promise state.
static value_t promise_state_rejected() {
  return new_promise_state(psRejected);
}

// Returns the enum value indicating the type of this relation.
static promise_state_t get_promise_state_value(value_t self) {
  CHECK_PHYLUM(tpPromiseState, self);
  return (promise_state_t) get_custom_tagged_payload(self);
}

// Does this promise state value represent a state that is not pending?
static bool is_promise_state_settled(value_t self) {
  CHECK_PHYLUM(tpPromiseState, self);
  return !is_same_value(self, promise_state_pending());
}

/// ## Foreign service
///
/// A foreign service is a remote process that is immediately backed by an
/// in-process native implementation.

// Extra state maintained around a foreign request.
struct foreign_request_state_t {
  undertaking_t as_undertaking;
  // The part of the data that will be passed to the native impl.
  native_request_t request;
  // The airlock of the process to return the result to.
  process_airlock_t *airlock;
  // The promise value that will be delivered to the surface language to
  // represent the result of this request.
  safe_value_t s_surface_promise;
  // This is where the result will be held between the request completing and
  // the process delivering it to the promise.
  blob_t result;
};

// The state associated with a delayed promise fulfillment.
struct fulfill_promise_state_t {
  undertaking_t as_undertaking;
  safe_value_t s_promise;
  safe_value_t s_value;
};

void foreign_request_state_init(foreign_request_state_t *state,
    process_airlock_t *airlock, safe_value_t s_surface_promise);

// Create an initialize a new native request state. Note that the arguments
// will not have been set, this only initializes the rest.
value_t foreign_request_state_new(runtime_t *runtime, value_t process,
    foreign_request_state_t **result_out);

static const size_t kForeignServiceSize = HEAP_OBJECT_SIZE(2);
static const size_t kForeignServiceImplsOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kForeignServiceDisplayNameOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// Mapping from method names to native implementations.
FROZEN_ACCESSORS_DECL(foreign_service, impls);

// Display name used to identify the value.
FROZEN_ACCESSORS_DECL(foreign_service, display_name);


/// ## Exported service

// Extra state maintained around a request to an exported service.
struct incoming_request_state_t {
  undertaking_t as_undertaking;
  // Capsule for the service that should handle this request.
  exported_service_capsule_t *capsule;
  // The request data.
  safe_value_t s_request;
  // The promise to resolve with the result.
  safe_value_t s_surface_promise;
  // How much did creating this request increase the capsule's request count?
  size_t request_count_delta;
};

// Initialize the given state. Note that this may change the request count of
// the capsule as a side-effect.
void incoming_request_state_init(incoming_request_state_t *state,
    exported_service_capsule_t *capsule, safe_value_t s_request,
    safe_value_t s_surface_promise, size_t request_count_delta);

// Returns a new incomine request state. Note that this may change the request
// count of the capsule as a side-effect.
value_t incoming_request_state_new(exported_service_capsule_t *capsule,
    safe_value_t s_request, safe_value_t s_surface_promise,
    size_t request_count_delta, incoming_request_state_t **result_out);

// State allocated on the C heap associated with an exported service. Unlike the
// service itself which may move around in the heap, this state can safely be
// passed around outside the runtime and between threads. However: it must only
// be passed around and back into the runtime, other threads must not change
// the state.
struct exported_service_capsule_t {
  // Reference to the service being exported.
  safe_value_t s_service;
  // Number of outstanding requests; this is taken into account when deciding
  // whether the service is still alive and so can be used as a refcount by
  // outstanding requests to keep it alive until they're complete.
  size_t request_count;
};

// Set up the given capsule struct.
void exported_service_capsule_init(exported_service_capsule_t *capsule,
    safe_value_t s_service);

// Create and return a new export service capsule.
exported_service_capsule_t *exported_service_capsule_new(runtime_t *runtime,
    safe_value_t s_service);

// Destroy the given capsule. The capsule must be dead, that is, there can
// be no active refcounts.
bool exported_service_capsule_destroy(exported_service_capsule_t *capsule);

// Returns true iff the given exported service can be safely collected.
bool is_exported_service_weak(value_t sellf, void *data);

static const size_t kExportedServiceSize = HEAP_OBJECT_SIZE(4);
static const size_t kExportedServiceCapsulePtrOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kExportedServiceProcessOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kExportedServiceHandlerOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kExportedServiceModuleOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// This service's capsule of externally visible data.
ACCESSORS_DECL(exported_service, capsule_ptr);

// Returns the capsule struct for the given service.
exported_service_capsule_t *get_exported_service_capsule(value_t process);

// The process to which this service belongs.
ACCESSORS_DECL(exported_service, process);

// The handler object that implements the service's behavior.
ACCESSORS_DECL(exported_service, handler);

// The module within which methods will be resolved.
ACCESSORS_DECL(exported_service, module);

#endif // _SYNC
