//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _IO
#define _IO

#include "sync.h"
#include "value.h"
#include "io/iop.h"

/// ## Pipe

static const size_t kPipeSize = HEAP_OBJECT_SIZE(3);
static const size_t kPipeNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kPipeInOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kPipeOutOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The underlying native pipe.
ACCESSORS_DECL(pipe, native);

// This pipe's input stream.
ACCESSORS_DECL(pipe, in);

// This pipe's output stream.
ACCESSORS_DECL(pipe, out);


/// ## Out stream

static const size_t kOutStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kOutStreamNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOutStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native out stream.
ACCESSORS_DECL(out_stream, native);

// Optional value kept alive by this out stream.
ACCESSORS_DECL(out_stream, lifeline);


/// ## In stream

static const size_t kInStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kInStreamNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kInStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native in stream.
ACCESSORS_DECL(in_stream, native);

// Optional value kept alive by this in stream.
ACCESSORS_DECL(in_stream, lifeline);


/// ## Process interface

// An outstanding iop.
typedef struct {
  pending_atomic_t as_pending_atomic;
  read_iop_t as_read;
  safe_value_t s_promise;
} pending_iop_state_t;

// When the process is ready for the result of the iop to be delivered this
// will be called.
value_t pending_iop_state_apply_atomic(pending_iop_state_t *state,
    process_airlock_t *airlock);

// Allocates and a new pending iop state on the heap and returns it. The state
// takes ownership of s_promise.
pending_iop_state_t *pending_iop_state_new(safe_value_t s_promise);

// Disposes a pending iop state after its result has been applied.
void pending_iop_state_destroy(runtime_t *runtime, pending_iop_state_t *state);


/// ## I/O engine abstraction

struct io_engine_t {
  nullary_callback_t *main_loop_callback;
  native_thread_t *thread;
};

// Creates a new I/O engine.
io_engine_t *io_engine_new();

// Disposes and frees an I/O engine.
void io_engine_destroy(io_engine_t *engine);

#endif // _IO
