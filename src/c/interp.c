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
  value_t lambda = get_method_syntax(method);
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, NULL));
  E_BEGIN_TRY_FINALLY();
    value_t dummy;
    E_TRY_DEF(code, compile_method_body(&assm, get_lambda_ast_signature(lambda),
        get_lambda_ast_body(lambda), &dummy));
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

value_t run_stack(runtime_t *runtime, value_t stack) {
  frame_t frame;
  get_stack_top_frame(stack, &frame);
  interpreter_state_t state;
  interpreter_state_load(&state, &frame);
  while (true) {
    opcode_t opcode = read_next_byte(&state);
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
        value_t space = read_next_value(&state);
        CHECK_FAMILY(ofMethodspace, space);
        value_t arg_map;
        value_t method = lookup_methodspace_method(runtime, space, record,
            &frame, &arg_map);
        if (is_signal(scNotFound, method)) {
          value_to_string_t data;
          ERROR("Unresolved method for %s.", value_to_string(&data, record));
          dispose_value_to_string(&data);
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
        if (is_signal(scNotFound, method)) {
          value_to_string_t data;
          ERROR("Unresolved delegate for %s.", value_to_string(&data, record));
          dispose_value_to_string(&data);
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
        builtin_method_t impl = get_void_p_value(wrapper);
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
      case ocLoadLocal: {
        size_t index = read_next_byte(&state);
        value_t value = frame_get_local(&frame, index);
        frame_push_value(&frame, value);
        break;
      }
      case ocLoadGlobal: {
        value_t name = read_next_value(&state);
        value_t namespace = read_next_value(&state);
        CHECK_FAMILY(ofNamespace, namespace);
        TRY_DEF(value, get_namespace_binding_at(namespace, name));
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
