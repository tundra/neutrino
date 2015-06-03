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

ACCESSORS_IMPL(OutStream, out_stream, acInFamilyOpt, ofVoidP, Native, native);
ACCESSORS_IMPL(OutStream, out_stream, acNoCheck, 0, Lifeline, lifeline);

value_t out_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofOutStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_out_stream_native(self));
  return success();
}

value_t new_heap_out_stream(runtime_t *runtime, out_stream_t *out, value_t lifeline) {
  TRY_DEF(native, new_heap_void_p(runtime, out));
  size_t size = kOutStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, out_stream_species)));
  set_out_stream_native(result, native);
  set_out_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

static value_t out_stream_write(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofOutStream, self);
  value_t data = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofUtf8, data);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY_DEF(promise, new_heap_pending_promise(runtime));
  safe_value_t s_promise = runtime_protect_value(runtime, promise);
  pending_iop_state_t *state = pending_iop_state_new(s_promise);
  value_t process = get_builtin_process(args);
  process_airlock_schedule_atomic(get_process_airlock(process),
      UPCAST_TO_PENDING_ATOMIC(state));
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

ACCESSORS_IMPL(InStream, in_stream, acInFamilyOpt, ofVoidP, Native, native);
ACCESSORS_IMPL(InStream, in_stream, acNoCheck, 0, Lifeline, lifeline);

value_t in_stream_validate(value_t self) {
  VALIDATE_FAMILY(ofInStream, self);
  VALIDATE_FAMILY_OPT(ofVoidP, get_in_stream_native(self));
  return success();
}

value_t new_heap_in_stream(runtime_t *runtime, in_stream_t *in, value_t lifeline) {
  TRY_DEF(native, new_heap_void_p(runtime, in));
  size_t size = kInStreamSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, in_stream_species)));
  set_in_stream_native(result, native);
  set_in_stream_lifeline(result, lifeline);
  return post_create_sanity_check(result, size);
}

value_t add_in_stream_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  return success();
}


/// ## Async interface

value_t pending_iop_state_apply_atomic(pending_iop_state_t *state,
    process_airlock_t *airlock) {
  value_t promise = deref(state->s_promise);
  fulfill_promise(promise, yes());
  return success();
}

pending_iop_state_t *pending_iop_state_new(safe_value_t s_promise) {
  memory_block_t memory = allocator_default_malloc(sizeof(pending_iop_state_t));
  if (memory_block_is_empty(memory))
    return NULL;
  pending_iop_state_t *state = (pending_iop_state_t*) memory.memory;
  state->as_pending_atomic.type = paIopComplete;
  state->s_promise = s_promise;
  return state;
}

void pending_iop_state_destroy(runtime_t *runtime, pending_iop_state_t *state) {
  safe_value_destroy(runtime, state->s_promise);
  memory_block_t memory = new_memory_block(state, sizeof(pending_iop_state_t));
  allocator_default_free(memory);
}
