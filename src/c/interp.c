// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "log.h"
#include "process.h"
#include "safe-inl.h"
#include "try-inl.h"
#include "value-inl.h"


// Encapsulates the working state of the interpreter. The assumption is that
// since this is allocated on the stack and the calls that use it will be
// inlined this is just a convenient way to work with what are essentially
// local variables.
typedef struct {
  // Current program counter.
  size_t pc;
  // The raw bytecode.
  blob_t bytecode;
  // The pool of constant values used by the bytecode.
  value_t value_pool;
} interpreter_state_t;

// Stores the current interpreter state back into the given frame.
static void interpreter_state_store(interpreter_state_t *state, frame_t *frame) {
  frame_set_pc(frame, state->pc);
}

// Load state from the given frame into the state object such that it can be
// interpreted.
static void interpreter_state_load(interpreter_state_t *state, frame_t *frame) {
  state->pc = frame_get_pc(frame);
  value_t code_block = frame_get_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  get_blob_data(bytecode, &state->bytecode);
  state->value_pool = get_code_block_value_pool(code_block);
}

// Pushes the current state of the interpreter onto the stack such that it can
// later be restored. Returns a location value that must later be passed to
// interpreter_state_restore when restoring the state. The pc offset is a delta
// to add to the current pc in the case there the code location to return to is
// not just the next instruction. The sp_offset is a delta to add to the stack
// pointer to account for any changes that happen after pushing the state that
// must be included when restoring the state.
static size_t interpreter_state_push(interpreter_state_t *state, frame_t *frame,
    size_t pc_offset, int sp_offset) {
  frame_t snapshot = *frame;
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  size_t location = snapshot.stack_pointer - stack_start;
  frame_push_value(frame, new_integer(snapshot.stack_pointer + sp_offset - stack_start));
  frame_push_value(frame, new_integer(snapshot.frame_pointer - stack_start));
  frame_push_value(frame, new_integer(snapshot.limit_pointer - stack_start));
  frame_push_value(frame, snapshot.flags);
  frame_push_value(frame, new_integer(state->pc + pc_offset));
  return location;
}

// Restores the previous state of the interpreter that, when captured, returned
// the given location value.
static void interpreter_state_restore(interpreter_state_t *state, frame_t *frame,
    value_t stack, value_t stack_piece, size_t location) {
  // Read the state from the given stack piece.
  value_t storage = get_stack_piece_storage(stack_piece);
  value_t stack_pointer = get_array_at(storage, location);
  value_t frame_pointer = get_array_at(storage, location + 1);
  value_t limit_pointer = get_array_at(storage, location + 2);
  value_t flags = get_array_at(storage, location + 3);
  value_t pc = get_array_at(storage, location + 4);
  // Restore the state to the stack/stack piece objects. We need those values
  // to be consistent with the frame so write them there and then update the
  // frame based on those values.
  if (!is_same_value(stack_piece, frame->stack_piece)) {
    set_stack_top_piece(stack, stack_piece);
    open_stack_piece(stack_piece, frame);
  }
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  frame->stack_pointer = stack_start + get_integer_value(stack_pointer);
  frame->frame_pointer = stack_start + get_integer_value(frame_pointer);
  frame->limit_pointer = stack_start + get_integer_value(limit_pointer);
  frame->flags = flags;
  // Restore the interpreter state from the now restored frame.
  state->pc = get_integer_value(pc);
  value_t code_block = frame_get_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  get_blob_data(bytecode, &state->bytecode);
  state->value_pool = get_code_block_value_pool(code_block);
}

// Reads over and returns the next word in the bytecode stream.
static short_t read_next_short(interpreter_state_t *state) {
  return blob_short_at(&state->bytecode, state->pc++);
}

// Reads over the next value index from the code and returns the associated
// value pool value.
static value_t read_next_value(interpreter_state_t *state) {
  size_t index = read_next_short(state);
  return get_array_at(state->value_pool, index);
}

// Reads a word from the previous instruction, assuming that we're currently
// at the first word of the next instruction. Size is the size of the
// instruction, offset is the offset of the word we want to read.
static short_t peek_previous_short(interpreter_state_t *state, size_t size,
    size_t offset) {
  return blob_short_at(&state->bytecode, state->pc - size + offset);
}

// Reads a value pool value from the previous instruction, assuming that we're
// currently at the first word of the next instruction.
static value_t peek_previous_value(interpreter_state_t *state, size_t size,
    size_t offset) {
  size_t index = peek_previous_short(state, size, offset);
  return get_array_at(state->value_pool, index);
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
  interpreter_state_t state;
  interpreter_state_load(&state, &frame);
  E_BEGIN_TRY_FINALLY();
    while (true) {
      opcode_t opcode = (opcode_t) read_next_short(&state);
      TOPIC_INFO(Interpreter, "Opcode: %s (%i)", get_opcode_name(opcode),
          opcode_counter++);
      switch (opcode) {
        case ocPush: {
          value_t value = read_next_value(&state);
          frame_push_value(&frame, value);
          break;
        }
        case ocPop: {
          size_t count = read_next_short(&state);
          for (size_t i = 0; i < count; i++)
            frame_pop_value(&frame);
          break;
        }
        case ocCheckStackHeight: {
          size_t expected = read_next_short(&state);
          size_t height = frame.stack_pointer - frame.frame_pointer;
          CHECK_EQ("stack height", expected, height);
          break;
        }
        case ocNewArray: {
          size_t length = read_next_short(&state);
          E_TRY_DEF(array, new_heap_array(runtime, length));
          for (size_t i = 0; i < length; i++) {
            value_t element = frame_pop_value(&frame);
            set_array_at(array, length - i - 1, element);
          }
          frame_push_value(&frame, array);
          break;
        }
        case ocInvoke: {
          // Look up the method in the method space.
          value_t record = read_next_value(&state);
          CHECK_FAMILY(ofInvocationRecord, record);
          value_t fragment = read_next_value(&state);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t helper = read_next_value(&state);
          CHECK_FAMILY(ofSignatureMap, helper);
          value_t arg_map;
          value_t method = lookup_method_full(ambience, fragment, record, &frame,
              helper, &arg_map);
          if (is_condition(ccLookupError, method)) {
            log_lookup_error(method, record, &frame);
            E_RETURN(method);
          }
          // The lookup may have failed with a different condition. Check for that.
          E_TRY(method);
          // Push a new activation.
          interpreter_state_store(&state, &frame);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block),
              arg_map);
          frame_set_code_block(&frame, code_block);
          interpreter_state_load(&state, &frame);
          break;
        }
        case ocSignal: {
          // Look up the method in the method space.
          value_t record = read_next_value(&state);
          CHECK_FAMILY(ofInvocationRecord, record);
          value_t fragment = read_next_value(&state);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t helper = read_next_value(&state);
          CHECK_PHYLUM(tpNothing, helper);
          // Push the signal frame onto the stack to record the state of it for
          // the enclosing code.
          interpreter_state_store(&state, &frame);
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
          interpreter_state_load(&state, &frame);
          // Extract the invocation record from the calling instruction.
          CHECK_EQ("invalid calling instruction", ocInvoke,
              peek_previous_short(&state, kInvokeOperationSize, 0));
          value_t record = peek_previous_value(&state, kInvokeOperationSize, 1);
          CHECK_FAMILY(ofInvocationRecord, record);
          // From this point we do the same as invoke except using the space from
          // the lambda rather than the space from the invoke instruction.
          value_t arg_map;
          value_t method = lookup_methodspace_method(ambience, space, record,
              &frame, &arg_map);
          if (is_condition(ccLookupError, method)) {
            log_lookup_error(method, record, &frame);
            E_RETURN(method);
          }
          // The lookup may have failed with a different condition. Check for that.
          E_TRY(method);
          E_TRY_DEF(code_block, ensure_method_code(runtime, method));
          push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block),
              arg_map);
          frame_set_code_block(&frame, code_block);
          interpreter_state_load(&state, &frame);
          break;
        }
        case ocBuiltin: {
          value_t wrapper = read_next_value(&state);
          builtin_method_t impl = (builtin_method_t) get_void_p_value(wrapper);
          builtin_arguments_t args;
          builtin_arguments_init(&args, runtime, &frame);
          value_t result = impl(&args);
          frame_push_value(&frame, result);
          break;
        }
        case ocReturn: {
          value_t result = frame_pop_value(&frame);
          frame_pop_within_stack_piece(&frame);
          interpreter_state_load(&state, &frame);
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
          interpreter_state_load(&state, &frame);
          frame_push_value(&frame, result);
          break;
        }
        case ocSlap: {
          value_t value = frame_pop_value(&frame);
          size_t argc = read_next_short(&state);
          for (size_t i = 0; i < argc; i++)
            frame_pop_value(&frame);
          frame_push_value(&frame, value);
          break;
        }
        case ocNewReference: {
          // Create the reference first so that if it fails we haven't clobbered
          // the stack yet.
          E_TRY_DEF(ref, new_heap_reference(runtime, nothing()));
          value_t value = frame_pop_value(&frame);
          set_reference_value(ref, value);
          E_TRY(frame_push_value(&frame, ref));
          break;
        }
        case ocSetReference: {
          value_t ref = frame_pop_value(&frame);
          CHECK_FAMILY(ofReference, ref);
          value_t value = frame_peek_value(&frame, 0);
          set_reference_value(ref, value);
          break;
        }
        case ocGetReference: {
          value_t ref = frame_pop_value(&frame);
          CHECK_FAMILY(ofReference, ref);
          value_t value = get_reference_value(ref);
          E_TRY(frame_push_value(&frame, value));
          break;
        }
        case ocLoadLocal: {
          size_t index = read_next_short(&state);
          value_t value = frame_get_local(&frame, index);
          frame_push_value(&frame, value);
          break;
        }
        case ocLoadGlobal: {
          value_t ident = read_next_value(&state);
          CHECK_FAMILY(ofIdentifier, ident);
          value_t fragment = read_next_value(&state);
          CHECK_FAMILY(ofModuleFragment, fragment);
          value_t module = get_module_fragment_module(fragment);
          E_TRY_DEF(value, module_lookup_identifier(runtime, module,
              get_identifier_stage(ident), get_identifier_path(ident)));
          frame_push_value(&frame, value);
          break;
        }
        case ocLoadArgument: {
          size_t param_index = read_next_short(&state);
          value_t value = frame_get_argument(&frame, param_index);
          frame_push_value(&frame, value);
          break;
        }
        case ocLoadOuter: {
          size_t index = read_next_short(&state);
          value_t subject = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofLambda, subject);
          value_t value = get_lambda_outer(subject, index);
          frame_push_value(&frame, value);
          break;
        }
        case ocLambda: {
          value_t space = read_next_value(&state);
          CHECK_FAMILY(ofMethodspace, space);
          size_t outer_count = read_next_short(&state);
          value_t outers;
          if (outer_count == 0) {
            outers = ROOT(runtime, empty_array);
          } else {
            E_TRY_SET(outers, new_heap_array(runtime, outer_count));
            for (size_t i = 0; i < outer_count; i++)
              set_array_at(outers, i, frame_pop_value(&frame));
          }
          E_TRY_DEF(lambda, new_heap_lambda(runtime, space, outers));
          frame_push_value(&frame, lambda);
          break;
        }
        case ocCaptureEscape: {
          size_t dest_offset = read_next_short(&state);
          // Push the interpreter state at the end of this instruction onto the
          // stack.
          value_t location = new_integer(interpreter_state_push(&state, &frame,
              dest_offset, kCapturedStateSize + 1));
          value_t stack_piece = frame.stack_piece;
          E_TRY_DEF(escape, new_heap_escape(runtime, yes(), stack_piece, location));
          frame_push_value(&frame, escape);
          break;
        }
        case ocFireEscape: {
          value_t escape = frame_get_argument(&frame, 0);
          CHECK_FAMILY(ofEscape, escape);
          value_t value = frame_get_argument(&frame, 2);
          size_t location = get_integer_value(get_escape_stack_pointer(escape));
          value_t stack_piece = get_escape_stack_piece(escape);
          interpreter_state_restore(&state, &frame, stack, stack_piece, location);
          frame_push_value(&frame, value);
          break;
        }
        case ocKillEscape: {
          value_t value = frame_pop_value(&frame);
          value_t escape = frame_pop_value(&frame);
          CHECK_FAMILY(ofEscape, escape);
          set_escape_is_live(escape, no());
          frame_push_value(&frame, value);
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

static value_t run_stack(value_t ambience, value_t stack) {
  value_t result = run_stack_pushing_signals(ambience, stack);
  if (is_condition(ccSignal, result)) {
    runtime_t *runtime = get_ambience_runtime(ambience);
    frame_t frame = open_stack(stack);
    TRY_DEF(trace, capture_backtrace(runtime, &frame));
    print_ln("%9v", trace);
  }
  return result;
}

const char *get_opcode_name(opcode_t opcode) {
  switch (opcode) {
#define __EMIT_CASE__(Name)                                                    \
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
  return run_stack(ambience, stack);
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
    E_RETURN(run_stack(deref(s_ambience), deref(s_stack)));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}
