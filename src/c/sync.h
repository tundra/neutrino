//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Higher-level allocation routines that allocate and initialize objects in a
// given heap.


#ifndef _SYNC
#define _SYNC

#include "value.h"

/// ## Promise

static const size_t kPromiseSize = HEAP_OBJECT_SIZE(2);
static const size_t kPromiseStateOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kPromiseValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The current state of this promise.
ACCESSORS_DECL(promise, state);

// For resolved promises the value, for failed the failure.
ACCESSORS_DECL(promise, value);

// Returns true if the given promise is in a resolved (non-pending) state.
bool is_promise_resolved(value_t self);

// Fulfill the given promise if it hasn't been already, otherwise this is a
// noop.
void fulfill_promise(value_t self, value_t value);


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

// Does this promise state value represent a state the is considered resolved?
static bool is_promise_state_resolved(value_t self) {
  CHECK_PHYLUM(tpPromiseState, self);
  return !is_same_value(self, promise_state_pending());
}


/// ## Foreign service
///
/// A foreign service is a remote process that is immediately backed by an
/// in-process native implementation.

// Extra state maintained around a foreign request.
struct foreign_request_state_t {
  // TODO: make gc safe.
  // The part of the data that will be passed to the native impl.
  native_request_t request;
  // The callback that will be called; kept around so we can destroy it later.
  unary_callback_t *callback;
  // The airlock of the process to return the result to.
  process_airlock_t *airlock;
  // The promise value that will be delivered to the surface language to
  // represent the result of this request.
  value_t surface_promise;
  // This is where the result will be held between the request completing and
  // the process delivering it to the promise.
  pton_variant_t result;
};

void foreign_request_state_init(foreign_request_state_t *state,
    unary_callback_t *callback, process_airlock_t *airlock,
    value_t surface_promise, pton_variant_t result);

// Create an initialize a new native request state. Note that the arguments
// will not have been set, this only initializes the rest.
value_t foreign_request_state_new(runtime_t *runtime, value_t process,
    foreign_request_state_t **result_out);

// Destroy the given state.
void foreign_request_state_destroy(foreign_request_state_t *state);

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
  // XXX: make GC safe.
  exported_service_capsule_t *capsule;
  value_t request;
  value_t surface_promise;
};

value_t incoming_request_state_new(exported_service_capsule_t *capsule,
    value_t request, value_t surface_promise, incoming_request_state_t **result_out);

void incoming_request_state_destroy(incoming_request_state_t *state);

// State allocated on the C heap associated with an exported service. Unlike the
// service itself which may move around in the heap, this state can safely be
// passed around outside the runtime and between threads.
struct exported_service_capsule_t {
  // XXX: make GC safe.
  value_t service;
};

// Set up the given capsule struct.
void exported_service_capsule_init(exported_service_capsule_t *capsule,
    value_t service);

// Create and return a new export service capsule.
exported_service_capsule_t *exported_service_capsule_new(runtime_t *runtime,
    value_t service);

bool exported_service_capsule_destroy(exported_service_capsule_t *capsule);

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
