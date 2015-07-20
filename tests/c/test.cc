//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "behavior.h"
#include "freeze.h"
#include "runtime.h"
#include "tagged.h"
#include "try-inl.h"
#include "utils.h"
#include "utils/check.h"
#include "utils/crash.h"
#include "utils/log.h"
#include "utils/ook-inl.h"
#include "value-inl.h"
END_C_INCLUDES

#include <stdlib.h>

static bool array_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofArray, a);
  CHECK_FAMILY(ofArray, b);
  int64_t length = get_array_length(a);
  if (get_array_length(b) != length)
    return false;
  for (int64_t i = 0; i < length; i++) {
    if (!(value_structural_equal(get_array_at(a, i), get_array_at(b, i))))
      return false;
  }
  return true;
}

static bool array_buffer_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofArrayBuffer, a);
  CHECK_FAMILY(ofArrayBuffer, b);
  int64_t length = get_array_buffer_length(a);
  if (get_array_buffer_length(b) != length)
    return false;
  for (int64_t i = 0; i < length; i++) {
    if (!(value_structural_equal(get_array_buffer_at(a, i),
        get_array_buffer_at(b, i))))
      return false;
  }
  return true;
}

static bool id_hash_map_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofIdHashMap, a);
  CHECK_FAMILY(ofIdHashMap, b);
  if (get_id_hash_map_size(a) != get_id_hash_map_size(b))
    return false;
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, a);
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t a_value;
    id_hash_map_iter_get_current(&iter, &key, &a_value);
    value_t b_value = get_id_hash_map_at(b, key);
    if (in_condition_cause(ccNotFound, b_value))
      return false;
    if (!value_structural_equal(a_value, b_value))
      return false;
  }
  return true;
}

static bool instance_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofInstance, a);
  CHECK_FAMILY(ofInstance, b);
  return value_structural_equal(get_instance_fields(a), get_instance_fields(b));
}

static bool object_structural_equal(value_t a, value_t b) {
  CHECK_DOMAIN(vdHeapObject, a);
  CHECK_DOMAIN(vdHeapObject, b);
  heap_object_family_t a_family = get_heap_object_family(a);
  heap_object_family_t b_family = get_heap_object_family(b);
  if (a_family != b_family)
    return false;
  switch (a_family) {
    case ofArray:
      return array_structural_equal(a, b);
    case ofArrayBuffer:
      return array_buffer_structural_equal(a, b);
    case ofIdHashMap:
      return id_hash_map_structural_equal(a, b);
    case ofInstance:
      return instance_structural_equal(a, b);
    default:
      return value_identity_compare(a, b);
  }
}

bool value_structural_equal(value_t a, value_t b) {
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return false;
  switch (a_domain) {
    case vdHeapObject:
      return object_structural_equal(a, b);
    default:
      return value_identity_compare(a, b);
  }
}

static void recorder_abort_callback(abort_o *super_self, abort_message_t *message) {
  check_recorder_o *self = DOWNCAST(check_recorder_o, super_self);
  self->count++;
  self->last_cause = (condition_cause_t) message->condition_cause;
}

VTABLE(check_recorder_o, abort_o) { recorder_abort_callback };

void install_check_recorder(check_recorder_o *recorder) {
  recorder->count = 0;
  recorder->last_cause = __ccFirst__;
  VTABLE_INIT(check_recorder_o, UPCAST(recorder));
  recorder->previous = set_global_abort(UPCAST(recorder));
  CHECK_TRUE("no previous abort callback", recorder->previous != NULL);
}

void uninstall_check_recorder(check_recorder_o *recorder) {
  CHECK_TRUE("uninstalling again", recorder->previous != NULL);
  set_global_abort(recorder->previous);
  recorder->previous = NULL;
}

static bool log_validator_log(log_o *super_self, log_entry_t *entry) {
  log_validator_o *self = DOWNCAST(log_validator_o, super_self);
  self->count++;
  // Temporarily restore the previous log callback in case validation wants to
  // log (which it typically will on validation failure).
  set_global_log(self->previous);
  (self->validate_callback)(UPCAST(self), entry);
  set_global_log(UPCAST(self));
  return true;
}

VTABLE(log_validator_o, log_o) { log_validator_log };

void install_log_validator(log_validator_o *validator, log_m callback,
    void *data) {
  VTABLE_INIT(log_validator_o, UPCAST(validator));
  validator->count = 0;
  validator->validate_callback = callback;
  validator->validate_data = data;
  validator->previous = set_global_log(UPCAST(validator));
  CHECK_TRUE("no previous log callback", validator->previous != NULL);
}

void uninstall_log_validator(log_validator_o *validator) {
  CHECK_TRUE("uninstalling again", validator->previous != NULL);
  set_global_log(validator->previous);
  validator->previous = NULL;
}


// --- V a r i a n t s ---

// The size of an individual variant container block.
static const size_t kVariantContainerBlockSize = 1024;

// Moves the current block, which is now presumably exhausted, into the list
// of past blocks.
static void test_arena_retire_current_block(test_arena_t *arena) {
  if (arena->past_block_count == arena->past_block_capacity) {
    // If we've exhausted the past blocks array we double the capacity.
    size_t new_capacity = 2 * arena->past_block_capacity;
    if (new_capacity < 4)
      // The first time the old capacity will be 0 so we go directly to 4.
      new_capacity = 4;
    // Allocate a new past blocks array.
    blob_t new_memory = allocator_default_malloc(new_capacity * sizeof(blob_t));
    blob_t *new_blocks = (blob_t*) new_memory.start;
    // Copy the previous values.
    for (size_t i = 0; i < arena->past_block_capacity; i++)
      new_blocks[i] = arena->past_blocks[i];
    // Free the old array before we clobber the field.
    allocator_default_free(arena->past_blocks_memory);
    // Update the state of the container.
    arena->past_blocks_memory = new_memory;
    arena->past_blocks = new_blocks;
    arena->past_block_capacity = new_capacity;
  }
  arena->past_blocks[arena->past_block_count] = arena->current_block;
  arena->past_block_count++;
  arena->current_block = allocator_default_malloc(kVariantContainerBlockSize);
  arena->current_block_cursor = 0;
}

void *test_arena_malloc(test_arena_t *arena, size_t size) {
  CHECK_REL("variant block too big", size, <, kVariantContainerBlockSize);
  if ((arena->current_block_cursor + size) > arena->current_block.size)
    test_arena_retire_current_block(arena);
  byte_t *start = (byte_t*) arena->current_block.start;
  byte_t *result = start + arena->current_block_cursor;
  arena->current_block_cursor += size;
  return result;
}

// Copies va_args into an array, where each element has the given type.
// TODO: verify that this is really necessary and otherwise get rid of it.
#define COPY_ARRAY_ELEMENTS(TYPE) do {                                         \
  TYPE *elms = (TYPE*) mem;                                                    \
  for (size_t i = 0; i < size; i++)                                            \
    elms[i] = va_arg(ap, TYPE);                                                \
} while (false)

void *test_arena_copy_array(test_arena_t *arena, size_t size, size_t elmsize, ...) {
  void *mem = ARENA_MALLOC_ARRAY(arena, variant_t*, size);
  va_list ap;
  va_start(ap, elmsize);
  switch (elmsize) {
  case 4:
    COPY_ARRAY_ELEMENTS(int32_t);
    break;
  case 8:
    COPY_ARRAY_ELEMENTS(int64_t);
    break;
  default:
    UNREACHABLE("unexpected element size");
    break;
  }
  va_end(ap);
  return mem;
}

void test_arena_init(test_arena_t *arena) {
  arena->past_blocks_memory = blob_empty();
  arena->current_block = allocator_default_malloc(kVariantContainerBlockSize);
  arena->current_block_cursor = 0;
  arena->past_block_capacity = 0;
  arena->past_block_count = 0;
  arena->past_blocks = NULL;
}

void test_arena_dispose(test_arena_t *arena) {
  for (size_t i = 0; i < arena->past_block_count; i++) {
    blob_t past_block = arena->past_blocks[i];
    allocator_default_free(past_block);
  }
  allocator_default_free(arena->past_blocks_memory);
  allocator_default_free(arena->current_block);
}

variant_value_t var_array(test_arena_t *arena, size_t elmc, ...) {
  variant_value_t value;
  value.as_array.length = elmc;
  void *mem = ARENA_MALLOC_ARRAY(arena, variant_t*, elmc);
  value.as_array.elements = (variant_t**) mem;
  va_list ap;
  va_start(ap, elmc);
  for (size_t i = 0; i < elmc; i++)
    value.as_array.elements[i] = va_arg(ap, variant_t*);
  va_end(ap);
  return value;
}

value_t expand_variant_to_integer(runtime_t *runtime, variant_value_t *value) {
  return new_integer(value->as_int64);
}

value_t expand_variant_to_stage_offset(runtime_t *runtime, variant_value_t *value) {
  return new_stage_offset((int32_t) value->as_int64);
}

value_t expand_variant_to_string(runtime_t *runtime, variant_value_t *value) {
  return new_heap_utf8(runtime, new_c_string(value->as_c_str));
}

value_t expand_variant_to_infix(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(str, expand_variant_to_string(runtime, value));
  return new_heap_operation(runtime, afFreeze, otInfix, str);
}

value_t expand_variant_to_index(runtime_t *runtime, variant_value_t *value) {
  return new_heap_operation(runtime, afFreeze, otIndex, nothing());
}

value_t expand_variant_to_bool(runtime_t *runtime, variant_value_t *value) {
  return new_boolean(value->as_bool);
}

value_t expand_variant_to_null(runtime_t *runtime, variant_value_t *value) {
  return null();
}

value_t expand_variant_to_value(runtime_t *runtime, variant_value_t *value) {
  return value->as_value;
}

value_t expand_variant_to_array(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  TRY_DEF(result, new_heap_array(runtime, length));
  for (size_t i = 0; i < length; i++) {
    TRY_DEF(element, C(value->as_array.elements[i]));
    set_array_at(result, i, element);
  }
  return result;
}

value_t expand_variant_to_map(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  TRY_DEF(result, new_heap_id_hash_map(runtime, length));
  for (size_t i = 0; i < length; i += 2) {
    TRY_DEF(key, C(value->as_array.elements[i]));
    TRY_DEF(val, C(value->as_array.elements[i + 1]));
    TRY(set_id_hash_map_at(runtime, result, key, val));
  }
  return result;
}

value_t expand_variant_to_array_buffer(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(array, expand_variant_to_array(runtime, value));
  return new_heap_array_buffer_with_contents(runtime, array);
}

value_t expand_variant_to_path(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  value_t result = ROOT(runtime, empty_path);
  for (size_t i = 0; i < length; i++) {
    // The path has to be constructed backwards to the first element becomes
    // the head of the result, rather than the head of the end.
    TRY_DEF(head, C(value->as_array.elements[length - i - 1]));
    TRY_SET(result, new_heap_path(runtime, afMutable, head, result));
  }
  return result;
}

value_t expand_variant_to_identifier(runtime_t *runtime, variant_value_t *value) {
  CHECK_EQ("invalid identifier variant input", 2, value->as_array.length);
  TRY_DEF(stage, C(value->as_array.elements[0]));
  TRY_DEF(path, C(value->as_array.elements[1]));
  return new_heap_identifier(runtime, afFreeze, stage, path);
}

value_t expand_variant_to_signature(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(args, expand_variant_to_array(runtime, value));
  bool allow_extra = get_boolean_value(get_array_at(args, 0));
  size_t param_count = (size_t) get_array_length(args) - 1;
  size_t mandatory_count = 0;
  size_t tag_count = 0;
  // First we just collect some information, then we'll build the signature.
  for (size_t i = 0; i < param_count; i++) {
    value_t param = get_array_at(args, i + 1);
    value_t tags = get_parameter_tags(param);
    if (!get_parameter_is_optional(param))
      mandatory_count++;
    tag_count += (size_t) get_array_length(tags);
  }
  // Create an array with pairs of values, the first entry of which is the tag
  // and the second is the parameter.
  TRY_DEF(entries, new_heap_pair_array(runtime, tag_count));
  // Loop over all the tags, t being the tag index across the whole signature.
  size_t t = 0;
  for (size_t i = 0; i < param_count; i++) {
    value_t param = get_array_at(args, i + 1);
    CHECK_EQ("param index already set", 0, get_parameter_index(param));
    set_parameter_index(param, i);
    ensure_frozen(runtime, param);
    value_t tags = get_parameter_tags(param);
    for (int64_t j = 0; j < get_array_length(tags); j++, t++) {
      value_t tag = get_array_at(tags, j);
      set_pair_array_first_at(entries, t, tag);
      set_pair_array_second_at(entries, t, param);
    }
  }
  co_sort_pair_array(entries);
  return new_heap_signature(runtime, afFreeze, entries, param_count,
      mandatory_count, allow_extra);
}

value_t expand_variant_to_parameter(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(args, expand_variant_to_array(runtime, value));
  value_t guard = get_array_at(args, 0);
  bool is_optional = get_boolean_value(get_array_at(args, 1));
  value_t tags = get_array_at(args, 2);
  // The parameter is kept mutable such that the signature construction code
  // can set the index. Don't reuse parameters.
  return new_heap_parameter(runtime, afMutable, guard, tags, is_optional, 0);
}

value_t expand_variant_to_guard(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(args, expand_variant_to_array(runtime, value));
  guard_type_t type = (guard_type_t) get_integer_value(get_array_at(args, 0));
  switch (type) {
  case gtAny:
    return ROOT(runtime, any_guard);
  default:
    return new_heap_guard(runtime, afFreeze, type, get_array_at(args, 1));
  }
}

value_t variant_to_value(runtime_t *runtime, variant_t *variant) {
  return (variant->expander)(runtime, &variant->value);
}

bool advance_lexical_permutation(int64_t *elms, size_t elmc) {
  // This implementation follows an outline from wikipedia's article on
  // "Algorithms to generate permutations".
  bool found_k = false;
  // Find the largest k such that a[k] < a[k + 1].
  size_t k = 0;
  for (size_t k_plus_1 = elmc - 1; k_plus_1 > 0; k_plus_1--) {
    k = k_plus_1 - 1;
    if (elms[k] < elms[k + 1]) {
      found_k = true;
      break;
    }
  }
  if (!found_k)
    return false;
  // Find the largest l such that a[k] < a[l].
  size_t l = 0;
  for (size_t l_plus_1 = elmc; l_plus_1 > 0; l_plus_1--) {
    l = l_plus_1 - 1;
    if (elms[k] < elms[l])
      break;
  }
  // Swap the value of a[k] with that of a[l].
  int64_t temp = elms[k];
  elms[k] = elms[l];
  elms[l] = temp;
  // Reverse the sequence from a[k + 1] up to and including the final element
  // a[n].
  for (size_t i = 0; (elmc - i - 1) > (k + 1 + i); i++) {
    int64_t temp = elms[elmc - i - 1];
    elms[elmc - i - 1] = elms[k + 1 + i];
    elms[k + 1 + i] = temp;
  }
  return true;
}
