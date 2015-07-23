//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "check.h"
#include "freeze.h"
#include "io.h"
#include "sync.h"
#include "sync/pipe.h"
#include "utils/log.h"
#include "value-inl.h"

/// # Pipe

GET_FAMILY_PRIMARY_TYPE_IMPL(os_pipe);
FIXED_GET_MODE_IMPL(os_pipe, vmMutable);
TRIVIAL_PRINT_ON_IMPL(OsPipe, os_pipe);

ACCESSORS_IMPL(OsPipe, os_pipe, acInFamilyOpt(ofVoidP), NativePtr, native_ptr);
ACCESSORS_IMPL(OsPipe, os_pipe, acInFamilyOpt(ofOsInStream), In, in);
ACCESSORS_IMPL(OsPipe, os_pipe, acInFamilyOpt(ofOsOutStream), Out, out);

native_pipe_t *get_os_pipe_native(value_t self) {
  value_t ptr = get_os_pipe_native_ptr(self);
  return (native_pipe_t*) get_void_p_value(ptr);
}

value_t os_pipe_validate(value_t self) {
  VALIDATE_FAMILY(ofOsPipe, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_os_pipe_native_ptr(self));
  VALIDATE_FAMILY(ofOsOutStream, get_os_pipe_out(self));
  VALIDATE_FAMILY(ofOsInStream, get_os_pipe_in(self));
  return success();
}

value_t new_heap_os_pipe(runtime_t *runtime) {
  native_pipe_t *pipe = allocator_default_malloc_struct(native_pipe_t);
  if (!native_pipe_open(pipe))
    return new_system_call_failed_condition("native_pipe_open");
  TRY_DEF(native, new_heap_void_p(runtime, pipe));
  TRY_DEF(out, new_heap_os_out_stream(runtime, native_pipe_out(pipe), nothing()));
  TRY_DEF(in, new_heap_os_in_stream(runtime, native_pipe_in(pipe), nothing()));
  size_t size = kOsPipeSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, os_pipe_species)));
  set_os_pipe_native_ptr(result, native);
  set_os_pipe_out(result, out);
  set_os_out_stream_lifeline(out, result);
  set_os_pipe_in(result, in);
  set_os_in_stream_lifeline(in, result);
  runtime_protect_value_with_flags(runtime, result, tfAlwaysWeak | tfSelfDestruct | tfFinalize, NULL);
  return post_create_sanity_check(result, size);
}

value_t finalize_os_pipe(garbage_value_t dead_self) {
  CHECK_EQ("running os pipe finalizer on non-os-pipe", ofOsPipe,
      get_garbage_object_family(dead_self));
  garbage_value_t dead_native_ptr = get_garbage_object_field(dead_self,
      kOsPipeNativePtrOffset);
  CHECK_EQ("invalid os pipe during finalization", ofVoidP,
      get_garbage_object_family(dead_native_ptr));
  garbage_value_t native_value = get_garbage_object_field(dead_native_ptr,
      kVoidPValueOffset);
  void *pipe = value_to_pointer_bit_cast(native_value.value);
  native_pipe_dispose((native_pipe_t*) pipe);
  allocator_default_free_struct(native_pipe_t, pipe);
  return success();
}

static value_t os_pipe_in(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOsPipe, self);
  return get_os_pipe_in(self);
}

static value_t os_pipe_out(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOsPipe, self);
  return get_os_pipe_out(self);
}

value_t add_os_pipe_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("os_pipe.in", 0, os_pipe_in);
  ADD_BUILTIN_IMPL("os_pipe.out", 0, os_pipe_out);
  return success();
}


/// ## Out stream

GET_FAMILY_PRIMARY_TYPE_IMPL(os_out_stream);
FIXED_GET_MODE_IMPL(os_out_stream, vmMutable);
TRIVIAL_PRINT_ON_IMPL(OsOutStream, os_out_stream);

ACCESSORS_IMPL(OsOutStream, os_out_stream, acInFamilyOpt(ofVoidP), NativePtr, native_ptr);
ACCESSORS_IMPL(OsOutStream, os_out_stream, acNoCheck, Lifeline, lifeline);

out_stream_t *get_os_out_stream_native(value_t self) {
  value_t ptr = get_os_out_stream_native_ptr(self);
  return (out_stream_t*) get_void_p_value(ptr);
}

value_t os_out_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofOsOutStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_os_out_stream_native_ptr(self));
  return success();
}

value_t new_heap_os_out_stream(runtime_t *runtime, out_stream_t *native,
    value_t lifeline) {
  TRY_DEF(native_ptr, new_heap_void_p(runtime, native));
  size_t size = kOsOutStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, os_out_stream_species)));
  set_os_out_stream_native_ptr(result, native_ptr);
  set_os_out_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

static value_t os_out_stream_write(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOsOutStream, self);
  value_t data = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofBlob, data);
  blob_t contents = get_blob_data(data);
  // Copy the string contents into a temporary block of memory because the
  // contents may be moved by the gc.
  blob_t scratch = allocator_default_malloc(contents.size);
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
  iop_init_write(&state->iop, get_os_out_stream_native(self), scratch.start,
      scratch.size, p2o(state));
  io_engine_t *io_engine = runtime_get_io_engine(runtime);
  if (!io_engine_schedule(io_engine, state))
    return new_condition(ccWat);
  return promise;
}

static value_t os_out_stream_close(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOsOutStream, self);
  out_stream_t *out = get_os_out_stream_native(self);
  out_stream_close(out);
  return null();
}

value_t add_os_out_stream_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("os_out_stream.write!", 1, os_out_stream_write);
  ADD_BUILTIN_IMPL("os_out_stream.close!", 0, os_out_stream_close);
  return success();
}


/// ## In stream

GET_FAMILY_PRIMARY_TYPE_IMPL(os_in_stream);
FIXED_GET_MODE_IMPL(os_in_stream, vmMutable);
TRIVIAL_PRINT_ON_IMPL(OsInStream, os_in_stream);

ACCESSORS_IMPL(OsInStream, os_in_stream, acInFamilyOpt(ofVoidP), NativePtr, native_ptr);
ACCESSORS_IMPL(OsInStream, os_in_stream, acNoCheck, Lifeline, lifeline);

in_stream_t *get_os_in_stream_native(value_t self) {
  value_t ptr = get_os_in_stream_native_ptr(self);
  return (in_stream_t*) get_void_p_value(ptr);
}

value_t os_in_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofOsInStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_os_in_stream_native_ptr(self));
  return success();
}

value_t new_heap_os_in_stream(runtime_t *runtime, in_stream_t *native,
    value_t lifeline) {
  TRY_DEF(native_ptr, new_heap_void_p(runtime, native));
  size_t size = kOsInStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, os_in_stream_species)));
  set_os_in_stream_native_ptr(result, native_ptr);
  set_os_in_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

static value_t os_in_stream_read(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOsInStream, self);
  value_t size_val = get_builtin_argument(args, 0);
  CHECK_DOMAIN(vdInteger, size_val);
  size_t size = (size_t) get_integer_value(size_val);
  blob_t scratch = allocator_default_malloc(size);
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
  iop_init_read(&state->iop, get_os_in_stream_native(self), scratch.start,
      scratch.size, p2o(state));
  io_engine_t *io_engine = runtime_get_io_engine(runtime);
  if (!io_engine_schedule(io_engine, state))
    return new_condition(ccWat);
  return promise;
}

value_t add_os_in_stream_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("os_in_stream.read!", 1, os_in_stream_read);
  return success();
}


/// ## Os process

GET_FAMILY_PRIMARY_TYPE_IMPL(os_process);
FIXED_GET_MODE_IMPL(os_process, vmMutable);
TRIVIAL_PRINT_ON_IMPL(OsProcess, os_process);

ACCESSORS_IMPL(OsProcess, os_process, acInFamilyOpt(ofVoidP), NativePtr, native_ptr);
ACCESSORS_IMPL(OsProcess, os_process, acNoCheck, StdinLifeline, stdin_lifeline);
ACCESSORS_IMPL(OsProcess, os_process, acNoCheck, StdoutLifeline, stdout_lifeline);
ACCESSORS_IMPL(OsProcess, os_process, acNoCheck, StderrLifeline, stderr_lifeline);

native_process_t *get_os_process_native(value_t self) {
  value_t ptr = get_os_process_native_ptr(self);
  return (native_process_t*) get_void_p_value(ptr);
}

value_t os_process_validate(value_t self) {
  VALIDATE_FAMILY(ofOsProcess, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_os_process_native_ptr(self));
  return success();
}

value_t new_heap_os_process(runtime_t *runtime) {
  native_process_t *native = native_process_new();
  TRY_DEF(native_ptr, new_heap_void_p(runtime, native));
  size_t size = kOsProcessSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, os_process_species)));
  set_os_process_native_ptr(result, native_ptr);
  set_os_process_stdin_lifeline(result, nothing());
  set_os_process_stdout_lifeline(result, nothing());
  set_os_process_stderr_lifeline(result, nothing());
  runtime_protect_value_with_flags(runtime, result, tfAlwaysWeak | tfSelfDestruct | tfFinalize, NULL);
  return post_create_sanity_check(result, size);
}

value_t finalize_os_process(garbage_value_t dead_self) {
  CHECK_EQ("running os process finalizer on non-os-process", ofOsProcess,
      get_garbage_object_family(dead_self));
  garbage_value_t dead_native_ptr = get_garbage_object_field(dead_self,
      kOsProcessNativePtrOffset);
  CHECK_EQ("invalid os process during finalization", ofVoidP,
      get_garbage_object_family(dead_native_ptr));
  garbage_value_t native_value = get_garbage_object_field(dead_native_ptr,
      kVoidPValueOffset);
  void *process = value_to_pointer_bit_cast(native_value.value);
  native_process_destroy((native_process_t*) process);
  return success();
}

static void on_exit_code_ready(process_airlock_t *airlock,
    fulfill_promise_state_t *state, int exit_code) {
  state->s_value = protect_immediate(new_integer(exit_code));
  process_airlock_deliver_undertaking(airlock, UPCAST_UNDERTAKING(state));
}

static opaque_t on_exit_code_ready_bridge(opaque_t o_airlock, opaque_t o_state,
    opaque_t o_exit_code) {
  on_exit_code_ready((process_airlock_t*) o2p(o_airlock),
      (fulfill_promise_state_t*) o2p(o_state),
      (int) o2u(o_exit_code));
  return o0();
}

static value_t os_process_start(builtin_arguments_t *args) {
  value_t os_process = get_builtin_subject(args);
  CHECK_FAMILY(ofOsProcess, os_process);
  utf8_t executable = get_utf8_contents(get_builtin_argument(args, 0));
  value_t arguments = get_builtin_argument(args, 1);
  CHECK_FAMILY(ofArray, arguments);
  value_t exit_code_promise = get_builtin_argument(args, 2);
  CHECK_FAMILY(ofPromise, exit_code_promise);
  native_process_t *native = get_os_process_native(os_process);
  size_t argc = (size_t) get_array_length(arguments);
  utf8_t *argv = allocator_default_malloc_structs(utf8_t, argc);
  for (size_t i = 0; i < argc; i++) {
    value_t arg = get_array_at(arguments, i);
    CHECK_FAMILY(ofUtf8, arg);
    argv[i] = get_utf8_contents(arg);
  }
  bool started = native_process_start(native, executable, argc, argv);
  allocator_default_free_structs(utf8_t, argc, argv);
  CHECK_TRUE("failed to start process", started);
  opaque_promise_t *exit_code = native_process_exit_code(native);
  process_airlock_t *airlock = get_process_airlock(get_builtin_process(args));
  fulfill_promise_state_t *state = allocator_default_malloc_struct(fulfill_promise_state_t);
  undertaking_init(UPCAST_UNDERTAKING(state), &kFulfillPromiseController);
  runtime_t *runtime = get_builtin_runtime(args);
  state->s_promise = runtime_protect_value(runtime, exit_code_promise);
  state->s_value = protect_immediate(nothing());
  process_airlock_begin_undertaking(airlock, UPCAST_UNDERTAKING(state));
  opaque_promise_on_fulfill(exit_code,
      unary_callback_new_2(on_exit_code_ready_bridge, p2o(airlock),
          p2o(state)),
      omTakeOwnership);
  return null();
}

static value_t os_process_set_stream(builtin_arguments_t *args,
    stdio_stream_t stream, size_t lifeline_offset, pipe_direction_t dir) {
  value_t os_process = get_builtin_subject(args);
  CHECK_FAMILY(ofOsProcess, os_process);
  value_t os_pipe = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofOsPipe, os_pipe);
  native_pipe_t *pipe = get_os_pipe_native(os_pipe);
  native_process_t *process = get_os_process_native(os_process);
  native_process_set_stream(process, stream, stream_redirect_from_pipe(pipe, dir));
  *access_heap_object_field(os_process, lifeline_offset) = os_pipe;
  return null();
}

static value_t os_process_set_stdin(builtin_arguments_t *args) {
  return os_process_set_stream(args, siStdin, kOsProcessStdinLifelineOffset, pdIn);
}

static value_t os_process_set_stdout(builtin_arguments_t *args) {
  return os_process_set_stream(args, siStdout, kOsProcessStdoutLifelineOffset, pdOut);
}

static value_t os_process_set_stderr(builtin_arguments_t *args) {
  return os_process_set_stream(args, siStderr, kOsProcessStderrLifelineOffset, pdOut);
}

value_t add_os_process_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("os_process.start!", 3, os_process_start);
  ADD_BUILTIN_IMPL("os_process.set_stdin!", 1, os_process_set_stdin);
  ADD_BUILTIN_IMPL("os_process.set_stdout!", 1, os_process_set_stdout);
  ADD_BUILTIN_IMPL("os_process.set_stderr!", 1, os_process_set_stderr);
  return success();
}


/// ## Async interface

value_t perform_iop_undertaking_finish(pending_iop_state_t *state,
    value_t process, process_airlock_t *airlock) {
  if (state->iop.type_ == ioRead) {
    value_t result = deref(state->s_result);
    set_blob_data(result, state->scratch);
    TRY(ensure_frozen(airlock->runtime, result));
    fulfill_promise(deref(state->s_promise), result);
  } else {
    size_t written = write_iop_bytes_written((write_iop_t*) &state->iop);
    fulfill_promise(deref(state->s_promise), new_integer(written));
  }
  return success();
}

pending_iop_state_t *pending_iop_state_new(blob_t scratch,
    safe_value_t s_promise, safe_value_t s_stream, safe_value_t s_process,
    safe_value_t s_result, process_airlock_t *airlock) {
  pending_iop_state_t *state = allocator_default_malloc_struct(pending_iop_state_t);
  if (state == NULL)
    return NULL;
  undertaking_init(UPCAST_UNDERTAKING(state), &kPerformIopController);
  state->scratch = scratch;
  state->s_promise = s_promise;
  state->s_stream = s_stream;
  state->s_process = s_process;
  state->s_result = s_result;
  state->airlock = airlock;
  process_airlock_begin_undertaking(airlock, UPCAST_UNDERTAKING(state));
  return state;
}

void perform_iop_undertaking_destroy(runtime_t *runtime, pending_iop_state_t *state) {
  allocator_default_free(state->scratch);
  safe_value_destroy(runtime, state->s_promise);
  safe_value_destroy(runtime, state->s_stream);
  safe_value_destroy(runtime, state->s_process);
  safe_value_destroy(runtime, state->s_result);
  iop_dispose(&state->iop);
  blob_t memory = blob_new(state, sizeof(pending_iop_state_t));
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
    iop_t *next = NULL;
    if (!iop_group_wait_for_next(&engine->iop_group, timeout, &next))
      return;
    pending_iop_state_t *state = (pending_iop_state_t*) o2p(iop_extra(next));
    process_airlock_deliver_undertaking(state->airlock, UPCAST_UNDERTAKING(state));
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
  io_engine_t *engine = allocator_default_malloc_struct(io_engine_t);
  if (engine == NULL)
    return NULL;
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
  blob_t memory = blob_new(engine, sizeof(io_engine_t));
  allocator_default_free(memory);
}

bool io_engine_schedule(io_engine_t *engine, pending_iop_state_t *op) {
  CHECK_FALSE("scheduling while terminating", engine->terminate_when_idle);
  opaque_t opaque_op = p2o(op);
  return worklist_schedule(kIoEngineMaxIncoming, 1)(&engine->incoming,
      &opaque_op, 1, duration_unlimited());
}
