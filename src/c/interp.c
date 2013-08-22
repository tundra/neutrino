// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "log.h"
#include "process.h"
#include "value-inl.h"

// --- I n t e r p r e t e r ---

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
        CHECK_FAMILY(ofMethodSpace, space);
        value_t method = lookup_method_space_method(runtime, space, record, &frame);
        if (is_signal(scNotFound, method)) {
          value_to_string_t data;
          ERROR("Unresolved method for %s.", value_to_string(&data, record));
          dispose_value_to_string(&data);
          return method;
        }
        // Push a new activation.
        interpreter_state_store(&state, &frame);
        value_t code_block = get_method_code(method);
        push_stack_frame(runtime, stack, &frame, get_code_block_high_water_mark(code_block));
        set_frame_code_block(&frame, code_block);
        interpreter_state_load(&state, &frame);
        break;
      }
      case ocBuiltin: {
        value_t wrapper = read_next_value(&state);
        builtin_method_t impl = get_void_p_value(wrapper);
        size_t argc = read_next_byte(&state);
        builtin_arguments_t args;
        builtin_arguments_init(&args, runtime, &frame, argc);
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
      default:
        ERROR("Unexpected opcode %i", opcode);
        UNREACHABLE("unexpected opcode");
        break;
    }
  }
  return success();
}

value_t run_code_block(runtime_t *runtime, value_t code) {
  TRY_DEF(stack, new_heap_stack(runtime, 1024));
  frame_t frame;
  size_t frame_size = get_code_block_high_water_mark(code);
  push_stack_frame(runtime, stack, &frame, frame_size);
  set_frame_code_block(&frame, code);
  return run_stack(runtime, stack);
}


// --- A s s e m b l e r ---

value_t assembler_init(assembler_t *assm, runtime_t *runtime, value_t space) {
  TRY_SET(assm->value_pool, new_heap_id_hash_map(runtime, 16));
  TRY_SET(assm->local_variable_map, new_heap_id_hash_map(runtime, 16));
  assm->runtime = runtime;
  assm->space = space;
  byte_buffer_init(&assm->code, NULL);
  assm->stack_height = assm->high_water_mark = 0;
  return success();
}

void assembler_dispose(assembler_t *assm) {
  byte_buffer_dispose(&assm->code);
}

value_t assembler_flush(assembler_t *assm) {
  // Copy the bytecode into a blob object.
  blob_t code_blob;
  byte_buffer_flush(&assm->code, &code_blob);
  TRY_DEF(bytecode, new_heap_blob_with_data(assm->runtime, &code_blob));
  // Invert the constant pool map into an array.
  value_t value_pool_map = assm->value_pool;
  size_t value_pool_size = get_id_hash_map_size(value_pool_map);
  TRY_DEF(value_pool, new_heap_array(assm->runtime, value_pool_size));
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, value_pool_map);
  size_t entries_seen = 0;
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    size_t index = get_integer_value(value);
    // Check that the entry hasn't been set already.
    CHECK_FAMILY(ofNull, get_array_at(value_pool, index));
    set_array_at(value_pool, index, key);
    entries_seen++;
  }
  CHECK_EQ("wrong number of entries", entries_seen, value_pool_size);
  return new_heap_code_block(assm->runtime, bytecode, value_pool,
      assm->high_water_mark);
}

// Writes a single byte to this assembler.
static void assembler_emit_byte(assembler_t *assm, size_t value) {
  CHECK_TRUE("large value", value <= 0xFF);
  byte_buffer_append(&assm->code, (byte_t) value);
}

// Writes an opcode to this assembler.
static void assembler_emit_opcode(assembler_t *assm, opcode_t opcode) {
  assembler_emit_byte(assm, opcode);
}

// Writes a reference to a value in the value pool, adding the value to the
// pool if necessary.
static value_t assembler_emit_value(assembler_t *assm, value_t value) {
  value_t value_pool = assm->value_pool;
  // Check if we've already emitted this value then we can use the index again.
  value_t prev_index = get_id_hash_map_at(value_pool, value);
  int64_t index;
  if (is_signal(scNotFound, prev_index)) {
    // We haven't so we add the value to the value pool.
    index = get_id_hash_map_size(value_pool);
    TRY(set_id_hash_map_at(assm->runtime, value_pool, value, new_integer(index)));
  } else {
    // Yes we have, grab the previous index.
    index = get_integer_value(prev_index);
  }
  // TODO: handle the case where there's more than 0xFF constants.
  CHECK_TRUE("negative index", index >= 0);
  CHECK_TRUE("large index", index <= 0xFF);
  byte_buffer_append(&assm->code, index);
  return success();
}

// Adds the given delta to the recorded stack height and updates the high water
// mark if necessary.
static void assembler_adjust_stack_height(assembler_t *assm, int delta) {
  assm->stack_height += delta;
  assm->high_water_mark = max_size(assm->high_water_mark, assm->stack_height);
}

value_t assembler_emit_push(assembler_t *assm, value_t value) {
  assembler_emit_opcode(assm, ocPush);
  TRY(assembler_emit_value(assm, value));
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_pop(assembler_t *assm, size_t count) {
  assembler_emit_opcode(assm, ocPop);
  assembler_emit_byte(assm, count);
  assembler_adjust_stack_height(assm, -count);
  return success();
}

value_t assembler_emit_new_array(assembler_t *assm, size_t length) {
  assembler_emit_opcode(assm, ocNewArray);
  assembler_emit_byte(assm, length);
  // Pops off 'length' elements, pushes back an array.
  assembler_adjust_stack_height(assm, -length+1);
  return success();
}

value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record) {
  CHECK_FAMILY(ofMethodSpace, space);
  CHECK_FAMILY(ofInvocationRecord, record);
  assembler_emit_opcode(assm, ocInvoke);
  TRY(assembler_emit_value(assm, record));
  TRY(assembler_emit_value(assm, space));
  // The result will be pushed onto the stack on top of the arguments.
  assembler_adjust_stack_height(assm, 1);
  size_t argc = get_invocation_record_argument_count(record);
  assembler_emit_opcode(assm, ocSlap);
  assembler_emit_byte(assm, argc);
  // Pop off all the arguments.
  assembler_adjust_stack_height(assm, -argc);
  return success();
}

value_t assembler_emit_builtin(assembler_t *assm, builtin_method_t builtin,
    size_t argc) {
  TRY_DEF(wrapper, new_heap_void_p(assm->runtime, builtin));
  assembler_emit_opcode(assm, ocBuiltin);
  TRY(assembler_emit_value(assm, wrapper));
  assembler_emit_byte(assm, argc);
  // Pushes the result.
  assembler_adjust_stack_height(assm, 1);
  return success();
}

value_t assembler_emit_return(assembler_t *assm) {
  assembler_emit_opcode(assm, ocReturn);
  return success();
}

value_t assembler_emit_load_local(assembler_t *assm, size_t index) {
  assembler_emit_opcode(assm, ocLoadLocal);
  assembler_emit_byte(assm, index);
  assembler_adjust_stack_height(assm, 1);
  return success();
}

value_t assembler_bind_local_variable(assembler_t *assm, value_t symbol,
    size_t index) {
  CHECK_FALSE("symbol already bound",
      has_id_hash_map_at(assm->local_variable_map, symbol));
  return set_id_hash_map_at(assm->runtime, assm->local_variable_map, symbol,
      new_integer(index));
}

value_t assembler_unbind_local_variable(assembler_t *assm, value_t symbol) {
  value_t deleted = delete_id_hash_map_at(assm->runtime, assm->local_variable_map,
      symbol);
  CHECK_FALSE("delete local failed", is_signal(scNotFound, deleted));
  return deleted;
}

bool assembler_is_local_variable_bound(assembler_t *assm, value_t symbol) {
  return has_id_hash_map_at(assm->local_variable_map, symbol);
}

size_t assembler_get_local_variable_binding(assembler_t *assm, value_t symbol) {
  CHECK_TRUE("no symbol binding", assembler_is_local_variable_bound(assm, symbol));
  return get_integer_value(get_id_hash_map_at(assm->local_variable_map, symbol));
}
