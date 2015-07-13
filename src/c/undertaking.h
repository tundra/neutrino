//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _UNDERTAKING
#define _UNDERTAKING

#include "globals.h"
#include "process.h"
#include "value.h"

// List all the different kinds of undertakings that exist.
// Name              name              type that holds the state
#define ENUM_UNDERTAKINGS(F)                                                   \
  F(OutgoingRequest, outgoing_request, foreign_request_state_t)                \
  F(PerformIop,      perform_iop,      pending_iop_state_t)                    \
  F(FulfillPromise,  fulfill_promise,  fulfill_promise_state_t)                \
  F(IncomingRequest, incoming_request, incoming_request_state_t)

typedef struct undertaking_controller_t undertaking_controller_t;

// The current state of an undertaking. For sanity checking and debugging only.
typedef enum {
  usInitialized,
  usBegun,
  usDelivered,
  usFinished
} undertaking_state_t;

// An abstract type that encapsulates a an asynchronous operation external to
// the runtime, that is, an operation that can be completed concurrently to the
// interpreter. An undertaking has three phases.
//
//   1. Once it has been created it is *begun*, potentially asynchronously. The
//      process needs to know many undertakings have been begun so that it can
//      wait for outstanding ones, that's the purpose of beginning.
//   2. When the external process is complete the undertaking must be
//      *delivered* to the airlock of the process. This can also happen
//      asynchronously. Its effect will not be evident to the surface language
//      though since we don't allow concurrent effects within turns, it is
//      just buffered.
//   3. At some point after the current turn the process will *finish* any
//      undertakings that have been delivered, making their result evident to
//      the surface language in whatever way is appropriate.
//
// Intuitively, beginning an undertaking amounts to promising the process to
// call deliver eventually, and calling deliver amounts to requesting that the
// process call finish on the undertaking from the interpreter thread.
struct undertaking_t {
  undertaking_controller_t *controller;
  undertaking_state_t state;
};

// Initialize an undertaking whose behavior is determined by the given
// controller.
void undertaking_init(undertaking_t *undertaking, undertaking_controller_t *controller);

typedef value_t (undertaking_finish_f)(undertaking_t *self, value_t process, process_airlock_t *airlock);
typedef void (undertaking_destroy_f)(runtime_t *runtime, undertaking_t *self);

// A hand-rolled virtual table for undertakings.
struct undertaking_controller_t {
  // Called by the process to finish the undertaking.
  undertaking_finish_f *finish;
  // Called by the process to destroy the undertaking struct.
  undertaking_destroy_f *destroy;
};

// Declare all the undertaking controllers.
#define DECLARE_UNDERTAKING_CONTROLLER(Name, name, type_t)                     \
extern undertaking_controller_t k##Name##Controller;
ENUM_UNDERTAKINGS(DECLARE_UNDERTAKING_CONTROLLER)
#undef DECLARE_UNDERTAKING_CONTROLLER

// Declare the undertaking handle functions. Note that these are slightly
// different in signature than what will be stored in the controller's fields.
// This should be safe.
#define __UNDERTAKING_FORWARD_DECLS__(Name, name, type_t)                      \
  typedef struct type_t type_t;                                                \
  value_t name##_undertaking_finish(type_t*, value_t, process_airlock_t*);     \
  void name##_undertaking_destroy(runtime_t*, type_t*);
ENUM_UNDERTAKINGS(__UNDERTAKING_FORWARD_DECLS__)
#undef __UNDERTAKING_FORWARD_DECLS__

// Given a state struct that can be used as an undertaking, returns it viewed
// as an undertaking.
#define UPCAST_UNDERTAKING(EXPR) (&(EXPR)->as_undertaking)

#endif // _UNDERTAKING
