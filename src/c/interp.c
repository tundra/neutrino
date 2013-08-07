// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "process.h"
#include "value-inl.h"

// --- I n t e r p r e t e r ---

value_t run_stack(runtime_t *runtime, value_t stack) {
  frame_t frame;
  get_stack_top_frame(stack, &frame);
  size_t pc = get_frame_pc(&frame);
  value_t code_block = get_frame_code_block(&frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  blob_t bytecode_blob;
  get_blob_data(bytecode, &bytecode_blob);
  value_t value_pool = get_code_block_value_pool(code_block);
  while (true) {
    opcode_t opcode = blob_byte_at(&bytecode_blob, pc++);
    switch (opcode) {
      case ocLiteral: {
        size_t index = blob_byte_at(&bytecode_blob, pc++);
        value_t value = get_array_at(value_pool, index);
        frame_push_value(&frame, value);
        break;
      }
      case ocReturn: {
        value_t result = frame_pop_value(&frame);
        return result;
      }
      default:
        printf("opcode: %i\n", opcode);
        CHECK_TRUE("unexpected opcode", false);
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
  set_frame_pc(&frame, 0);
  return run_stack(runtime, stack);
}



// --- A s s e m b l e r ---

value_t assembler_init(assembler_t *assm, runtime_t *runtime) {
  assm->runtime = runtime;
  byte_buffer_init(&assm->code, NULL);
  TRY_SET(assm->value_pool, new_heap_id_hash_map(runtime, 16));
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
  // TODO: calculate this correctly.
  size_t high_water_mark = 16;
  return new_heap_code_block(assm->runtime, bytecode, value_pool, high_water_mark);
}

void assembler_emit_opcode(assembler_t *assm, opcode_t opcode) {
  byte_buffer_append(&assm->code, opcode);
}

value_t assembler_emit_value(assembler_t *assm, value_t value) {
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
