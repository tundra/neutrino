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


#endif // _SYNC
