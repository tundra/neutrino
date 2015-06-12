//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _IO
#define _IO

#include "sync.h"
#include "value.h"
#include "io/iop.h"

/// ## Pipe

static const size_t kOsPipeSize = HEAP_OBJECT_SIZE(3);
static const size_t kOsPipeNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOsPipeInOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kOsPipeOutOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The underlying native pipe.
ACCESSORS_DECL(os_pipe, native_ptr);

// This pipe's input stream.
ACCESSORS_DECL(os_pipe, in);

// This pipe's output stream.
ACCESSORS_DECL(os_pipe, out);


/// ## Out stream

static const size_t kOsOutStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kOsOutStreamNativePtrOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOsOutStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native out stream.
ACCESSORS_DECL(os_out_stream, native_ptr);

// Optional value kept alive by this out stream.
ACCESSORS_DECL(os_out_stream, lifeline);

out_stream_t *get_os_out_stream_native(value_t self);


/// ## In stream

static const size_t kOsInStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kOsInStreamNativePtrOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOsInStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native in stream.
ACCESSORS_DECL(os_in_stream, native_ptr);

// Optional value kept alive by this in stream.
ACCESSORS_DECL(os_in_stream, lifeline);

in_stream_t *get_os_in_stream_native(value_t self);


/// ## Process interface

// An outstanding iop.
typedef struct {
  // Header so this can be used as an abstract pending atomic.
  pending_atomic_t as_pending_atomic;
  // Iop.
  iop_t iop;
  // Scratch storage to use for the source or destination data. Owned by the
  // state value so gets deallocated along with it.
  memory_block_t scratch;
  // The promise to resolve with the result.
  safe_value_t s_promise;
  // The heap stream value this operates on. Accessing this outside the runtime
  // isn't safe so it's only here to keep the stream alive.
  safe_value_t s_stream;
  // The process that initiated this iop. Needs to be kept alive so we can
  // deliver the result to its airlock.
  safe_value_t s_process;
  // Optionally pre-allocated result to eventually resolve the promise with.
  // This gets pre-allocateds uch that we don't need to do an allocation that
  // might fail when the op has succeeded.
  safe_value_t s_result;
  // The airlock to notify when this iop is complete.
  process_airlock_t *airlock;
} pending_iop_state_t;

// When the process is ready for the result of the iop to be delivered this
// will be called.
value_t pending_iop_state_apply_atomic(pending_iop_state_t *state,
    process_airlock_t *airlock);

// Allocates and a new pending iop state on the heap and returns it. The state
// takes ownership of s_promise.
pending_iop_state_t *pending_iop_state_new(memory_block_t scratch,
    safe_value_t s_promise, safe_value_t s_stream, safe_value_t s_process,
    safe_value_t s_result, process_airlock_t *airlock);

// Disposes a pending iop state after its result has been applied.
void pending_iop_state_destroy(runtime_t *runtime, pending_iop_state_t *state);


/// ## I/O engine abstraction

#define kIoEngineMaxIncoming 16

// The I/O engine is an abstraction that performs asynchronous native I/O on
// behalf of the runtime. It multiplexes any I/O operations to perform and
// dispatches the results back through pending atomic ops to the respective
// processes that initiate the operations.
struct io_engine_t {
  nullary_callback_t *main_loop_callback;
  native_thread_t *thread;
  bool terminate_when_idle;
  iop_group_t iop_group;
  worklist_t(kIoEngineMaxIncoming, 1) incoming;
};

// Creates a new I/O engine, starting up the background thread that performs
// the I/O.
io_engine_t *io_engine_new();

// Disposes and frees an I/O engine. This may include some amount of blocking
// because this involves shutting down worker threads that are currently
// blocked waiting for I/O and we have to wait for those to finish up.
void io_engine_destroy(io_engine_t *engine);

// Schedule an operation to be performed by the given IO engine.
bool io_engine_schedule(io_engine_t *engine, pending_iop_state_t *op);

// Returns true if the given engine currently has no more work.
bool io_engine_is_idle(io_engine_t *engine);

#endif // _IO
