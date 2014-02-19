// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "log.h"
#include "process.h"
#include "safe-inl.h"
#include "try-inl.h"
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

// Pushes the current state of the interpreter onto the stack such that it can
// later be restored by invoking the associated escape. Returns a location value
// that must later be passed to restore_escape_state when restoring the state.
// The pc offset is a delta to add to the current pc in the case there the code
// location to return to is not just the next instruction. The sp_offset is a
// delta to add to the stack pointer to account for any changes that happen
// after pushing the state that must be included when restoring the state.
static size_t push_escape_state(frame_t *frame, size_t pc_offset, int sp_offset) {
  frame_t snapshot = *frame;
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  size_t location = snapshot.stack_pointer - stack_start;
  frame_push_value(frame, new_integer(snapshot.stack_pointer + sp_offset - stack_start));
  frame_push_value(frame, new_integer(snapshot.frame_pointer - stack_start));
  frame_push_value(frame, new_integer(snapshot.limit_pointer - stack_start));
  frame_push_value(frame, snapshot.flags);
  frame_push_value(frame, new_integer(snapshot.pc + pc_offset));
  return location;
}

// Restores the previous state of the interpreter that, when captured, returned
// the given location value.
static void restore_escape_state(frame_t *frame, value_t stack,
    value_t target_piece, size_t location) {
  value_t storage = get_stack_piece_storage(target_piece);
  // Restore the state to the stack/stack piece objects. We need those values
  // to be consistent with the frame so write them there and then update the
  // frame based on those values.
  if (!is_same_value(target_piece, frame->stack_piece)) {
    set_stack_top_piece(stack, target_piece);
    open_stack_piece(target_piece, frame);
  }
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  frame->stack_pointer = stack_start
      + get_integer_value(get_array_at(storage, location));
  frame->frame_pointer = stack_start
      + get_integer_value(get_array_at(storage, location + 1));
  frame->limit_pointer = stack_start
      + get_integer_value(get_array_at(storage, location + 2));
  frame->flags = get_array_at(storage, location + 3);
  frame->pc = get_integer_value(get_array_at(storage, location + 4));
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

// Reads a word from the previous instruction, assuming that we're currently
// at the first word of the next instruction. Size is the size of the
// instruction, offset is the offset of the word we want to read.
static short_t peek_previous_short(code_cache_t *state, frame_t *frame, size_t size,
    size_t offset) {
  return blob_short_at(&state->bytecode, frame->pc - size + offset);
}

// Reads a value pool value from the previous instruction, assuming that we're
// currently at the first word of the next instruction.
static value_t peek_previous_value(code_cache_t *cache, frame_t *frame, size_t size,
    size_t offset) {
  size_t index = peek_previous_short(cache, frame, size, offset);
  return get_array_at(cache->value_pool, index);
}

// Returns the code that implements the given method object.
static value_t compile_method(runtime_t *runtime, value_t method) {
  value_t method_ast = get_method_syntax(method);
  value_t fragment = get_method_module_fragment(method);
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, fragment, scope_lookup_callback_get_bottom()));
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

static void log_lookup_error(value_t condition, value_t record, frame_t *frame) {
  size_t arg_count = get_invocation_record_argument_count(record);
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%v: {", condition);
  for (size_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(&buf, ", ");
    value_t tag = get_invocation_record_tag_at(record, i);
    value_t value = get_invocation_record_argument_at(record, frame, i);
    string_buffer_printf(&buf, "%v: %v", tag, value);
  }
  string_buffer_printf(&buf, "}");
  string_t str;
  string_buffer_flush(&buf, &str);
  ERROR("%s", str.chars);
  string_buffer_dispose(&buf);
}

// Counter that increments for each opcode executed when interpreter topic
// logging is enabled. Can be helpful for debugging but is kind of a lame hack.
static uint64_t opcode_counter = 0;

// Runs the given stack within the given ambience until a condition is
// encountered or evaluation completes. This function also bails on and leaves
// it to the surrounding code to report error messages.
static value_t run_stack_pushing_signals(value_t ambience, value_t stack) {
  CHECK_FAMILY(ofAmbience, ambience);
  CHECK_FAMILY(ofStack, stack);
  runtime_t *runtime = get_ambience_runtime(ambience);
  frame_t frame = open_stack(stack);
  code_cache_t cache;
  code_cache_refresh(&cache, &frame);
  E_BEGIN_TRY_FINALLY();
    while (true) {
      opcode_t opcode = (opcode_t) read_short(&cache, &frame, 0);
      TOPIC_INFO(Interpreter, "Opcode: %s (%i)", get_opcode_name(opcode),
          opcode_counter++);
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
          value_t record = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofInvocationRecord, record);
          value_t fragment = read_value(&cache, &frame, 2);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t helper = read_value(&cache, &frame, 3);
          CHECK_FAMILY(ofSignatureMap, helper);
          value_t arg_map;
          value_t method = lookup_method_full(ambience, fragment, record, &frame,
              helper, &arg_map);
          if (in_condition_cause(ccLookupError, method)) {
            log_lookup_error(method, record, &frame);
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
        case ocSignal: {
          // Look up the method in the method space.
          value_t record = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofInvocationRecord, record);
          value_t fragment = read_value(&cache, &frame, 2);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t helper = read_value(&cache, &frame, 3);
          CHECK_PHYLUM(tpNothing, helper);
          frame.pc += kSignalOperationSize;
          // Push the signal frame onto the stack to record the state of it for
          // the enclosing code.
          E_TRY(push_stack_frame(runtime, stack, &frame, 1, nothing()));
          // The stack tracing code expects all frames to have a valid code block
          // object. The rest makes less of a difference.
          frame_set_code_block(&frame, ROOT(runtime, empty_code_block));
          E_RETURN(new_signal_condition());
        }
        case ocDelegateToLambda: {
          // Get the lambda to delegate to.
          value_t lambda = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofLambda, lambda);
          value_t space = get_lambda_methods(lambda);
          // Pop off the top frame since we're repeating the previous call.
          drop_to_stack_frame(stack, &frame, ffOrganic);
          code_cache_refresh(&cache, &frame);
          // Extract the invocation record from the calling instruction.
          CHECK_EQ("invalid calling instruction", ocInvoke,
              peek_previous_short(&cache, &frame, kInvokeOperationSize, 0));
          value_t record = peek_previous_value(&cache, &frame, kInvokeOperationSize, 1);
          CHECK_FAMILY(ofInvocationRecord, record);
          // From this point we do the same as invoke except using the space from
          // the lambda rather than the space from the invoke instruction.
          value_t arg_map;
          value_t method = lookup_methodspace_method(ambience, space, record,
              &frame, &arg_map);
          if (in_condition_cause(ccLookupError, method)) {
            log_lookup_error(method, record, &frame);
            E_RETURN(method);
          }
          // The lookup may have failed with a different condition. Check for that.
          E_TRY(method);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block),
              arg_map);
          frame_set_code_block(&frame, code_block);
          code_cache_refresh(&cache, &frame);
          break;
        }
        case ocDelegateToBlock: {
          // Get the lambda to delegate to.
          value_t block = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofBlock, block);
          CHECK_TRUE_VALUE("calling dead local lambda", get_block_is_live(block));
          // Locate the place on the stack that holds this block's state.
          value_t *home = get_block_home(block);
          value_t space = home[1];
          CHECK_FAMILY(ofMethodspace, space);
          // Pop off the top frame since we're repeating the previous call.
          drop_to_stack_frame(stack, &frame, ffOrganic);
          code_cache_refresh(&cache, &frame);
          // Extract the invocation record from the calling instruction.
          CHECK_EQ("invalid calling instruction", ocInvoke,
              peek_previous_short(&cache, &frame, kInvokeOperationSize, 0));
          value_t record = peek_previous_value(&cache, &frame, kInvokeOperationSize, 1);
          CHECK_FAMILY(ofInvocationRecord, record);
          // From this point we do the same as invoke except using the space from
          // the lambda rather than the space from the invoke instruction.
          value_t arg_map;
          value_t method = lookup_methodspace_method(ambience, space, record,
              &frame, &arg_map);
          if (in_condition_cause(ccLookupError, method)) {
            log_lookup_error(method, record, &frame);
            E_RETURN(method);
          }
          // The lookup may have failed with a different condition. Check for that.
          E_TRY(method);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block),
              arg_map);
          frame_set_code_block(&frame, code_block);
          code_cache_refresh(&cache, &frame);
          break;
        }
        case ocBuiltin: {
          value_t wrapper = read_value(&cache, &frame, 1);
          builtin_method_t impl = (builtin_method_t) get_void_p_value(wrapper);
          builtin_arguments_t args;
          builtin_arguments_init(&args, runtime, &frame);
          E_TRY_DEF(result, impl(&args));
          frame_push_value(&frame, result);
          frame.pc += kBuiltinOperationSize;
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
          E_RETURN(result);
        }
        case ocStackPieceBottom: {
          value_t result = frame_pop_value(&frame);
          value_t top_piece = get_stack_top_piece(stack);
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
          value_t ident = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofIdentifier, ident);
          value_t fragment = read_value(&cache, &frame, 2);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t module = get_module_fragment_module(fragment);
          E_TRY_DEF(value, module_lookup_identifier(runtime, module,
              get_identifier_stage(ident), get_identifier_path(ident)));
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
        case ocLoadOuterArgument: {
          size_t param_index = read_short(&cache, &frame, 1);
          size_t block_depth = read_short(&cache, &frame, 2);
          value_t subject = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofBlock, subject);
          frame_t home;
          get_block_incomplete_outer_frame(subject, block_depth, &home);
          value_t value = frame_get_argument(&home, param_index);
          frame_push_value(&frame, value);
          frame.pc += kLoadOuterArgumentOperationSize;
          break;
        }
        case ocLoadLambdaOuter: {
          size_t index = read_short(&cache, &frame, 1);
          value_t subject = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofLambda, subject);
          value_t value = get_lambda_outer(subject, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadLambdaOuterOperationSize;
          break;
        }
        case ocLoadBlockOuter: {
          size_t index = read_short(&cache, &frame, 1);
          value_t subject = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofBlock, subject);
          value_t value = get_block_outer(subject, index);
          frame_push_value(&frame, value);
          frame.pc += kLoadBlockOuterOperationSize;
          break;
        }
        case ocLambda: {
          value_t space = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofMethodspace, space);
          size_t outer_count = read_short(&cache, &frame, 2);
          value_t outers;
          E_TRY_DEF(lambda, new_heap_lambda(runtime, space, nothing()));
          if (outer_count == 0) {
            outers = ROOT(runtime, empty_array);
            frame.pc += kLambdaOperationSize;
          } else {
            E_TRY_SET(outers, new_heap_array(runtime, outer_count));
            // The pc gets incremented here because it is after we've done all
            // the allocation but before anything has been popped off the stack.
            // This way all the above is idempotent, and the below is guaranteed
            // to succeed.
            frame.pc += kLambdaOperationSize;
            for (size_t i = 0; i < outer_count; i++)
              set_array_at(outers, i, frame_pop_value(&frame));
          }
          set_lambda_outers(lambda, outers);
          frame_push_value(&frame, lambda);
          break;
        }
        case ocBlock: {
          value_t space = read_value(&cache, &frame, 1);
          CHECK_FAMILY(ofMethodspace, space);
          size_t outer_count = read_short(&cache, &frame, 2);
          value_t outers;
          E_TRY_DEF(block, new_heap_block(runtime, yes(), frame.stack_piece,
              nothing()));
          if (outer_count == 0) {
            outers = ROOT(runtime, empty_array);
            frame.pc += kBlockOperationSize;
          } else {
            E_TRY_SET(outers, new_heap_array(runtime, outer_count));
            // The pc gets incremented here because it is after we've done all
            // the allocation but before anything has been popped off the stack.
            // This way all the above is idempotent, and the below is guaranteed
            // to succeed.
            frame.pc += kBlockOperationSize;
            for (size_t i = 0; i < outer_count; i++)
              set_array_at(outers, i, frame_pop_value(&frame));
          }
          value_t *state_pointer = frame.stack_pointer;
          value_t *stack_bottom = frame_get_stack_piece_bottom(&frame);
          set_block_home_state_pointer(block, new_integer(state_pointer - stack_bottom));
          frame_push_value(&frame, block);
          frame_push_value(&frame, space);
          frame_push_value(&frame, outers);
          frame_push_value(&frame, new_integer(frame.frame_pointer - stack_bottom));
          break;
        }
        case ocCaptureEscape: {
          size_t dest_offset = read_short(&cache, &frame, 1);
          // Push the interpreter state at the end of this instruction onto the
          // stack.
          value_t stack_piece = frame.stack_piece;
          E_TRY_DEF(escape, new_heap_escape(runtime, yes(), stack_piece, nothing()));
          size_t location = push_escape_state(&frame,
              dest_offset + kCaptureEscapeOperationSize, kCapturedStateSize + 1);
          set_escape_stack_pointer(escape, new_integer(location));
          frame_push_value(&frame, escape);
          frame.pc += kCaptureEscapeOperationSize;
          break;
        }
        case ocFireEscape: {
          value_t escape = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofEscape, escape);
          value_t value = frame_get_argument(&frame, 2);
          size_t location = get_integer_value(get_escape_stack_pointer(escape));
          value_t stack_piece = get_escape_stack_piece(escape);
          restore_escape_state(&frame, stack, stack_piece, location);
          code_cache_refresh(&cache, &frame);
          frame_push_value(&frame, value);
          break;
        }
        case ocKillEscape: {
          value_t value = frame_pop_value(&frame);
          value_t escape = frame_pop_value(&frame);
          CHECK_FAMILY(ofEscape, escape);
          set_escape_is_live(escape, no());
          frame_push_value(&frame, value);
          frame.pc += kKillEscapeOperationSize;
          break;
        }
        case ocKillBlock: {
          value_t value = frame_pop_value(&frame);
          value_t fp = frame_pop_value(&frame);
          CHECK_DOMAIN(vdInteger, fp);
          value_t outers = frame_pop_value(&frame);
          CHECK_FAMILY(ofArray, outers);
          value_t space = frame_pop_value(&frame);
          CHECK_FAMILY(ofMethodspace, space);
          value_t block = frame_pop_value(&frame);
          CHECK_FAMILY(ofBlock, block);
          set_block_is_live(block, no());
          frame_push_value(&frame, value);
          frame.pc += kKillBlockOperationSize;
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
static value_t run_stack_until_condition(value_t ambience, value_t stack) {
  value_t result = run_stack_pushing_signals(ambience, stack);
  if (in_condition_cause(ccSignal, result)) {
    runtime_t *runtime = get_ambience_runtime(ambience);
    frame_t frame = open_stack(stack);
    TRY_DEF(trace, capture_backtrace(runtime, &frame));
    print_ln("%9v", trace);
  }
  return result;
}

// Runs the given stack until it hits a signal or completes successfully. If the
// heap becomes exhausted this function will try garbage collecting and
// continuing.
static value_t run_stack_until_signal(safe_value_t s_ambience, safe_value_t s_stack) {
  loop: do {
    value_t ambience = deref(s_ambience);
    value_t stack = deref(s_stack);
    value_t result = run_stack_until_condition(ambience, stack);
    if (in_condition_cause(ccHeapExhausted, result)) {
      runtime_t *runtime = get_ambience_runtime(ambience);
      runtime_garbage_collect(runtime);
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
  // Create the stack to run the code on.
  runtime_t *runtime = get_ambience_runtime(ambience);
  TRY_DEF(stack, new_heap_stack(runtime, 1024));
  // Push an activation onto the empty stack to get execution going.
  size_t frame_size = get_code_block_high_water_mark(code);
  frame_t frame = open_stack(stack);
  TRY(push_stack_frame(runtime, stack, &frame, frame_size, ROOT(runtime, empty_array)));
  frame_set_code_block(&frame, code);
  close_frame(&frame);
  // Run the stack.
  return run_stack_until_condition(ambience, stack);
}

value_t run_code_block(safe_value_t s_ambience, safe_value_t code) {
  runtime_t *runtime = get_ambience_runtime(deref(s_ambience));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    // Build a stack to run the code on.
    E_S_TRY_DEF(s_stack, protect(pool, new_heap_stack(runtime, 1024)));
    {
      frame_t frame = open_stack(deref(s_stack));
      // Set up the initial frame.
      size_t frame_size = get_code_block_high_water_mark(deref(code));
      E_TRY(push_stack_frame(runtime, deref(s_stack), &frame, frame_size,
          ROOT(runtime, empty_array)));
      frame_set_code_block(&frame, deref(code));
      close_frame(&frame);
    }
    // Run until completion.
    E_RETURN(run_stack_until_signal(s_ambience, s_stack));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}
