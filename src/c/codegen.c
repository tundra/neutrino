//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "try-inl.h"
#include "utils/log.h"
#include "utils/ook-inl.h"
#include "value-inl.h"


// --- B i n d i n g   i n f o ---

void binding_info_set(binding_info_t *info, binding_type_t type, uint16_t data,
    uint16_t block_depth) {
  info->type = type;
  info->data = data;
  info->block_depth = block_depth;
}


// --- S c o p e s ---

static scope_o kBottomScope;
static scope_o *bottom_scope = NULL;

IMPLEMENTATION(bottom_scope_o, scope_o);

static value_t bottom_scope_lookup(scope_o *self, value_t symbol, binding_info_t *info_out) {
  return new_not_found_condition('d');
}

VTABLE(bottom_scope_o, scope_o) { bottom_scope_lookup };

// Returns the bottom callback that never finds symbols.
scope_o *scope_get_bottom() {
  if (bottom_scope == NULL) {
    VTABLE_INIT(bottom_scope_o, &kBottomScope);
    bottom_scope = &kBottomScope;
  }
  return bottom_scope;
}

value_t scope_lookup(scope_o *self, value_t symbol, binding_info_t *info_out) {
  return METHOD(self, lookup)(self, symbol, info_out);
}

// Performs a lookup for a single symbol scope.
static value_t single_symbol_scope_lookup(scope_o *super_self, value_t symbol,
    binding_info_t *info_out) {
  single_symbol_scope_o *self = DOWNCAST(single_symbol_scope_o, super_self);
  if (value_identity_compare(symbol, self->symbol)) {
    if (info_out != NULL)
      *info_out = self->binding;
    return success();
  } else {
    return scope_lookup(self->outer, symbol, info_out);
  }
}

VTABLE(single_symbol_scope_o, scope_o) { single_symbol_scope_lookup };

void assembler_push_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_o *scope, value_t symbol, binding_type_t type,
    uint16_t data) {
  VTABLE_INIT(single_symbol_scope_o, UPCAST(scope));
  scope->symbol = symbol;
  binding_info_set(&scope->binding, type, data, 0);
  scope->outer = assembler_set_scope(assm, UPCAST(scope));
}

void assembler_pop_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_o *scope) {
  CHECK_PTREQ("scopes out of sync", assm->scope, scope);
  assm->scope = scope->outer;
}

// Encoder-decoder union that lets a binding info struct be packed into an int64
// to be stored in a tagged integer.
typedef union {
  binding_info_t decoded;
  int64_t encoded;
} binding_info_codec_t;

// Performs a lookup for a single symbol scope.
static value_t map_scope_lookup(scope_o *super_self, value_t symbol,
    binding_info_t *info_out) {
  map_scope_o *self = DOWNCAST(map_scope_o, super_self);
  value_t value = get_id_hash_map_at(self->map, symbol);
  if (in_condition_cause(ccNotFound, value)) {
    return scope_lookup(self->outer, symbol, info_out);
  } else {
    if (info_out != NULL) {
      binding_info_codec_t codec;
      codec.encoded = get_integer_value(value);
      *info_out = codec.decoded;
    }
    return success();
  }
}

VTABLE(map_scope_o, scope_o) { map_scope_lookup };

value_t assembler_push_map_scope(assembler_t *assm, map_scope_o *scope) {
  TRY_SET(scope->map, new_heap_id_hash_map(assm->runtime, 8));
  VTABLE_INIT(map_scope_o, UPCAST(scope));
  scope->outer = assembler_set_scope(assm, UPCAST(scope));
  scope->assembler = assm;
  return success();
}

void assembler_pop_map_scope(assembler_t *assm, map_scope_o *scope) {
  CHECK_PTREQ("scopes out of sync", assm->scope, scope);
  assm->scope = scope->outer;
}

value_t map_scope_bind(map_scope_o *scope, value_t symbol, binding_type_t type,
    uint16_t data) {
  binding_info_codec_t codec;
  binding_info_set(&codec.decoded, type, data, 0);
  value_t value = new_integer(codec.encoded);
  TRY(set_id_hash_map_at(scope->assembler->runtime, scope->map, symbol, value));
  return success();
}

static value_t lambda_scope_lookup(scope_o *super_self, value_t symbol,
    binding_info_t *info_out) {
  lambda_scope_o *self = DOWNCAST(lambda_scope_o, super_self);
  int64_t capture_count_before = get_array_buffer_length(self->captures);
  // See if we've captured this variable before.
  for (int64_t i = 0; i < capture_count_before; i++) {
    value_t captured = get_array_buffer_at(self->captures, i);
    if (value_identity_compare(captured, symbol)) {
      // Found it. Record that we did if necessary and return success.
      if (info_out != NULL)
        binding_info_set(info_out, btLambdaCaptured, (uint16_t) i, 0);
      return success();
    }
  }
  // We haven't seen this one before so look it up outside.
  value_t value = scope_lookup(self->outer, symbol, info_out);
  if (info_out != NULL && !in_condition_cause(ccNotFound, value)) {
    // We found something and this is a read. Add it to the list of captures.
    runtime_t *runtime = self->assembler->runtime;
    if (get_array_buffer_length(self->captures) == 0) {
      // The first time we add something we have to create a new array buffer
      // since all empty capture scopes share the singleton empty buffer.
      TRY_SET(self->captures, new_heap_array_buffer(runtime, 2));
    }
    TRY(add_to_array_buffer(runtime, self->captures, symbol));
    binding_info_set(info_out, btLambdaCaptured, (uint16_t) capture_count_before, 0);
  }
  return value;
}

VTABLE(lambda_scope_o, scope_o) { lambda_scope_lookup };

value_t assembler_push_lambda_scope(assembler_t *assm, lambda_scope_o *scope) {
  VTABLE_INIT(lambda_scope_o, UPCAST(scope));
  scope->outer = assembler_set_scope(assm, UPCAST(scope));
  scope->captures = ROOT(assm->runtime, empty_array_buffer);
  scope->assembler = assm;
  return success();
}

void assembler_pop_lambda_scope(assembler_t *assm, lambda_scope_o *scope) {
  CHECK_PTREQ("scopes out of sync", assm->scope, scope);
  assm->scope = scope->outer;
}

static value_t block_scope_lookup(scope_o *super_self, value_t symbol,
    binding_info_t *info_out) {
  block_scope_o *self = DOWNCAST(block_scope_o, super_self);
  // Look up outside this scope.
  value_t value = scope_lookup(self->outer, symbol, info_out);
  if (info_out != NULL && !in_condition_cause(ccNotFound, value))
    // If we found a binding refract it increasing the block depth but otherwise
    // leaving the binding as it is.
    info_out->block_depth++;
  return value;
}

VTABLE(block_scope_o, scope_o) { block_scope_lookup };

value_t assembler_push_block_scope(assembler_t *assm, block_scope_o *scope) {
  VTABLE_INIT(block_scope_o, UPCAST(scope));
  scope->outer = assembler_set_scope(assm, UPCAST(scope));
  scope->assembler = assm;
  return success();
}

void assembler_pop_block_scope(assembler_t *assm, block_scope_o *scope) {
  CHECK_PTREQ("scopes out of sync", assm->scope, scope);
  assm->scope = scope->outer;
}

value_t assembler_lookup_symbol(assembler_t *assm, value_t symbol,
    binding_info_t *info_out) {
  return scope_lookup(assm->scope, symbol, info_out);
}

bool assembler_is_symbol_bound(assembler_t *assm, value_t symbol) {
  return !in_condition_cause(ccNotFound, assembler_lookup_symbol(assm, symbol, NULL));
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

void reusable_scratch_memory_double_alloc(reusable_scratch_memory_t *memory,
    size_t first_size, void **first, size_t second_size, void **second) {
  void *block = reusable_scratch_memory_alloc(memory, first_size + second_size);
  *first = block;
  *second = ((byte_t*) block) + first_size;
}


// --- A s s e m b l e r ---

value_t assembler_init(assembler_t *assm, runtime_t *runtime, value_t fragment,
    scope_o *scope) {
  CHECK_FALSE("no scope callback", scope == NULL);
  CHECK_FAMILY_OPT(ofModuleFragment, fragment);
  TRY(assembler_init_stripped_down(assm, runtime));
  assm->scope = scope;
  assm->fragment = fragment;
  return success();
}

value_t assembler_init_stripped_down(assembler_t *assm, runtime_t *runtime) {
  assm->scope = NULL;
  assm->runtime = runtime;
  assm->fragment = null();
  assm->value_pool = nothing();
  short_buffer_init(&assm->code);
  assm->stack_height = assm->high_water_mark = 0;
  reusable_scratch_memory_init(&assm->scratch_memory);
  return success();
}

void assembler_dispose(assembler_t *assm) {
  short_buffer_dispose(&assm->code);
  reusable_scratch_memory_dispose(&assm->scratch_memory);
}

scope_o *assembler_set_scope(assembler_t *assm, scope_o *scope) {
  scope_o *result = assm->scope;
  assm->scope = scope;
  return result;
}

value_t assembler_flush(assembler_t *assm) {
  // Copy the bytecode into a blob object.
  blob_t code_blob = short_buffer_flush(&assm->code);
  TRY_DEF(bytecode, new_heap_blob_with_data(assm->runtime, code_blob));
  // Invert the constant pool map into an array.
  value_t value_pool = whatever();
  value_t value_pool_map = assm->value_pool;
  if (is_nothing(value_pool_map)) {
    value_pool = ROOT(assm->runtime, empty_array);
  } else {
    int64_t value_pool_size = get_id_hash_map_size(value_pool_map);
    TRY_SET(value_pool, new_heap_array(assm->runtime, (size_t) value_pool_size));
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, value_pool_map);
    int64_t entries_seen = 0;
    while (id_hash_map_iter_advance(&iter)) {
      value_t key;
      value_t value;
      id_hash_map_iter_get_current(&iter, &key, &value);
      int64_t index = get_integer_value(value);
      // Check that the entry hasn't been set already.
      CHECK_PHYLUM(tpNull, get_array_at(value_pool, index));
      set_array_at(value_pool, index, key);
      entries_seen++;
    }
    CHECK_EQ("wrong number of entries", entries_seen, value_pool_size);
  }
  return new_heap_code_block(assm->runtime, bytecode, value_pool,
      assm->high_water_mark);
}

reusable_scratch_memory_t *assembler_get_scratch_memory(assembler_t *assm) {
  return &assm->scratch_memory;
}

// Writes a single byte to this assembler.
static void assembler_emit_short(assembler_t *assm, size_t value) {
  CHECK_REL("large value", value, <=, 0xFFFF);
  short_buffer_append(&assm->code, (short_t) value);
}

// Writes an opcode to this assembler.
static void assembler_emit_opcode(assembler_t *assm, opcode_t opcode) {
  assembler_emit_short(assm, opcode);
}

static void assembler_emit_cursor(assembler_t *assm, short_buffer_cursor_t *out) {
  short_buffer_append_cursor(&assm->code, out);
}

// Writes a reference to a value in the value pool, adding the value to the
// pool if necessary.
static value_t assembler_emit_value(assembler_t *assm, value_t value) {
  if (is_nothing(assm->value_pool))
    TRY_SET(assm->value_pool, new_heap_id_hash_map(assm->runtime, 16));
  value_t value_pool = assm->value_pool;
  // Check if we've already emitted this value then we can use the index again.
  value_t prev_index = get_id_hash_map_at(value_pool, value);
  int64_t index;
  if (in_condition_cause(ccNotFound, prev_index)) {
    // We haven't so we add the value to the value pool.
    index = get_id_hash_map_size(value_pool);
    TRY(set_id_hash_map_at(assm->runtime, value_pool, value, new_integer(index)));
  } else {
    // Yes we have, grab the previous index.
    index = get_integer_value(prev_index);
  }
  // TODO: handle the case where there's more than 0xFF constants.
  CHECK_REL("negative index", index, >=, 0);
  CHECK_REL("large index", index, <=, 0xFF);
  short_buffer_append(&assm->code, (uint16_t) index);
  return success();
}

// Adjusts the stack height and optionally inserts a check stack height op.
static void assembler_emit_stack_height_check(assembler_t *assm) {
  assembler_emit_opcode(assm, ocCheckStackHeight);
  assembler_emit_short(assm, assm->stack_height);
}

// Adjusts the stack height without inserting a check stack height op.
static void assembler_adjust_stack_height_nocheck(assembler_t *assm, int64_t delta) {
  assm->stack_height = (size_t) (assm->stack_height + delta);
  assm->high_water_mark = max_size(assm->high_water_mark, assm->stack_height);
}

void assembler_adjust_stack_height(assembler_t *assm, int64_t delta) {
  assembler_adjust_stack_height_nocheck(assm, delta);
  IF_EXPENSIVE_CHECKS_ENABLED(assembler_emit_stack_height_check(assm));
}

size_t assembler_get_code_cursor(assembler_t *assm) {
  // The length is measured in number of elements so we can just return it
  // directly, there's no need to adjust for the element size.
  return assm->code.length;
}

value_t assembler_emit_push(assembler_t *assm, value_t value) {
  assembler_emit_opcode(assm, ocPush);
  TRY(assembler_emit_value(assm, value));
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_reify_arguments(assembler_t *assm, value_t params) {
  assembler_emit_opcode(assm, ocReifyArguments);
  TRY(assembler_emit_value(assm, params));
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_pop(assembler_t *assm, size_t count) {
  assembler_emit_opcode(assm, ocPop);
  assembler_emit_short(assm, count);
  assembler_adjust_stack_height(assm, -count);
  return success();
}

value_t assembler_emit_slap(assembler_t *assm, size_t count) {
  assembler_emit_opcode(assm, ocSlap);
  assembler_emit_short(assm, count);
  assembler_adjust_stack_height(assm, -count);
  return success();
}

value_t assembler_emit_new_array(assembler_t *assm, size_t length) {
  assembler_emit_opcode(assm, ocNewArray);
  assembler_emit_short(assm, length);
  // Pops off 'length' elements, pushes back an array.
  assembler_adjust_stack_height(assm, -length+1);
  return success();
}

value_t assembler_emit_delegate_lambda_call(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDelegateToLambda);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_delegate_block_call(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDelegateToBlock);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_module_fragment_private_invoke_call_data(assembler_t *assm) {
  assembler_emit_opcode(assm, ocModuleFragmentPrivateInvokeCallData);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_module_fragment_private_invoke_reified_arguments(assembler_t *assm) {
  assembler_emit_opcode(assm, ocModuleFragmentPrivateInvokeReifiedArguments);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_create_escape(assembler_t *assm,
    short_buffer_cursor_t *offset_out) {
  assembler_emit_opcode(assm, ocCreateEscape);
  assembler_emit_cursor(assm, offset_out);
  // We'll record the complete state and also push a barrier containing the
  // escape.
  assembler_adjust_stack_height(assm, get_genus_descriptor(dgEscapeSection)->field_count + 1);
  return success();
}

value_t assembler_emit_goto_forward(assembler_t *assm,
    short_buffer_cursor_t *offset_out) {
  assembler_emit_opcode(assm, ocGoto);
  assembler_emit_cursor(assm, offset_out);
  return success();
}

value_t assembler_emit_fire_escape_or_barrier(assembler_t *assm) {
  // A tiny bit of stack space is required to fire some barriers so the first
  // step here is to push null that take up that space. That way, each time
  // around this op gets executed, if it needs any space it can just pop off
  // the nulls and push on the values it needs to store. The neat part is that
  // this way you never need to know whether any previous instructions have been
  // executed to know if you need to clean up -- there's always junk on the
  // stack so you always have to clean it up.
  assembler_emit_push(assm, null());
  assembler_emit_push(assm, null());
  assembler_emit_opcode(assm, ocFireEscapeOrBarrier);
  // This op never allows execution past it but it simplifies some sanity checks
  // if the stack height looks like it's 1 at the end of the method.
  assembler_adjust_stack_height(assm, -1);
  return success();
}

value_t assembler_emit_leave_or_fire_barrier(assembler_t *assm, size_t argc) {
  // This op works the same way as assembler_emit_fire_escape_or_barrier.
  assembler_emit_push(assm, null());
  assembler_emit_push(assm, null());
  assembler_emit_opcode(assm, ocLeaveOrFireBarrier);
  assembler_emit_short(assm, argc);
  assembler_adjust_stack_height(assm, -2);
  return success();
}

value_t assembler_emit_dispose_escape(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDisposeEscape);
  assembler_adjust_stack_height(assm, -get_genus_descriptor(dgEscapeSection)->field_count - 1);
  return success();
}

value_t assembler_emit_dispose_block(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDisposeBlock);
  assembler_adjust_stack_height(assm,
      -get_genus_descriptor(dgBlockSection)->field_count
      -1);
  return success();
}

value_t assembler_emit_stack_bottom(assembler_t *assm) {
  assembler_emit_opcode(assm, ocStackBottom);
  return success();
}

value_t assembler_emit_stack_piece_bottom(assembler_t *assm) {
  assembler_emit_opcode(assm, ocStackPieceBottom);
  return success();
}

value_t assembler_emit_invocation(assembler_t *assm, value_t fragment,
    value_t tags, value_t nexts) {
  CHECK_FAMILY_OPT(ofModuleFragment, fragment);
  CHECK_FAMILY(ofCallTags, tags);
  assembler_emit_opcode(assm, ocInvoke);
  TRY(assembler_emit_value(assm, tags));
  TRY(assembler_emit_value(assm, fragment));
  TRY(assembler_emit_value(assm, nexts));
  // The result will be pushed onto the stack on top of the arguments.
  assembler_adjust_stack_height(assm, 1);
  return success();
}

value_t assembler_emit_create_call_data(assembler_t *assm, size_t argc) {
  assembler_emit_opcode(assm, ocCreateCallData);
  assembler_emit_short(assm, argc);
  assembler_adjust_stack_height(assm,
      1            // the call data
      - 2 * argc); // the entries in the call data
  return success();
}

value_t assembler_emit_signal(assembler_t *assm, opcode_t opcode, value_t tags) {
  CHECK_FAMILY(ofCallTags, tags);
  assembler_emit_opcode(assm, opcode);
  TRY(assembler_emit_value(assm, tags));
  // Pad the instruction to give it the same length as the other invoke ops.
  assembler_emit_short(assm, 0);
  assembler_emit_short(assm, 0);
  // Do Not Adjust Your Stack Height.
  return success();
}

value_t assembler_emit_builtin(assembler_t *assm, builtin_implementation_t builtin) {
  TRY_DEF(wrapper, new_heap_void_p(assm->runtime, builtin));
  assembler_emit_opcode(assm, ocBuiltin);
  TRY(assembler_emit_value(assm, wrapper));
  // Pushes the result.
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_builtin_maybe_escape(assembler_t *assm,
    builtin_implementation_t builtin, size_t leave_argc,
    short_buffer_cursor_t *leave_offset_out) {
  TRY_DEF(wrapper, new_heap_void_p(assm->runtime, builtin));
  assembler_emit_opcode(assm, ocBuiltinMaybeEscape);
  TRY(assembler_emit_value(assm, wrapper));
  assembler_emit_cursor(assm, leave_offset_out);
  // Pad this op to be the same length as invoke ops since all ops that can
  // produce a backtrace entry should have the same length.
  assembler_emit_short(assm, 0);
  // The builting will either succeed and leave one value on the stack or fail
  // and leave argc signal params on the stack plus the appropriate invocation
  // record.
  assembler_adjust_stack_height_nocheck(assm, +1 + leave_argc);
  // The failure case jumps over this code so we'll only get here if the call
  // succeeded, in which case there's only one value on the stack so adjust
  // for that.
  assembler_adjust_stack_height(assm, -leave_argc);
  return success();
}

value_t assembler_emit_return(assembler_t *assm) {
  CHECK_EQ("invalid stack height", 1, assm->stack_height);
  TRY(assembler_emit_unchecked_return(assm));
  return success();
}

value_t assembler_emit_unchecked_return(assembler_t *assm) {
  assembler_emit_opcode(assm, ocReturn);
  return success();
}

value_t assembler_emit_set_reference(assembler_t *assm) {
  assembler_emit_opcode(assm, ocSetReference);
  // Pop the reference but not the value off the stack.
  assembler_adjust_stack_height(assm, -1);
  return success();
}

value_t assembler_emit_get_reference(assembler_t *assm) {
  assembler_emit_opcode(assm, ocGetReference);
  // There is no stack adjustment because the reference is popped off and the
  // value pushed on.
  return success();
}

value_t assembler_emit_new_reference(assembler_t *assm) {
  assembler_emit_opcode(assm, ocNewReference);
  // There is no stack adjustment because the value is popped off and the
  // reference pushed on.
  return success();
}

value_t assembler_emit_load_local(assembler_t *assm, size_t index) {
  assembler_emit_opcode(assm, ocLoadLocal);
  assembler_emit_short(assm, index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_refracted_local(assembler_t *assm, size_t index,
    size_t block_depth) {
  assembler_emit_opcode(assm, ocLoadRefractedLocal);
  assembler_emit_short(assm, index);
  assembler_emit_short(assm, block_depth);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_global(assembler_t *assm, value_t path,
    value_t fragment) {
  CHECK_FAMILY_OPT(ofModuleFragment, fragment);
  CHECK_FAMILY(ofPath, path);
  assembler_emit_opcode(assm, ocLoadGlobal);
  TRY(assembler_emit_value(assm, path));
  TRY(assembler_emit_value(assm, fragment));
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_argument(assembler_t *assm, size_t param_index) {
  assembler_emit_opcode(assm, ocLoadArgument);
  assembler_emit_short(assm, param_index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_raw_argument(assembler_t *assm, size_t eval_index) {
  assembler_emit_opcode(assm, ocLoadRawArgument);
  assembler_emit_short(assm, eval_index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_refracted_argument(assembler_t *assm,
    size_t param_index, size_t block_depth) {
  CHECK_REL("direct block argument read", block_depth, >, 0);
  assembler_emit_opcode(assm, ocLoadRefractedArgument);
  assembler_emit_short(assm, param_index);
  assembler_emit_short(assm, block_depth);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_lambda_capture(assembler_t *assm, size_t index) {
  assembler_emit_opcode(assm, ocLoadLambdaCapture);
  assembler_emit_short(assm, index);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_load_refracted_capture(assembler_t *assm, size_t index,
    size_t block_depth) {
  assembler_emit_opcode(assm, ocLoadRefractedCapture);
  assembler_emit_short(assm, index);
  assembler_emit_short(assm, block_depth);
  assembler_adjust_stack_height(assm, +1);
  return success();
}

value_t assembler_emit_lambda(assembler_t *assm, value_t methods,
    size_t capture_count) {
  assembler_emit_opcode(assm, ocLambda);
  TRY(assembler_emit_value(assm, methods));
  assembler_emit_short(assm, capture_count);
  // Pop off all the captures and push back the lambda.
  assembler_adjust_stack_height(assm, -capture_count+1);
  return success();
}

value_t assembler_emit_create_block(assembler_t *assm, value_t methods) {
  assembler_emit_opcode(assm, ocCreateBlock);
  TRY(assembler_emit_value(assm, methods));
  // Pop off all the captuers and push back the lambda.
  assembler_adjust_stack_height(assm,
      get_genus_descriptor(dgBlockSection)->field_count // The block section
      +1);                                // The block object
  return success();
}

value_t assembler_emit_create_ensurer(assembler_t *assm, value_t code_block) {
  assembler_emit_opcode(assm, ocCreateEnsurer);
  TRY(assembler_emit_value(assm, code_block));
  // Pop off all the captuers and push back the lambda.
  assembler_adjust_stack_height(assm, get_genus_descriptor(dgEnsureSection)->field_count + 1);
  return success();
}

value_t assembler_emit_call_ensurer(assembler_t *assm) {
  assembler_emit_opcode(assm, ocCallEnsurer);
  // Pad to make it invoke-length for the backtrace logic.
  assembler_emit_short(assm, 0);
  assembler_emit_short(assm, 0);
  assembler_emit_short(assm, 0);
  assembler_adjust_stack_height(assm,
      + 1);               // the return value from the shard
  return success();
}

value_t assembler_emit_dispose_ensurer(assembler_t *assm) {
  assembler_emit_opcode(assm, ocDisposeEnsurer);
  assembler_adjust_stack_height(assm,
      - get_genus_descriptor(dgEnsureSection)->field_count // the ensure section
      - 1                                       // the code shard pointer
      - 1);                                     // the result
  return success();
}

value_t assembler_emit_install_signal_handler(assembler_t *assm, value_t space,
    short_buffer_cursor_t *continue_offset_out) {
  assembler_emit_opcode(assm, ocInstallSignalHandler);
  TRY(assembler_emit_value(assm, space));
  assembler_emit_cursor(assm, continue_offset_out);
  assembler_adjust_stack_height(assm, get_genus_descriptor(dgSignalHandlerSection)->field_count + 1);
  return success();
}

value_t assembler_emit_uninstall_signal_handler(assembler_t *assm) {
  assembler_emit_opcode(assm, ocUninstallSignalHandler);
  assembler_adjust_stack_height(assm, -get_genus_descriptor(dgSignalHandlerSection)->field_count - 1);
  return success();
}
