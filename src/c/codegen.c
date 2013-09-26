// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "try-inl.h"
#include "value-inl.h"


// --- S c o p e s ---

static scope_lookup_callback_t kBottomCallback;
static scope_lookup_callback_t *bottom_callback = NULL;

// Returns the bottom callback that never finds symbols.
static scope_lookup_callback_t *get_bottom_callback() {
  if (bottom_callback == NULL) {
    scope_lookup_callback_init_bottom(&kBottomCallback);
    bottom_callback = &kBottomCallback;
  }
  return bottom_callback;
}

void scope_lookup_callback_init(scope_lookup_callback_t *callback,
    scope_lookup_function_t function, void *data) {
  callback->function = function;
  callback->data = data;
}

static value_t bottom_scope_lookup(value_t symbol, void *data,
    binding_info_t *info_out) {
  return new_signal(scNotFound);
}

void scope_lookup_callback_init_bottom(scope_lookup_callback_t *callback) {
  scope_lookup_callback_init(callback, bottom_scope_lookup, NULL);
}

value_t scope_lookup_callback_call(scope_lookup_callback_t *callback,
    value_t symbol, binding_info_t *info_out) {
  return (callback->function)(symbol, callback->data, info_out);
}

// Performs a lookup for a single symbol scope.
static value_t single_symbol_scope_lookup(value_t symbol, void *data,
    binding_info_t *info_out) {
  single_symbol_scope_t *scope = data;
  if (value_identity_compare(symbol, scope->symbol)) {
    if (info_out != NULL)
      *info_out = scope->binding;
    return success();
  } else {
    return scope_lookup_callback_call(scope->outer, symbol, info_out);
  }
}

void assembler_push_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_t *scope, value_t symbol, binding_type_t type,
    uint32_t data) {
  scope->symbol = symbol;
  scope->binding = (binding_info_t) {type, data};
  scope_lookup_callback_init(&scope->callback, single_symbol_scope_lookup, scope);
  scope->outer = assembler_set_scope_callback(assm, &scope->callback);
}

void assembler_pop_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_t *scope) {
  CHECK_EQ("scopes out of sync", assm->scope_callback, &scope->callback);
  assm->scope_callback = scope->outer;
}

// Encoder-decoder union that lets a binding info struct be packed into an int64
// to be stored in a tagged integer.
typedef union {
  binding_info_t decoded;
  int64_t encoded;
} binding_info_codec_t;

// Performs a lookup for a single symbol scope.
static value_t map_scope_lookup(value_t symbol, void *data,
    binding_info_t *info_out) {
  map_scope_t *scope = data;
  value_t value = get_id_hash_map_at(scope->map, symbol);
  if (is_signal(scNotFound, value)) {
    return scope_lookup_callback_call(scope->outer, symbol, info_out);
  } else {
    if (info_out != NULL) {
      binding_info_codec_t codec = {.encoded=get_integer_value(value)};
      *info_out = codec.decoded;
    }
    return success();
  }
}

value_t assembler_push_map_scope(assembler_t *assm, map_scope_t *scope) {
  TRY_SET(scope->map, new_heap_id_hash_map(assm->runtime, 8));
  scope_lookup_callback_init(&scope->callback, map_scope_lookup, scope);
  scope->outer = assembler_set_scope_callback(assm, &scope->callback);
  scope->assembler = assm;
  return success();
}

void assembler_pop_map_scope(assembler_t *assm, map_scope_t *scope) {
  CHECK_EQ("scopes out of sync", assm->scope_callback, &scope->callback);
  assm->scope_callback = scope->outer;
}

value_t map_scope_bind(map_scope_t *scope, value_t symbol, binding_type_t type,
    uint32_t data) {
  binding_info_codec_t codec = {.decoded={type, data}};
  value_t value = new_integer(codec.encoded);
  TRY(set_id_hash_map_at(scope->assembler->runtime, scope->map, symbol, value));
  return success();
}

static value_t capture_scope_lookup(value_t symbol, void *data,
    binding_info_t *info_out) {
  capture_scope_t *scope = data;
  size_t capture_count_before = get_array_buffer_length(scope->captures);
  // See if we've captured this variable before.
  for (size_t i = 0; i < capture_count_before; i++) {
    value_t captured = get_array_buffer_at(scope->captures, i);
    if (value_identity_compare(captured, symbol)) {
      // Found it. Record that we did if necessary and return success.
      if (info_out != NULL) {
        info_out->type = btCaptured;
        info_out->data = i;
      }
      return success();
    }
  }
  // We haven't seen this one before so look it up outside.
  value_t value = scope_lookup_callback_call(scope->outer, symbol, info_out);
  if (info_out != NULL && !is_signal(scNotFound, value)) {
    // We found something and this is a read. Add it to the list of captures.
    runtime_t *runtime = scope->assembler->runtime;
    if (get_array_buffer_length(scope->captures) == 0) {
      // The first time we add something we have to create a new array buffer
      // since all empty capture scopes share the singleton empty buffer.
      TRY_SET(scope->captures, new_heap_array_buffer(runtime, 2));
    }
    TRY(add_to_array_buffer(runtime, scope->captures, symbol));
    info_out->type = btCaptured;
    info_out->data = capture_count_before;
  }
  return value;
}

value_t assembler_push_capture_scope(assembler_t *assm, capture_scope_t *scope) {
  scope_lookup_callback_init(&scope->callback, capture_scope_lookup, scope);
  scope->outer = assembler_set_scope_callback(assm, &scope->callback);
  scope->captures = ROOT(assm->runtime, empty_array_buffer);
  scope->assembler = assm;
  return success();
}

void assembler_pop_capture_scope(assembler_t *assm, capture_scope_t *scope) {
  CHECK_EQ("scopes out of sync", assm->scope_callback, &scope->callback);
  assm->scope_callback = scope->outer;
}

value_t assembler_lookup_symbol(assembler_t *assm, value_t symbol,
    binding_info_t *info_out) {
  return scope_lookup_callback_call(assm->scope_callback, symbol, info_out);
}

bool assembler_is_symbol_bound(assembler_t *assm, value_t symbol) {
  return !is_signal(scNotFound, assembler_lookup_symbol(assm, symbol, NULL));
}


// --- S c r a t c h ---

void reusable_scratch_memory_init(reusable_scratch_memory_t *memory) {
  memory->memory = memory_block_empty();
}

void reusable_scratch_memory_dispose(reusable_scratch_memory_t *memory) {
  allocator_default_free(memory->memory);
  memory->memory = memory_block_empty();
}

void *reusable_scratch_memory_alloc(reusable_scratch_memory_t *memory,
    size_t size) {
  memory_block_t current = memory->memory;
  if (current.size < size) {
    // If the current memory block is too small to handle what we're asking
    // for replace it with a new one with room enough.
    allocator_default_free(current);
    current = allocator_default_malloc(size * 2);
    memory->memory = current;
  }
  return current.memory;
}


// --- A s s e m b l e r ---

value_t assembler_init(assembler_t *assm, runtime_t *runtime,
    scope_lookup_callback_t *scope_callback) {
  if (scope_callback == NULL)
    scope_callback = get_bottom_callback();
  TRY_SET(assm->value_pool, new_heap_id_hash_map(runtime, 16));
  assm->scope_callback = scope_callback;
  assm->runtime = runtime;
  byte_buffer_init(&assm->code);
  assm->stack_height = assm->high_water_mark = 0;
  reusable_scratch_memory_init(&assm->scratch_memory);
  return success();
}

void assembler_dispose(assembler_t *assm) {
  byte_buffer_dispose(&assm->code);
  reusable_scratch_memory_dispose(&assm->scratch_memory);
}

scope_lookup_callback_t *assembler_set_scope_callback(assembler_t *assm,
    scope_lookup_callback_t *callback) {
  scope_lookup_callback_t *result = assm->scope_callback;
  assm->scope_callback = callback;
  return result;
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

void *assembler_scratch_malloc(assembler_t *assm, size_t size) {
  return reusable_scratch_memory_alloc(&assm->scratch_memory, size);
}

void assembler_scratch_double_malloc(assembler_t *assm, size_t first_size,
    void **first, size_t second_size, void **second) {
  void *block = assembler_scratch_malloc(assm, first_size + second_size);
  *first = block;
  *second = ((byte_t*) block) + second_size;
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

value_t assembler_emit_delegate_lambda_call(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDelegateToLambda);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record) {
  CHECK_FAMILY(ofMethodspace, space);
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

value_t assembler_emit_builtin(assembler_t *assm, builtin_method_t builtin) {
  TRY_DEF(wrapper, new_heap_void_p(assm->runtime, builtin));
  assembler_emit_opcode(assm, ocBuiltin);
  TRY(assembler_emit_value(assm, wrapper));
  // Pushes the result.
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_return(assembler_t *assm) {
  assembler_emit_opcode(assm, ocReturn);
  return success();
}

value_t assembler_emit_load_local(assembler_t *assm, size_t index) {
  assembler_emit_opcode(assm, ocLoadLocal);
  assembler_emit_byte(assm, index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_global(assembler_t *assm, value_t name,
    value_t namespace) {
  assembler_emit_opcode(assm, ocLoadGlobal);
  TRY(assembler_emit_value(assm, name));
  TRY(assembler_emit_value(assm, namespace));
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_argument(assembler_t *assm, size_t param_index) {
  assembler_emit_opcode(assm, ocLoadArgument);
  assembler_emit_byte(assm, param_index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_outer(assembler_t *assm, size_t index) {
  assembler_emit_opcode(assm, ocLoadOuter);
  assembler_emit_byte(assm, index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_lambda(assembler_t *assm, value_t methods,
    size_t outer_count) {
  assembler_emit_opcode(assm, ocLambda);
  TRY(assembler_emit_value(assm, methods));
  assembler_emit_byte(assm, outer_count);
  // Pop off all the outers and push back the lambda.
  assembler_adjust_stack_height(assm, -outer_count+1);
  return success();
}
