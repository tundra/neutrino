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
  set_frame_pc(frame, state->pc);
}

// Load state from the given frame into the state object such that it can be
// interpreted.
static void interpreter_state_load(interpreter_state_t *state, frame_t *frame) {
  state->pc = get_frame_pc(frame);
  value_t code_block = get_frame_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  get_blob_data(bytecode, &state->bytecode);
  state->value_pool = get_code_block_value_pool(code_block);
}

static void interpreter_state_push(interpreter_state_t *state, frame_t *frame) {
  frame_t snapshot = *frame;
  frame_push_value(frame, new_integer(snapshot.stack_pointer));
  frame_push_value(frame, new_integer(snapshot.frame_pointer));
  frame_push_value(frame, new_integer(snapshot.capacity));
  frame_push_value(frame, new_integer(state->pc));
}

static void interpreter_state_pop(interpreter_state_t *state, frame_t *frame) {
  frame_t snapshot;
  state->pc = get_integer_value(frame_pop_value(frame));
  snapshot.capacity = get_integer_value(frame_pop_value(frame));
  snapshot.frame_pointer = get_integer_value(frame_pop_value(frame));
  snapshot.stack_pointer = get_integer_value(frame_pop_value(frame));
  value_t code_block = get_frame_code_block(&snapshot);
  value_t bytecode = get_code_block_bytecode(code_block);
  get_blob_data(bytecode, &state->bytecode);
  state->value_pool = get_code_block_value_pool(code_block);
  *frame = snapshot;
}

// Reads over and returns the next byte in the bytecode stream.
static byte_t read_next_byte(interpreter_state_t *state) {
  return blob_byte_at(&state->bytecode, state->pc++);
}

// Reads over the next value index from the code and returns the associated
// value pool value.
static value_t read_next_value(interpreter_state_t *state) {
  size_t index = read_next_byte(state);
  return get_array_at(state->value_pool, index);
}

// Reads a byte from the previous instruction, assuming that we're currently
// at the first byte of the next instruction. Size is the size of the
// instruction, offset is the offset of the byte we want to read.
static byte_t peek_previous_byte(interpreter_state_t *state, size_t size,
    size_t offset) {
  return blob_byte_at(&state->bytecode, state->pc - size + offset);
}

// Reads a value pool value from the previous instruction, assuming that we're
// currently at the first byte of the next instruction.
static value_t peek_previous_value(interpreter_state_t *state, size_t size,
    size_t offset) {
  size_t index = peek_previous_byte(state, size, offset);
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

static void frame_log_info(frame_t *frame) {
  INFO("frame %p", frame);
  INFO(" - stack_piece: %v", frame->stack_piece);
  INFO(" - stack_pointer: %i", frame->stack_pointer);
  INFO(" - frame_pointer: %i", frame->frame_pointer);
  INFO(" - capacity: %i", frame->capacity);
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

static void log_lookup_error(value_t signal, value_t record, frame_t *frame) {
  size_t arg_count = get_invocation_record_argument_count(record);
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%v: {", signal);
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

value_t run_stack(runtime_t *runtime, value_t stack) {
  frame_t frame;
  get_stack_top_frame(stack, &frame);
  interpreter_state_t state;
  interpreter_state_load(&state, &frame);
  while (true) {
    opcode_t opcode = (opcode_t) read_next_byte(&state);
    switch (opcode) {
      case ocPush: {
        value_t value = read_next_value(&state);
        frame_push_value(&frame, value);
        break;
      }
      case ocPop: {
        size_t count = read_next_byte(&state);
        for (size_t i = 0; i < count; i++)
          frame_pop_value(&frame);
        break;
      }
      case ocCheckStackHeight: {
        size_t expected = read_next_byte(&state);
        size_t height = frame.stack_pointer - frame.frame_pointer;
        CHECK_EQ("stack height", expected, height);
        break;
      }
      case ocNewArray: {
        size_t length = read_next_byte(&state);
        TRY_DEF(array, new_heap_array(runtime, length));
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
        value_t arg_map;
        value_t method = lookup_fragment_method(runtime, fragment, record,
            &frame, &arg_map);
        if (is_signal(scLookupError, method)) {
          log_lookup_error(method, record, &frame);
          return method;
        }
        // The lookup may have failed with a different signal. Check for that.
        TRY(method);
        // Push a new activation.
        interpreter_state_store(&state, &frame);
        TRY_DEF(code_block, ensure_method_code(runtime, method));
        push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block));
        set_frame_code_block(&frame, code_block);
        set_frame_argument_map(&frame, arg_map);
        interpreter_state_load(&state, &frame);
        break;
      }
      case ocDelegateToLambda: {
        // Get the lambda to delegate to.
        value_t lambda = frame_get_argument(&frame, 0);
        CHECK_FAMILY(ofLambda, lambda);
        value_t space = get_lambda_methods(lambda);
        // Pop off the top frame since we're repeating the previous call.
        bool popped = pop_stack_frame(stack, &frame);
        CHECK_TRUE("delegating from bottom frame", popped);
        interpreter_state_load(&state, &frame);
        // Extract the invocation record from the calling instruction.
        CHECK_EQ("invalid calling instruction", ocInvoke,
            peek_previous_byte(&state, 3, 0));
        value_t record = peek_previous_value(&state, 3, 1);
        CHECK_FAMILY(ofInvocationRecord, record);
        // From this point we do the same as invoke except using the space from
        // the lambda rather than the space from the invoke instruction.
        value_t arg_map;
        value_t method = lookup_methodspace_method(runtime, space, record,
            &frame, &arg_map);
        if (is_signal(scLookupError, method)) {
          log_lookup_error(method, record, &frame);
          return method;
        }
        // The lookup may have failed with a different signal. Check for that.
        TRY(method);
        TRY_DEF(code_block, ensure_method_code(runtime, method));
        push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block));
        set_frame_code_block(&frame, code_block);
        set_frame_argument_map(&frame, arg_map);
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
        if (!pop_stack_frame(stack, &frame)) {
          // TODO: push a bottom frame on a clean stack that does the final
          //   return rather than make return do both.
          return result;
        } else {
          interpreter_state_load(&state, &frame);
          frame_push_value(&frame, result);
          break;
        }
      }
      case ocSlap: {
        value_t value = frame_pop_value(&frame);
        size_t argc = read_next_byte(&state);
        for (size_t i = 0; i < argc; i++)
          frame_pop_value(&frame);
        frame_push_value(&frame, value);
        break;
      }
      case ocNewReference: {
        // Create the reference first so that if it fails we haven't clobbered
        // the stack yet.
        TRY_DEF(ref, new_heap_reference(runtime, nothing()));
        value_t value = frame_pop_value(&frame);
        set_reference_value(ref, value);
        TRY(frame_push_value(&frame, ref));
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
        TRY(frame_push_value(&frame, value));
        break;
      }
      case ocLoadLocal: {
        size_t index = read_next_byte(&state);
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
        TRY_DEF(value, module_lookup_identifier(runtime, module,
            get_identifier_stage(ident), get_identifier_path(ident)));
        frame_push_value(&frame, value);
        break;
      }
      case ocLoadArgument: {
        size_t param_index = read_next_byte(&state);
        value_t value = frame_get_argument(&frame, param_index);
        frame_push_value(&frame, value);
        break;
      }
      case ocLoadOuter: {
        size_t index = read_next_byte(&state);
        value_t subject = frame_get_argument(&frame, 0);
        CHECK_FAMILY(ofLambda, subject);
        value_t value = get_lambda_outer(subject, index);
        frame_push_value(&frame, value);
        break;
      }
      case ocLambda: {
        value_t space = read_next_value(&state);
        CHECK_FAMILY(ofMethodspace, space);
        size_t outer_count = read_next_byte(&state);
        value_t outers;
        if (outer_count == 0) {
          outers = ROOT(runtime, empty_array);
        } else {
          TRY_SET(outers, new_heap_array(runtime, outer_count));
          for (size_t i = 0; i < outer_count; i++)
            set_array_at(outers, i, frame_pop_value(&frame));
        }
        TRY_DEF(lambda, new_heap_lambda(runtime, space, outers));
        frame_push_value(&frame, lambda);
        break;
      }
      case ocCaptureEscape: {
        frame_log_info(&frame);
        // Push the current interpreter state onto the stack.
        interpreter_state_push(&state, &frame);
        value_t stack_piece = frame.stack_piece;
        value_t stack_pointer = new_integer(frame.stack_pointer);
        INFO("Created escape for %v@%v", stack_piece, stack_pointer);
        TRY_DEF(escape, new_heap_escape(runtime, yes(), stack_piece, stack_pointer));
        frame_push_value(&frame, escape);
        break;
      }
      case ocFireEscape: {
        value_t escape = frame_get_argument(&frame, 0);
        CHECK_FAMILY(ofEscape, escape);
        value_t value = frame_get_argument(&frame, 2);
        frame_push_value(&frame, value);
        break;
      }
      default:
        ERROR("Unexpected opcode %i", opcode);
        UNREACHABLE("unexpected opcode");
        break;
    }
  }
  return success();
}

value_t run_code_block_until_signal(runtime_t *runtime, value_t code) {
  TRY_DEF(stack, new_heap_stack(runtime, 1024));
  frame_t frame;
  size_t frame_size = get_code_block_high_water_mark(code);
  push_stack_frame(runtime, stack, &frame, frame_size);
  set_frame_code_block(&frame, code);
  set_frame_argument_map(&frame, ROOT(runtime, empty_array));
  return run_stack(runtime, stack);
}

value_t run_code_block(runtime_t *runtime, safe_value_t code) {
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    // Build a stack to run the code on.
    E_S_TRY_DEF(s_stack, protect(pool, new_heap_stack(runtime, 1024)));
    {
      // Set up the initial frame.
      size_t frame_size = get_code_block_high_water_mark(deref(code));
      frame_t frame;
      push_stack_frame(runtime, deref(s_stack), &frame, frame_size);
      set_frame_code_block(&frame, deref(code));
      set_frame_argument_map(&frame, ROOT(runtime, empty_array));
    }
    // Run until completion.
    E_RETURN(run_stack(runtime, deref(s_stack)));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}
