//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "io.h"
#include "sync.h"
#include "sync/pipe.h"
#include "value-inl.h"

/// # Pipe

GET_FAMILY_PRIMARY_TYPE_IMPL(pipe);
FIXED_GET_MODE_IMPL(pipe, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Pipe, pipe);

ACCESSORS_IMPL(Pipe, pipe, acInFamilyOpt, ofVoidP, Native, native);
ACCESSORS_IMPL(Pipe, pipe, acNoCheck, 0, In, in);
ACCESSORS_IMPL(Pipe, pipe, acNoCheck, 0, Out, out);

value_t pipe_validate(value_t self) {
  VALIDATE_FAMILY(ofPipe, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_pipe_native(self));
  VALIDATE_FAMILY(ofOutStream, get_pipe_out(self));
  VALIDATE_FAMILY(ofInStream, get_pipe_in(self));
  return success();
}

value_t new_heap_pipe(runtime_t *runtime) {
  memory_block_t memory = allocator_default_malloc(sizeof(native_pipe_t));
  native_pipe_t *pipe = (native_pipe_t*) memory.memory;
  if (!native_pipe_open(pipe))
    return new_system_call_failed_condition("native_pipe_open");
  TRY_DEF(native, new_heap_void_p(runtime, pipe));
  TRY_DEF(out, new_heap_out_stream(runtime, native_pipe_out(pipe), nothing()));
  TRY_DEF(in, new_heap_in_stream(runtime, native_pipe_in(pipe), nothing()));
  size_t size = kPipeSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, pipe_species)));
  set_pipe_native(result, native);
  set_pipe_out(result, out);
  set_out_stream_lifeline(out, result);
  set_pipe_in(result, in);
  set_in_stream_lifeline(in, result);
  runtime_protect_value_with_flags(runtime, result, tfAlwaysWeak | tfSelfDestruct | tfFinalize, NULL);
  return post_create_sanity_check(result, size);
}

value_t finalize_pipe(garbage_value_t dead_self) {
  CHECK_EQ("running process finalizer on non-process", ofPipe,
      get_garbage_object_family(dead_self));
  garbage_value_t dead_native_ptr = get_garbage_object_field(dead_self,
      kPipeNativeOffset);
  CHECK_EQ("invalid process during finalization", ofVoidP,
      get_garbage_object_family(dead_native_ptr));
  garbage_value_t native_value = get_garbage_object_field(dead_native_ptr,
      kVoidPValueOffset);
  void *pipe = value_to_pointer_bit_cast(native_value.value);
  native_pipe_dispose((native_pipe_t*) pipe);
  allocator_default_free(new_memory_block(pipe, sizeof(native_pipe_t)));
  return success();
}

static value_t pipe_in(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPipe, self);
  return get_pipe_in(self);
}

static value_t pipe_out(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofPipe, self);
  return get_pipe_out(self);
}

value_t add_pipe_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("pipe.in", 0, pipe_in);
  ADD_BUILTIN_IMPL("pipe.out", 0, pipe_out);
  return success();
}


/// ## Out stream

GET_FAMILY_PRIMARY_TYPE_IMPL(out_stream);
FIXED_GET_MODE_IMPL(out_stream, vmMutable);
TRIVIAL_PRINT_ON_IMPL(OutStream, out_stream);

ACCESSORS_IMPL(OutStream, out_stream, acInFamilyOpt, ofVoidP, NativePtr, native_ptr);
ACCESSORS_IMPL(OutStream, out_stream, acNoCheck, 0, Lifeline, lifeline);

out_stream_t *get_out_stream_native(value_t self) {
  value_t ptr = get_out_stream_native_ptr(self);
  return (out_stream_t*) get_void_p_value(ptr);
}

value_t out_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofOutStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_out_stream_native_ptr(self));
  return success();
}

value_t new_heap_out_stream(runtime_t *runtime, out_stream_t *native,
    value_t lifeline) {
  TRY_DEF(native_ptr, new_heap_void_p(runtime, native));
  size_t size = kOutStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, out_stream_species)));
  set_out_stream_native_ptr(result, native_ptr);
  set_out_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

static value_t out_stream_write(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOutStream, self);
  value_t data = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofBlob, data);
  memory_block_t contents = get_blob_data(data);
  // Copy the string contents into a temporary block of memory because the
  // contents may be moved by the gc.
  memory_block_t scratch = allocator_default_malloc(contents.size);
  blob_copy_to(contents, scratch);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  safe_value_t s_promise = runtime_protect_value(runtime, promise);
  safe_value_t s_stream = runtime_protect_value(runtime, self);
  value_t process = get_builtin_process(args);
  safe_value_t s_process = runtime_protect_value(runtime, process);
  process_airlock_t *airlock = get_process_airlock(process);
  pending_iop_state_t *state = pending_iop_state_new(scratch, s_promise, s_stream,
      s_process, protect_immediate(nothing()), airlock);
  iop_init_write(&state->iop, get_out_stream_native(self), scratch.memory,
      scratch.size, p2o(state));
  io_engine_t *io_engine = runtime_get_io_engine(runtime);
  if (!io_engine_schedule(io_engine, state))
    return new_condition(ccWat);
  return promise;
}

value_t add_out_stream_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("out_stream.write", 1, out_stream_write);
  return success();
}


/// ## In stream

GET_FAMILY_PRIMARY_TYPE_IMPL(in_stream);
FIXED_GET_MODE_IMPL(in_stream, vmMutable);
TRIVIAL_PRINT_ON_IMPL(InStream, in_stream);

ACCESSORS_IMPL(InStream, in_stream, acInFamilyOpt, ofVoidP, NativePtr, native_ptr);
ACCESSORS_IMPL(InStream, in_stream, acNoCheck, 0, Lifeline, lifeline);

in_stream_t *get_in_stream_native(value_t self) {
  value_t ptr = get_in_stream_native_ptr(self);
  return (in_stream_t*) get_void_p_value(ptr);
}

value_t in_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofInStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_in_stream_native_ptr(self));
  return success();
}

value_t new_heap_in_stream(runtime_t *runtime, in_stream_t *native,
    value_t lifeline) {
  TRY_DEF(native_ptr, new_heap_void_p(runtime, native));
  size_t size = kInStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, in_stream_species)));
  set_in_stream_native_ptr(result, native_ptr);
  set_in_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

static value_t in_stream_read(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofInStream, self);
  value_t size_val = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, size_val);
  size_t size = (size_t) get_integer_value(size_val);
  memory_block_t scratch = allocator_default_malloc(size);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY_DEF(result, new_heap_blob(runtime, size, afMutable));
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  safe_value_t s_promise = runtime_protect_value(runtime, promise);
  safe_value_t s_stream = runtime_protect_value(runtime, self);
  value_t process = get_builtin_process(args);
  safe_value_t s_process = runtime_protect_value(runtime, process);
  safe_value_t s_result = runtime_protect_value(runtime, result);
  process_airlock_t *airlock = get_process_airlock(process);
  pending_iop_state_t *state = pending_iop_state_new(scratch, s_promise,
      s_stream, s_process, s_result, airlock);
  iop_init_read(&state->iop, get_in_stream_native(self), scratch.memory,
      scratch.size, p2o(state));
  io_engine_t *io_engine = runtime_get_io_engine(runtime);
  if (!io_engine_schedule(io_engine, state))
    return new_condition(ccWat);
  return promise;
}

value_t add_in_stream_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("in_stream.read", 1, in_stream_read);
  return success();
}


/// ## Async interface

value_t pending_iop_state_apply_atomic(pending_iop_state_t *state,
    process_airlock_t *airlock) {
  if (state->iop.type_ == ioRead) {
    value_t result = deref(state->s_result);
    set_blob_data(result, state->scratch);
    TRY(ensure_frozen(airlock->runtime, result));
    fulfill_promise(deref(state->s_promise), result);
  } else {
    fulfill_promise(deref(state->s_promise),
        new_boolean(iop_has_succeeded(&state->iop)));
  }
  return success();
}

pending_iop_state_t *pending_iop_state_new(memory_block_t scratch,
    safe_value_t s_promise, safe_value_t s_stream, safe_value_t s_process,
    safe_value_t s_result, process_airlock_t *airlock) {
  memory_block_t memory = allocator_default_malloc(sizeof(pending_iop_state_t));
  if (memory_block_is_empty(memory))
    return NULL;
  pending_iop_state_t *state = (pending_iop_state_t*) memory.memory;
  state->as_pending_atomic.type = paIopComplete;
  state->scratch = scratch;
  state->s_promise = s_promise;
  state->s_stream = s_stream;
  state->s_process = s_process;
  state->s_result = s_result;
  state->airlock = airlock;
  return state;
}

void pending_iop_state_destroy(runtime_t *runtime, pending_iop_state_t *state) {
  allocator_default_free(state->scratch);
  safe_value_destroy(runtime, state->s_promise);
  safe_value_destroy(runtime, state->s_stream);
  safe_value_destroy(runtime, state->s_process);
  safe_value_destroy(runtime, state->s_result);
  iop_dispose(&state->iop);
  memory_block_t memory = new_memory_block(state, sizeof(pending_iop_state_t));
  allocator_default_free(memory);
}


/// ## I/O engine

#include "utils/log.h"

static void io_engine_activate_pending(io_engine_t *engine,
    pending_iop_state_t *state) {
  iop_group_schedule(&engine->iop_group, &state->iop);
}

// Transfer any iops currently pending in the IO engine's incoming worklist to
// the iop group we'll select on.
static void io_engine_transfer_pending(io_engine_t *engine) {
  while (true) {
    opaque_t next = o0();
    if (worklist_take(kIoEngineMaxIncoming, 1)(&engine->incoming, &next, 1,
        duration_instant())) {
      io_engine_activate_pending(engine, (pending_iop_state_t*) o2p(next));
    } else {
      // As soon as we run out of pending ops we don't wait for new ones, we
      // just move on.
      break;
    }
  }
}

// Waits the given duration for a pending operation to be added to the set to be
// performed by this engine.
static void io_engine_wait_for_pending(io_engine_t *engine, duration_t timeout) {
  opaque_t next = o0();
  if (worklist_take(kIoEngineMaxIncoming, 1)(&engine->incoming, &next, 1,
      timeout))
    io_engine_activate_pending(engine, (pending_iop_state_t*) o2p(next));
}

static void io_engine_select(io_engine_t *engine, duration_t timeout) {
  if (iop_group_pending_count(&engine->iop_group) == 0) {
    // If there are no pending ops in the group we block waiting for ops to be
    // added. We're willing to wait the full timeout so if an op is scheduled
    // we don't perform it in this round, we loop around again first.
    io_engine_wait_for_pending(engine, timeout);
  } else {
    opaque_t opaque_state = o0();
    if (!iop_group_wait_for_next(&engine->iop_group, timeout, &opaque_state))
      return;
    pending_iop_state_t *state = (pending_iop_state_t*) o2p(opaque_state);
    process_airlock_schedule_atomic(state->airlock,
        UPCAST_TO_PENDING_ATOMIC(state));
  }
}

bool io_engine_is_idle(io_engine_t *engine) {
  return worklist_is_empty(kIoEngineMaxIncoming, 1)(&engine->incoming)
      && iop_group_pending_count(&engine->iop_group) == 0;
}

// Is it time for this engine to shut down?
static bool io_engine_shut_down(io_engine_t *engine) {
  return io_engine_is_idle(engine) && engine->terminate_when_idle;
}

// The main loop of the io engine's thread.
static void io_engine_main_loop(io_engine_t *engine) {
  duration_t interval = duration_seconds(0.1);
  while (!io_engine_shut_down(engine)) {
    io_engine_transfer_pending(engine);
    io_engine_select(engine, interval);
  }
}

// Allows the main loop to be called from a callback.
static opaque_t io_engine_main_loop_bridge(opaque_t opaque_io_engine) {
  io_engine_t *engine = (io_engine_t*) o2p(opaque_io_engine);
  io_engine_main_loop(engine);
  return o0();
}

io_engine_t *io_engine_new() {
  memory_block_t memory = allocator_default_malloc(sizeof(io_engine_t));
  if (memory_block_is_empty(memory))
    return NULL;
  io_engine_t *engine = (io_engine_t*) memory.memory;
  engine->terminate_when_idle = false;
  if (!worklist_init(kIoEngineMaxIncoming, 1)(&engine->incoming))
    return NULL;
  iop_group_initialize(&engine->iop_group);
  engine->main_loop_callback = nullary_callback_new_1(io_engine_main_loop_bridge,
      p2o(engine));
  engine->thread = native_thread_new(engine->main_loop_callback);
  native_thread_start(engine->thread);
  return engine;
}

void io_engine_destroy(io_engine_t *engine) {
  CHECK_FALSE("io engine already shutting down", engine->terminate_when_idle);
  engine->terminate_when_idle = true;
  native_thread_join(engine->thread);
  CHECK_TRUE("disposing non idle engine", io_engine_is_idle(engine));
  native_thread_destroy(engine->thread);
  callback_destroy(engine->main_loop_callback);
  worklist_dispose(kIoEngineMaxIncoming, 1)(&engine->incoming);
  iop_group_dispose(&engine->iop_group);
  memory_block_t memory = new_memory_block(engine, sizeof(io_engine_t));
  allocator_default_free(memory);
}

bool io_engine_schedule(io_engine_t *engine, pending_iop_state_t *op) {
  CHECK_FALSE("scheduling while terminating", engine->terminate_when_idle);
  opaque_t opaque_op = p2o(op);
  return worklist_schedule(kIoEngineMaxIncoming, 1)(&engine->incoming,
      &opaque_op, 1, duration_unlimited());
}
