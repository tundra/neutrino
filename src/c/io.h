//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _IO
#define _IO

#include "io/iop.h"
#include "sync.h"
#include "sync/process.h"
#include "utils/eventseq.h"
#include "value.h"

/// ## Pipe

static const size_t kOsPipeSize = HEAP_OBJECT_SIZE(3);
static const size_t kOsPipeNativePtrOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOsPipeInOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kOsPipeOutOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The underlying native pipe.
ACCESSORS_DECL(os_pipe, native_ptr);

// This pipe's input stream.
ACCESSORS_DECL(os_pipe, in);

// This pipe's output stream.
ACCESSORS_DECL(os_pipe, out);

// Returns the native underlying pipe.
native_pipe_t *get_os_pipe_native(value_t self);


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


/// ## Os process

static const size_t kOsProcessSize = HEAP_OBJECT_SIZE(4);
static const size_t kOsProcessNativePtrOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOsProcessStdinLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kOsProcessStdoutLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kOsProcessStderrLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// The underlying native process.
ACCESSORS_DECL(os_process, native_ptr);

// Pointers to stdin/out/err to keep them alive while they're being used by the
// underlying process.
ACCESSORS_DECL(os_process, stdin_lifeline);
ACCESSORS_DECL(os_process, stdout_lifeline);
ACCESSORS_DECL(os_process, stderr_lifeline);

native_process_t *get_os_process_native(value_t self);

value_t new_heap_os_process(runtime_t *runtime);


/// ## Process/iop interface

// An outstanding iop.
struct pending_iop_state_t {
  // Header so this can be used as an external async.
  undertaking_t as_undertaking;
  // Iop.
  iop_t iop;
  // Scratch storage to use for the source or destination data. Owned by the
  // state value so gets deallocated along with it.
  blob_t scratch;
  // The promise to resolve with the result.
  safe_value_t s_promise;
  // The heap stream value this operates on. Accessing this outside the runtime
  // isn't safe so it's only here to keep the stream alive.
  safe_value_t s_stream;
  // The process that initiated this iop. Needs to be kept alive so we can
  // deliver the result to its airlock.
  safe_value_t s_process;
  // Optionally pre-allocated result to eventually resolve the promise with.
  // This gets pre-allocated such that we don't need to do an allocation that
  // might fail when the op has succeeded.
  safe_value_t s_result;
  // The airlock to notify when this iop is complete.
  process_airlock_t *airlock;
};

// Allocates and a new pending iop state on the heap and returns it. The state
// takes ownership of s_promise.
pending_iop_state_t *pending_iop_state_new(blob_t scratch,
    safe_value_t s_promise, safe_value_t s_stream, safe_value_t s_process,
    safe_value_t s_result, process_airlock_t *airlock);


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
