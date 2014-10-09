//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "derived-inl.h"
#include "interp.h"
#include "process.h"
#include "safe-inl.h"
#include "sync.h"
#include "try-inl.h"
#include "utils/log.h"
#include "value-inl.h"


// Cache of various data associated with the code currently being executed.
typedef struct {
  // The raw bytecode.
  blob_t bytecode;
  // The pool of constant values used by the bytecode.
  value_t value_pool;
} code_cache_t;

// Updates the code cache according to the given frame. This must be called each
// time control moves from one frame to another.
static void code_cache_refresh(code_cache_t *cache, frame_t *frame) {
  value_t code_block = frame_get_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  get_blob_data(bytecode, &cache->bytecode);
  cache->value_pool = get_code_block_value_pool(code_block);
}

// Records the current state of the given frame in the given escape state object
// such that restoring from the state will bring the frame back to the state
// it is now, modulo the given pc-offset which will have been added to the
// frame's pc.
static void capture_escape_state(value_t self, frame_t *frame, size_t pc_offset) {
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  escape_state_init(self,
      frame->stack_pointer - stack_start,
      frame->frame_pointer - stack_start,
      frame->limit_pointer - stack_start,
      frame->flags,
      frame->pc + pc_offset);
}

// Restores the previous state of the interpreter from the given derived
// object's escape state.
static void restore_escape_state(frame_t *frame, value_t stack,
    value_t destination) {
  value_t target_piece = get_derived_object_host(destination);
  if (!is_same_value(target_piece, frame->stack_piece)) {
    set_stack_top_piece(stack, target_piece);
    open_stack_piece(target_piece, frame);
  }
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  value_t stack_pointer = get_escape_state_stack_pointer(destination);
  frame->stack_pointer = stack_start + get_integer_value(stack_pointer);
  value_t frame_pointer = get_escape_state_frame_pointer(destination);
  frame->frame_pointer = stack_start + get_integer_value(frame_pointer);
  value_t limit_pointer = get_escape_state_limit_pointer(destination);
  frame->limit_pointer = stack_start + get_integer_value(limit_pointer);
  frame->flags = get_escape_state_flags(destination);
  value_t pc = get_escape_state_pc(destination);
  frame->pc = get_integer_value(pc);
}

// Returns the short value at the given offset from the current pc.
static short_t read_short(code_cache_t *cache, frame_t *frame, size_t offset) {
  return blob_short_at(&cache->bytecode, frame->pc + offset);
}

// Returns the value at the given offset from the current pc.
static value_t read_value(code_cache_t *cache, frame_t *frame, size_t offset) {
  size_t index = read_short(cache, frame, offset);
  return get_array_at(cache->value_pool, index);
}

// Returns the code that implements the given method object.
static value_t compile_method(runtime_t *runtime, value_t method) {
  value_t method_ast = get_method_syntax(method);
  value_t fragment = get_method_module_fragment(method);
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, fragment, scope_get_bottom()));
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(code, compile_method_body(&assm, method_ast));
    E_RETURN(code);
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

// Gets the code from a method object, compiling the method if necessary.
static value_t ensure_method_code(runtime_t *runtime, value_t method) {
  value_t code = get_method_code(method);
  if (is_nothing(code)) {
    TRY_SET(code, compile_method(runtime, method));
    set_method_code(method, code);
  }
  return code;
}

static void log_lookup_error(value_t condition, total_sigmap_input_o *input) {
  size_t arg_count = sigmap_input_get_argument_count(UPCAST(input));
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%v: {", condition);
  for (size_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(&buf, ", ");
    value_t tag = sigmap_input_get_tag_at(UPCAST(input), i);
    value_t value = total_sigmap_input_get_value_at(input, i);
    string_buffer_printf(&buf, "%v: %v", tag, value);
  }
  string_buffer_printf(&buf, "}");
  string_t str;
  string_buffer_flush(&buf, &str);
  ERROR("%s", str.chars);
  string_buffer_dispose(&buf);
}

// Validates that the stack looks correct after execution completes normally.
static void validate_stack_on_normal_exit(frame_t *frame) {
  value_t stack = get_stack_piece_stack(frame->stack_piece);
  CHECK_TRUE("leftover barriers", is_nothing(get_stack_top_barrier(stack)));
}

// Checks whether to fire the next barrier on the way to the given destination.
// If there is a barrier to fire, fire it. Returns false iff a barrier was fired,
// true if we've arrived at the destination.
static bool maybe_fire_next_barrier(code_cache_t *cache, frame_t *frame,
    runtime_t *runtime, value_t stack, value_t destination) {
  CHECK_DOMAIN(vdDerivedObject, destination);
  value_t next_barrier = get_stack_top_barrier(stack);
  if (is_same_value(next_barrier, destination)) {
    // We've arrived.
    return true;
  }
  // Grab the next barrier's handler.
  value_t payload = get_barrier_state_payload(next_barrier);
  value_t previous = get_barrier_state_previous(next_barrier);
  // Unhook the barrier from the barrier stack.
  set_stack_top_barrier(stack, previous);
  // Fire the exit action for the handler object.
  if (in_genus(dgEnsureSection, next_barrier)) {
    // Pop any previous state off the stack. If we've executed any
    // code shards before the first will be the result from the shard
    // the second will be the shard itself.
    frame_pop_value(frame);
    frame_pop_value(frame);
    // Push the shard onto the stack as the subject since we may need it
    // to refract access to outer variables.
    frame_push_value(frame, next_barrier);
    value_t argmap = ROOT(runtime, array_of_zero);
    value_t code_block = payload;
    push_stack_frame(runtime, stack, frame,
        get_code_block_high_water_mark(code_block), argmap);
    frame_set_code_block(frame, code_block);
    code_cache_refresh(cache, frame);
  } else {
    on_derived_object_exit(next_barrier);
  }
  return false;
}

// Counter that increments for each opcode executed when interpreter topic
// logging is enabled. Can be helpful for debugging but is kind of a lame hack.
static uint64_t opcode_counter = 0;

// Counter that is used to schedule validation interrupts in expensive checks
// mode.
static uint64_t interrupt_counter = 0;

// Interval between forced validations. Must be a power of 2.
#define kForceValidateInterval 2048

// Expands to a block that checks whether it's time to force validation.
#define MAYBE_INTERRUPT() do {                                                 \
  if ((++interrupt_counter & (kForceValidateInterval - 1)) == 0) {             \
    size_t serial = interrupt_counter / kForceValidateInterval;                \
    E_RETURN(new_force_validate_condition(serial));                            \
  }                                                                            \
} while (false)


// Runs the given task within the given ambience until a condition is
// encountered or evaluation completes. This function also bails on and leaves
// it to the surrounding code to report error messages.
static value_t run_task_pushing_signals(value_t ambience, value_t task) {
  CHECK_FAMILY(ofAmbience, ambience);
  CHECK_FAMILY(ofTask, task);
  value_t process = get_task_process(task);
  value_t stack = get_task_stack(task);
  runtime_t *runtime = get_ambience_runtime(ambience);
  frame_t frame = open_stack(stack);
  code_cache_t cache;
  code_cache_refresh(&cache, &frame);
  E_BEGIN_TRY_FINALLY();
    while (true) {
      opcode_t opcode = (opcode_t) read_short(&cache, &frame, 0);
      TOPIC_INFO(Interpreter, "Opcode: %s (%i)", get_opcode_name(opcode),
          ++opcode_counter);
      IF_EXPENSIVE_CHECKS_ENABLED(MAYBE_INTERRUPT());
      switch (opcode) {
        case ocPush: {
          value_t value = read_value(&cache, &frame, 1);
          frame_push_value(&frame, value);
          frame.pc += kPushOperationSize;
          break;
        }
        case ocPop: {
          size_t count = read_short(&cache, &frame, 1);
          for (size_t i = 0; i < count; i++)
            frame_pop_value(&frame);
          frame.pc += kPopOperationSize;
          break;
        }
        case ocCheckStackHeight: {
          size_t expected = read_short(&cache, &frame, 1);
          size_t height = frame.stack_pointer - frame.frame_pointer;
          CHECK_EQ("stack height", expected, height);
          frame.pc += kCheckStackHeightOperationSize;
          break;
        }
        case ocNewArray: {
          size_t length = read_short(&cache, &frame, 1);
          E_TRY_DEF(array, new_heap_array(runtime, length));
          for (size_t i = 0; i < length; i++) {
            value_t element = frame_pop_value(&frame);
            set_array_at(array, length - i - 1, element);
          }
          frame_push_value(&frame, array);
          frame.pc += kNewArrayOperationSize;
          break;
        }
        case ocInvoke: {
          // Look up the method in the method space.
          value_t tags = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofCallTags, tags);
          value_t fragment = read_value(&cache, &frame, 2);
          CHECK_FAMILY_OPT(ofModuleFragment, fragment);
          value_t arg_map;
          frame_sigmap_input_o input = frame_sigmap_input_new(ambience, tags,
              &frame);
          value_t method = lookup_method_full(UPCAST(&input), fragment,
              &arg_map);
          if (in_condition_cause(ccLookupError, method)) {
            log_lookup_error(method, UPCAST(&input));
            E_RETURN(method);
          }
          // The lookup may have failed with a different condition. Check for that.
          E_TRY(method);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          // We should now have done everything that can fail so we advance the
          // pc over this instruction. In reality we haven't, the frame push op
          // below can fail so we should really push the next frame before
          // storing the pc for this one. Laters.
          frame.pc += kInvokeOperationSize;
          // Push a new activation.
          E_TRY(push_stack_frame(runtime, stack, &frame,
              get_code_block_high_water_mark(code_block), arg_map));
          frame_set_code_block(&frame, code_block);
          code_cache_refresh(&cache, &frame);
          break;
        }
        case ocSignalContinue: case ocSignalEscape: {
          // Look up the method in the method space.
          value_t tags = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofCallTags, tags);
          frame.pc += kSignalEscapeOperationSize;
          value_t arg_map = whatever();
          value_t handler = whatever();
          frame_sigmap_input_o input = frame_sigmap_input_new(ambience, tags, &frame);
          value_t method = lookup_signal_handler_method(UPCAST(UPCAST(&input)),
              &frame, &handler, &arg_map);
          bool is_escape = (opcode == ocSignalEscape);
          if (in_condition_cause(ccLookupError, method)) {
            if (is_escape) {
              // There was no handler for this so we have to escape out of the
              // interpreter altogether. Push the signal frame onto the stack to
              // record the state of it for the enclosing code.
              E_TRY(push_stack_frame(runtime, stack, &frame, 1, nothing()));
              // The stack tracing code expects all frames to have a valid code block
              // object. The rest makes less of a difference.
              frame_set_code_block(&frame, ROOT(runtime, empty_code_block));
              E_RETURN(new_signal_condition(is_escape));
            } else {
              // There was no handler but this is not an escape so we skip over
              // the post-handler goto to the default block.
              CHECK_EQ("signal not followed by goto", ocGoto,
                  read_short(&cache, &frame, 0));
              frame.pc += kGotoOperationSize;
            }
          } else {
            // We found a method. Invoke it.
            E_TRY(method);
            E_TRY_DEF(code_block, ensure_method_code(runtime, method));
            E_TRY(push_stack_frame(runtime, stack, &frame,
                get_code_block_high_water_mark(code_block), arg_map));
            frame_set_code_block(&frame, code_block);
            CHECK_TRUE("subject not null", is_null(frame_get_argument(&frame, 0)));
            frame_set_argument(&frame, 0, handler);
            code_cache_refresh(&cache, &frame);
          }
          break;
        }
        case ocGoto: {
          size_t delta = read_short(&cache, &frame, 1);
          frame.pc += delta;
          break;
        }
        case ocDelegateToLambda:
        case ocDelegateToBlock: {
          // This op only appears in the lambda and block delegator methods.
          // They should never be executed because the delegation happens during
          // method lookup. If we hit here something's likely wrong with the
          // lookup process.
          UNREACHABLE("delegate to lambda");
          return new_condition(ccWat);
        }
        case ocBuiltin: {
          value_t wrapper = read_value(&cache, &frame, 1);
          builtin_method_t impl = (builtin_method_t) get_void_p_value(wrapper);
          builtin_arguments_t args;
          builtin_arguments_init(&args, runtime, &frame, process);
          E_TRY_DEF(result, impl(&args));
          frame_push_value(&frame, result);
          frame.pc += kBuiltinOperationSize;
          break;
        }
        case ocBuiltinMaybeEscape: {
          value_t wrapper = read_value(&cache, &frame, 1);
          builtin_method_t impl = (builtin_method_t) get_void_p_value(wrapper);
          builtin_arguments_t args;
          builtin_arguments_init(&args, runtime, &frame, process);
          value_t result = impl(&args);
          if (in_condition_cause(ccSignal, result)) {
            // The builtin failed. Find the appropriate signal handler and call
            // it. The invocation record is at the top of the stack.
            value_t tags = frame_pop_value(&frame);
            CHECK_FAMILY(ofCallTags, tags);
            value_t arg_map = whatever();
            value_t handler = whatever();
            frame_sigmap_input_o input = frame_sigmap_input_new(ambience, tags,
                &frame);
            value_t method = lookup_signal_handler_method(UPCAST(UPCAST(&input)),
                &frame, &handler, &arg_map);
            if (in_condition_cause(ccLookupError, method)) {
              // Push the record back onto the stack to it's available to back
              // tracing.
              frame_push_value(&frame, tags);
              frame.pc += kBuiltinMaybeEscapeOperationSize;
              // There was no handler for this so we have to escape out of the
              // interpreter altogether. Push the signal frame onto the stack to
              // record the state of it for the enclosing code.
              E_TRY(push_stack_frame(runtime, stack, &frame, 1, nothing()));
              // The stack tracing code expects all frames to have a valid code block
              // object. The rest makes less of a difference.
              frame_set_code_block(&frame, ROOT(runtime, empty_code_block));
              E_RETURN(new_signal_condition(true));
            }
            // Either found a signal or encountered a different condition.
            E_TRY(method);
            // Skip forward to the point we want the signal to return to, the
            // leave-or-fire-barrier op that will do the leaving.
            size_t dest_offset = read_short(&cache, &frame, 2);
            frame.pc += dest_offset;
            // Run the handler.
            E_TRY_DEF(code_block, ensure_method_code(runtime, method));
            E_TRY(push_stack_frame(runtime, stack, &frame,
                get_code_block_high_water_mark(code_block), arg_map));
            frame_set_code_block(&frame, code_block);
            CHECK_TRUE("subject not null", is_null(frame_get_argument(&frame, 0)));
            frame_set_argument(&frame, 0, handler);
            code_cache_refresh(&cache, &frame);
          } else {
            // The builtin didn't cause a condition so we can just keep going.
            E_TRY(result);
            frame_push_value(&frame, result);
            frame.pc += kBuiltinMaybeEscapeOperationSize;
          }
          break;
        }
        case ocReturn: {
          value_t result = frame_pop_value(&frame);
          frame_pop_within_stack_piece(&frame);
          code_cache_refresh(&cache, &frame);
          frame_push_value(&frame, result);
          break;
        }
        case ocStackBottom: {
          value_t result = frame_pop_value(&frame);
          validate_stack_on_normal_exit(&frame);
          E_RETURN(result);
        }
        case ocStackPieceBottom: {
          value_t top_piece = frame.stack_piece;
          value_t result = frame_pop_value(&frame);
          value_t next_piece = get_stack_piece_previous(top_piece);
          set_stack_top_piece(stack, next_piece);
          frame = open_stack(stack);
          code_cache_refresh(&cache, &frame);
          frame_push_value(&frame, result);
          break;
        }
        case ocSlap: {
          value_t value = frame_pop_value(&frame);
          size_t argc = read_short(&cache, &frame, 1);
          for (size_t i = 0; i < argc; i++)
            frame_pop_value(&frame);
          frame_push_value(&frame, value);
          frame.pc += kSlapOperationSize;
          break;
        }
        case ocNewReference: {
          // Create the reference first so that if it fails we haven't clobbered
          // the stack yet.
          E_TRY_DEF(ref, new_heap_reference(runtime, nothing()));
          value_t value = frame_pop_value(&frame);
          set_reference_value(ref, value);
          frame_push_value(&frame, ref);
          frame.pc += kNewReferenceOperationSize;
          break;
        }
        case ocSetReference: {
          value_t ref = frame_pop_value(&frame);
          CHECK_FAMILY(ofReference, ref);
          value_t value = frame_peek_value(&frame, 0);
          set_reference_value(ref, value);
          frame.pc += kSetReferenceOperationSize;
          break;
        }
        case ocGetReference: {
          value_t ref = frame_pop_value(&frame);
          CHECK_FAMILY(ofReference, ref);
          value_t value = get_reference_value(ref);
          frame_push_value(&frame, value);
          frame.pc += kGetReferenceOperationSize;
          break;
        }
        case ocLoadLocal: {
          size_t index = read_short(&cache, &frame, 1);
          value_t value = frame_get_local(&frame, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadLocalOperationSize;
          break;
        }
        case ocLoadGlobal: {
          value_t path = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofPath, path);
          value_t fragment = read_value(&cache, &frame, 2);
          CHECK_FAMILY_OPT(ofModuleFragment, fragment);
          E_TRY_DEF(value, module_fragment_lookup_path_full(runtime, fragment, path));
          frame_push_value(&frame, value);
          frame.pc += kLoadGlobalOperationSize;
          break;
        }
        case ocLoadArgument: {
          size_t param_index = read_short(&cache, &frame, 1);
          value_t value = frame_get_argument(&frame, param_index);
          frame_push_value(&frame, value);
          frame.pc += kLoadArgumentOperationSize;
          break;
        }
        case ocLoadRawArgument: {
          size_t eval_index = read_short(&cache, &frame, 1);
          value_t value = frame_get_raw_argument(&frame, eval_index);
          frame_push_value(&frame, value);
          frame.pc += kLoadRawArgumentOperationSize;
          break;
        }
        case ocLoadRefractedArgument: {
          size_t param_index = read_short(&cache, &frame, 1);
          size_t block_depth = read_short(&cache, &frame, 2);
          value_t subject = frame_get_argument(&frame, 0);
          frame_t home = frame_empty();
          get_refractor_refracted_frame(subject, block_depth, &home);
          value_t value = frame_get_argument(&home, param_index);
          frame_push_value(&frame, value);
          frame.pc += kLoadRefractedArgumentOperationSize;
          break;
        }
        case ocLoadRefractedLocal: {
          size_t index = read_short(&cache, &frame, 1);
          size_t block_depth = read_short(&cache, &frame, 2);
          value_t subject = frame_get_argument(&frame, 0);
          frame_t home = frame_empty();
          get_refractor_refracted_frame(subject, block_depth, &home);
          value_t value = frame_get_local(&home, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadRefractedLocalOperationSize;
          break;
        }
        case ocLoadLambdaCapture: {
          size_t index = read_short(&cache, &frame, 1);
          value_t subject = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofLambda, subject);
          value_t value = get_lambda_capture(subject, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadLambdaCaptureOperationSize;
          break;
        }
        case ocLoadRefractedCapture: {
          size_t index = read_short(&cache, &frame, 1);
          size_t block_depth = read_short(&cache, &frame, 2);
          value_t subject = frame_get_argument(&frame, 0);
          frame_t home = frame_empty();
          get_refractor_refracted_frame(subject, block_depth, &home);
          value_t lambda = frame_get_argument(&home, 0);
          CHECK_FAMILY(ofLambda, lambda);
          value_t value = get_lambda_capture(lambda, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadRefractedLocalOperationSize;
          break;
        }
        case ocLambda: {
          value_t space = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofMethodspace, space);
          size_t capture_count = read_short(&cache, &frame, 2);
          value_t captures;
          E_TRY_DEF(lambda, new_heap_lambda(runtime, space, nothing()));
          if (capture_count == 0) {
            captures = ROOT(runtime, empty_array);
            frame.pc += kLambdaOperationSize;
          } else {
            E_TRY_SET(captures, new_heap_array(runtime, capture_count));
            // The pc gets incremented here because it is after we've done all
            // the allocation but before anything has been popped off the stack.
            // This way all the above is idempotent, and the below is guaranteed
            // to succeed.
            frame.pc += kLambdaOperationSize;
            for (size_t i = 0; i < capture_count; i++)
              set_array_at(captures, i, frame_pop_value(&frame));
          }
          set_lambda_captures(lambda, captures);
          frame_push_value(&frame, lambda);
          break;
        }
        case ocCreateBlock: {
          value_t space = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofMethodspace, space);
          // Create the block object.
          E_TRY_DEF(block, new_heap_block(runtime, nothing()));
          // Create the stack section that describes the block.
          value_t section = frame_alloc_derived_object(&frame, get_genus_descriptor(dgBlockSection));
          set_barrier_state_payload(section, block);
          refraction_point_init(section, &frame);
          set_block_section_methodspace(section, space);
          set_block_section(block, section);
          value_validate(block);
          value_validate(section);
          // Push the block object.
          frame_push_value(&frame, block);
          frame.pc += kCreateBlockOperationSize;
          break;
        }
        case ocCreateEnsurer: {
          value_t code_block = read_value(&cache, &frame, 1);
          value_t section = frame_alloc_derived_object(&frame,
              get_genus_descriptor(dgEnsureSection));
          set_barrier_state_payload(section, code_block);
          refraction_point_init(section, &frame);
          value_validate(section);
          frame_push_value(&frame, section);
          frame.pc += kCreateEnsurerOperationSize;
          break;
        }
        case ocCallEnsurer: {
          value_t value = frame_pop_value(&frame);
          value_t shard = frame_pop_value(&frame);
          frame_push_value(&frame, value);
          frame_push_value(&frame, shard);
          CHECK_GENUS(dgEnsureSection, shard);
          value_t code_block = get_barrier_state_payload(shard);
          CHECK_FAMILY(ofCodeBlock, code_block);
          // Unregister the barrier before calling it, otherwise if we leave
          // by escaping we'll end up calling it over again.
          barrier_state_unregister(shard, stack);
          frame.pc += kCallEnsurerOperationSize;
          value_t argmap = ROOT(runtime, array_of_zero);
          push_stack_frame(runtime, stack, &frame,
              get_code_block_high_water_mark(code_block), argmap);
          frame_set_code_block(&frame, code_block);
          code_cache_refresh(&cache, &frame);
          break;
        }
        case ocDisposeEnsurer: {
          // Discard the result of the ensure block. If an ensure blocks needs
          // to return a useful value it can do it via an escape.
          frame_pop_value(&frame);
          value_t shard = frame_pop_value(&frame);
          CHECK_GENUS(dgEnsureSection, shard);
          value_t value = frame_pop_value(&frame);
          frame_destroy_derived_object(&frame, get_genus_descriptor(dgEnsureSection));
          frame_push_value(&frame, value);
          frame.pc += kDisposeEnsurerOperationSize;
          break;
        }
        case ocInstallSignalHandler: {
          value_t space = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofMethodspace, space);
          size_t dest_offset = read_short(&cache, &frame, 2);
          // Allocate the derived object that's going to hold the signal handler
          // state.
          value_t section = frame_alloc_derived_object(&frame,
              get_genus_descriptor(dgSignalHandlerSection));
          // Initialize the handler.
          set_barrier_state_payload(section, space);
          refraction_point_init(section, &frame);
          // Bring the frame state to the point we'll want to escape to (modulo
          // the destination offset).
          frame_push_value(&frame, section);
          frame.pc += kInstallSignalHandlerOperationSize;
          // Finally capture the escape state.
          capture_escape_state(section, &frame, dest_offset);
          value_validate(section);
          break;
        }
        case ocUninstallSignalHandler: {
          // The result has been left at the top of the stack.
          value_t value = frame_pop_value(&frame);
          value_t section = frame_pop_value(&frame);
          CHECK_GENUS(dgSignalHandlerSection, section);
          barrier_state_unregister(section, stack);
          frame_destroy_derived_object(&frame, get_genus_descriptor(dgSignalHandlerSection));
          frame_push_value(&frame, value);
          frame.pc += kUninstallSignalHandlerOperationSize;
          break;
        }
        case ocCreateEscape: {
          size_t dest_offset = read_short(&cache, &frame, 1);
          // Create an initially empty escape object.
          E_TRY_DEF(escape, new_heap_escape(runtime, nothing()));
          // Allocate the escape section on the stack, hooking the barrier into
          // the barrier chain.
          value_t section = frame_alloc_derived_object(&frame, get_genus_descriptor(dgEscapeSection));
          // Point the state and object to each other.
          set_barrier_state_payload(section, escape);
          set_escape_section(escape, section);
          // Get execution ready for the next operation.
          frame_push_value(&frame, escape);
          frame.pc += kCreateEscapeOperationSize;
          // This is the execution state the escape will escape to (modulo the
          // destination offset) so this is what we want to capture.
          capture_escape_state(section, &frame,
              dest_offset);
          break;
        }
        case ocLeaveOrFireBarrier: {
          size_t argc = read_short(&cache, &frame, 1);
          // At this point the handler has been set as the subject of the call
          // to the handler method. Above the arguments are also two scratch
          // stack entries.
          value_t handler = frame_peek_value(&frame, argc + 2);
          CHECK_GENUS(dgSignalHandlerSection, handler);
          if (maybe_fire_next_barrier(&cache, &frame, runtime, stack, handler)) {
            // Pop the scratch entries off.
            frame_pop_value(&frame);
            frame_pop_value(&frame);
            // Pop the value off.
            value_t value = frame_pop_value(&frame);
            // Escape to the handler's home.
            restore_escape_state(&frame, stack, handler);
            code_cache_refresh(&cache, &frame);
            // Push the value back on, now in the handler's home frame.
            frame_push_value(&frame, value);
          } else {
            // If a barrier was fired we'll want to let the interpreter loop
            // around again so just break without touching .pc.
          }
          break;
        }
        case ocFireEscapeOrBarrier: {
          value_t escape = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofEscape, escape);
          value_t section = get_escape_section(escape);
          // Fire the next barrier or, if there are no more barriers, apply the
          // escape.
          if (maybe_fire_next_barrier(&cache, &frame, runtime, stack, section)) {
            value_t value = frame_get_argument(&frame, 2);
            restore_escape_state(&frame, stack, section);
            code_cache_refresh(&cache, &frame);
            frame_push_value(&frame, value);
          } else {
            // If a barrier was fired we'll want to let the interpreter loop
            // around again so just break without touching .pc.
          }
          break;
        }
        case ocDisposeEscape: {
          value_t value = frame_pop_value(&frame);
          value_t escape = frame_pop_value(&frame);
          CHECK_FAMILY(ofEscape, escape);
          value_t section = get_escape_section(escape);
          value_validate(section);
          barrier_state_unregister(section, stack);
          on_escape_section_exit(section);
          frame_destroy_derived_object(&frame, get_genus_descriptor(dgEscapeSection));
          frame_push_value(&frame, value);
          frame.pc += kDisposeEscapeOperationSize;
          break;
        }
        case ocDisposeBlock: {
          value_t value = frame_pop_value(&frame);
          value_t block = frame_pop_value(&frame);
          CHECK_FAMILY(ofBlock, block);
          value_t section = get_block_section(block);
          barrier_state_unregister(section, stack);
          on_block_section_exit(section);
          frame_destroy_derived_object(&frame, get_genus_descriptor(dgBlockSection));
          frame_push_value(&frame, value);
          frame.pc += kDisposeBlockOperationSize;
          break;
        }
        case ocCreateCallData: {
          size_t argc = read_short(&cache, &frame, 1);
          TRY_DEF(raw_tags, new_heap_array(runtime, argc));
          for (size_t i = 0; i < argc; i++) {
            value_t tag = frame_peek_value(&frame, 2 * (argc - i) - 1);
            set_array_at(raw_tags, i, tag);
          }
          TRY_DEF(entries, build_call_tags_entries(runtime, raw_tags));
          TRY_DEF(call_tags, new_heap_call_tags(runtime, afFreeze, entries));
          value_t values = raw_tags;
          for (size_t i = 0; i < argc; i++) {
            value_t value = frame_pop_value(&frame);
            frame_pop_value(&frame);
            set_array_at(values, i, value);
          }
          TRY_DEF(call_data, new_heap_call_data(runtime, call_tags, values));
          frame_push_value(&frame, call_data);
          frame.pc += kCreateCallDataOperationSize;
          break;
        }
        case ocModuleFragmentPrivateInvoke: {
          // Perform the method lookup.
          value_t phrivate = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofModuleFragmentPrivate, phrivate);
          value_t fragment = get_module_fragment_private_owner(phrivate);
          value_t call_data = frame_get_argument(&frame, 2);
          CHECK_FAMILY(ofCallData, call_data);
          value_t arg_map;
          call_data_sigmap_input_o input = call_data_sigmap_input_new(ambience,
              call_data);
          value_t method = lookup_method_full(UPCAST(&input), fragment,
              &arg_map);
          if (in_condition_cause(ccLookupError, method)) {
            log_lookup_error(method, UPCAST(&input));
            E_RETURN(method);
          }
          E_TRY(method);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          frame.pc += kModuleFragmentPrivateInvokeOperationSize;
          // Method lookup succeeded. Build the frame that holds the arguments.
          value_t values = get_call_data_values(call_data);
          size_t argc = get_array_length(values);
          // The argument frame needs room for all the arguments as well as
          // the return value.
          E_TRY(push_stack_frame(runtime, stack, &frame, argc + 1, nothing()));
          frame_set_code_block(&frame, ROOT(runtime, return_code_block));
          for (size_t i = 0; i < argc; i++)
            frame_push_value(&frame, get_array_at(values, argc - i - 1));
          // Then build the method's frame.
          value_t pushed = push_stack_frame(runtime, stack, &frame,
              get_code_block_high_water_mark(code_block), arg_map);
          // This should be handled gracefully.
          CHECK_FALSE("call literal invocation failed", is_condition(pushed));
          frame_set_code_block(&frame, code_block);
          code_cache_refresh(&cache, &frame);
          break;
        }
        default:
          ERROR("Unexpected opcode %i", opcode);
          UNREACHABLE("unexpected opcode");
          break;
      }
    }
  E_FINALLY();
    close_frame(&frame);
  E_END_TRY_FINALLY();
}

// Runs the given stack until it hits a condition or completes successfully.
static value_t run_task_until_condition(value_t ambience, value_t task) {
  CHECK_FAMILY(ofAmbience, ambience);
  CHECK_FAMILY(ofTask, task);
  value_t result = run_task_pushing_signals(ambience, task);
  if (in_condition_cause(ccSignal, result)) {
    runtime_t *runtime = get_ambience_runtime(ambience);
    frame_t frame = open_stack(get_task_stack(task));
    TRY_DEF(trace, capture_backtrace(runtime, &frame));
    print_ln("%9v", trace);
  }
  return result;
}

// Runs the given stack until it hits a signal or completes successfully. If the
// heap becomes exhausted this function will try garbage collecting and
// continuing.
static value_t run_task_until_signal(safe_value_t s_ambience, safe_value_t s_task) {
  CHECK_FAMILY(ofAmbience, deref(s_ambience));
  CHECK_FAMILY(ofTask, deref(s_task));
  loop: do {
    value_t ambience = deref(s_ambience);
    value_t task = deref(s_task);
    value_t result = run_task_until_condition(ambience, task);
    if (in_condition_cause(ccHeapExhausted, result)) {
      runtime_t *runtime = get_ambience_runtime(ambience);
      runtime_garbage_collect(runtime);
      goto loop;
    } else if (in_condition_cause(ccForceValidate, result)) {
      runtime_t *runtime = get_ambience_runtime(ambience);
      runtime_validate(runtime, result);
      goto loop;
    }
    return result;
  } while (false);
}

const char *get_opcode_name(opcode_t opcode) {
  switch (opcode) {
#define __EMIT_CASE__(Name, ARGC)                                              \
    case oc##Name:                                                             \
      return #Name;
  ENUM_OPCODES(__EMIT_CASE__)
#undef __EMIT_CASE__
    default:
      return NULL;
  }
}

value_t run_code_block_until_condition(value_t ambience, value_t code) {
  CHECK_FAMILY(ofAmbience, ambience);
  CHECK_FAMILY(ofCodeBlock, code);
  // Create the stack to run the code on.
  runtime_t *runtime = get_ambience_runtime(ambience);
  TRY_DEF(process, new_heap_process(runtime));
  TRY_DEF(task, get_process_root_task(process));
  TRY_DEF(stack, get_task_stack(task));
  // Push an activation onto the empty stack to get execution going.
  size_t frame_size = get_code_block_high_water_mark(code);
  frame_t frame = open_stack(stack);
  TRY(push_stack_frame(runtime, stack, &frame, frame_size, ROOT(runtime, empty_array)));
  frame_set_code_block(&frame, code);
  close_frame(&frame);
  // Run the stack.
  loop: do {
    value_t result = run_task_until_condition(ambience, task);
    if (in_condition_cause(ccForceValidate, result)) {
      runtime_t *runtime = get_ambience_runtime(ambience);
      runtime_validate(runtime, result);
      goto loop;
    }
    return result;
  } while (false);
}

static value_t prepare_run_job(runtime_t *runtime, value_t stack, job_t *job) {
  frame_t frame = open_stack(stack);
  // Set up the frame containing the argument. The code frame returns to
  // this and then this returns by itself so at the end, if the job is
  // successful, we're back to an empty stack.
  TRY(push_stack_frame(runtime, stack, &frame, 2, ROOT(runtime, empty_array)));
  frame_set_code_block(&frame, ROOT(runtime, return_code_block));
  frame_push_value(&frame, job->data);
  // Set up the frame for running the code.
  size_t frame_size = get_code_block_high_water_mark(job->code);
  TRY(push_stack_frame(runtime, stack, &frame, frame_size,
      ROOT(runtime, empty_array)));
  frame_set_code_block(&frame, job->code);
  close_frame(&frame);
  return success();
}

static value_t resolve_job_promise(value_t result, job_t *job) {
  if (is_nothing(job->promise))
    return success();
  fulfill_promise(job->promise, result);
  return success();
}

// Grabs the next work job from the given process, which must have more work,
// and executes it on the process' main task.
static value_t run_next_process_job(safe_value_t s_ambience, safe_value_t s_process) {
  runtime_t *runtime = get_ambience_runtime(deref(s_ambience));
  job_t job;
  TRY(take_process_job(deref(s_process), &job));
  CREATE_SAFE_VALUE_POOL(runtime, 5, pool);
  E_BEGIN_TRY_FINALLY();
    E_S_TRY_DEF(s_task, protect(pool, get_process_root_task(deref(s_process))));
    E_TRY(prepare_run_job(runtime, get_task_stack(deref(s_task)), &job));
    E_TRY_DEF(result, run_task_until_signal(s_ambience, s_task));
    E_TRY(resolve_job_promise(result, &job));
    E_RETURN(result);
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

static value_t run_process_until_idle(safe_value_t s_ambience, safe_value_t s_process) {
  if (!is_process_idle(deref(s_process))) {
    // There's at least one job to run.
    do {
      TRY_DEF(result, run_next_process_job(s_ambience, s_process));
      if (is_process_idle(deref(s_process)))
        // The last job completed normally and it was the last job; return its
        // value.
        return result;
    } while (true);
  }
  return nothing();
}

value_t run_code_block(safe_value_t s_ambience, safe_value_t s_code) {
  runtime_t *runtime = get_ambience_runtime(deref(s_ambience));
  CREATE_SAFE_VALUE_POOL(runtime, 5, pool);
  E_BEGIN_TRY_FINALLY();
    // Build a process to run the code within.
    E_S_TRY_DEF(s_process, protect(pool, new_heap_process(runtime)));
    job_t job;
    job_init(&job, deref(s_code), null(), nothing(), nothing());
    E_TRY(offer_process_job(runtime, deref(s_process), &job));
    E_RETURN(run_process_until_idle(s_ambience, s_process));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}
