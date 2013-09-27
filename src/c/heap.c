// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "check.h"
#include "heap.h"
#include "try-inl.h"
#include "value-inl.h"


// --- M i s c ---

void value_callback_init(value_callback_t *callback, value_callback_function_t *function,
    void *data) {
  callback->function = function;
  callback->data = data;
}

value_t value_callback_call(value_callback_t *callback, value_t value) {
  return (callback->function)(value, callback);
}

void *value_callback_get_data(value_callback_t *callback) {
  return callback->data;
}

void field_callback_init(field_callback_t *callback, field_callback_function_t *function,
    void *data) {
  callback->function = function;
  callback->data = data;
}

value_t field_callback_call(field_callback_t *callback, value_t *value) {
  return (callback->function)(value, callback);
}

void *field_callback_get_data(field_callback_t *callback) {
  return callback->data;
}


// --- S p a c e ---

address_t align_address(address_arith_t alignment, address_t ptr) {
  address_arith_t addr = (address_arith_t) ptr;
  return (address_t) ((addr + (alignment - 1)) & ~(alignment - 1));
}

// Returns true if the given size value is aligned to the given boundary.
static size_t is_size_aligned(uint32_t alignment, size_t size) {
  return (size & (alignment - 1)) == 0;
}

// The default space config.
static const runtime_config_t kDefaultConfig = {
  1 * kMB,      // semispace_size_bytes
  100 * kMB,    // system_memory_limit
  0,            // allocation_failure_fuzzer_frequency
  0             // allocation_failure_fuzzer_seed
};

void runtime_config_init_defaults(runtime_config_t *config) {
  *config = *runtime_config_get_default();
}

const runtime_config_t *runtime_config_get_default() {
  return &kDefaultConfig;
}

value_t space_init(space_t *space, const runtime_config_t *config) {
  // Start out by clearing it, just for good measure.
  space_clear(space);
  // Allocate one word more than strictly necessary to account for possible
  // alignment.
  size_t bytes = config->semispace_size_bytes + kValueSize;
  memory_block_t memory = allocator_default_malloc(bytes);
  if (memory_block_is_empty(memory))
    return new_signal(scSystemError);
  // Clear the newly allocated memory to a recognizable value.
  memset(memory.memory, kBlankHeapMarker, bytes);
  address_t aligned = align_address(kValueSize, memory.memory);
  space->memory = memory;
  space->next_free = space->start = aligned;
  // If malloc gives us an aligned pointer using only 'size_bytes' of memory
  // wastes the extra word we allocated to make room for alignment. However,
  // making the space size slightly different depending on whether malloc
  // aligns its data or not is a recipe for subtle bugs.
  space->limit = space->next_free + config->semispace_size_bytes;
  return success();
}

void space_dispose(space_t *space) {
  if (memory_block_is_empty(space->memory))
    return;
  memset(space->memory.memory, kFreedHeapMarker, space->memory.size);
  allocator_default_free(space->memory);
  space_clear(space);
}

void space_clear(space_t *space) {
  space->next_free = NULL;
  space->limit = NULL;
  space->memory = memory_block_empty();
}

bool space_is_empty(space_t *space) {
  return space->next_free == NULL;
}

bool space_try_alloc(space_t *space, size_t size, address_t *memory_out) {
  CHECK_FALSE("allocating in empty space", space_is_empty(space));
  size_t aligned = align_size(kValueSize, size);
  address_t addr = space->next_free;
  address_t next = addr + aligned;
  if (next <= space->limit) {
    // Clear the newly allocated memory to a different value, again to make the
    // contents recognizable.
    memset(addr, kAllocedHeapMarker, aligned);
    *memory_out = addr;
    space->next_free = next;
    return true;
  } else {
    return false;
  }
}

value_t space_for_each_object(space_t *space, value_callback_t *callback) {
  address_t current = space->start;
  while (current < space->next_free) {
    value_t value = new_object(current);
    TRY(value_callback_call(callback, value));
    object_layout_t layout;
    object_layout_init(&layout);
    get_object_layout(value, &layout);
    size_t size = layout.size;
    CHECK_TRUE("object heap size alignment", is_size_aligned(kValueSize, size));
    current += size;
  }
  return success();
}

// --- G C   S a f e ---

// Data used when iterating object trackers within a heap.
typedef struct object_tracker_iter_t {
  // The current node being visited.
  object_tracker_t *current;
  // The node that indicates when we've reached the end.
  object_tracker_t *limit;
} object_tracker_iter_t;

// Initializes an object tracker iterator so that it's ready to iterate through
// all the handles in the given heap.
static void object_tracker_iter_init(object_tracker_iter_t *iter, heap_t *heap) {
  iter->current = heap->root_object_tracker.next;
  iter->limit = &heap->root_object_tracker;
}

// Returns true if there is a current node to return, false if we've reached the
// end.
static bool object_tracker_iter_has_current(object_tracker_iter_t *iter) {
  return iter->current != iter->limit;
}

// Returns the current object tracker.
static object_tracker_t *object_tracker_iter_get_current(object_tracker_iter_t *iter) {
  CHECK_TRUE("object tracker iter get past end",
      object_tracker_iter_has_current(iter));
  return iter->current;
}

// Advances the iterator to the next node.
static void object_tracker_iter_advance(object_tracker_iter_t *iter) {
  CHECK_TRUE("object tracker iter advance past end",
      object_tracker_iter_has_current(iter));
  iter->current = iter->current->next;
}

object_tracker_t *heap_new_object_tracker(heap_t *heap, value_t value) {
  CHECK_DOMAIN(vdObject, value);
  memory_block_t memory = allocator_default_malloc(sizeof(object_tracker_t));
  object_tracker_t *new_tracker = memory.memory;
  object_tracker_t *next = heap->root_object_tracker.next;
  object_tracker_t *prev = next->prev;
  new_tracker->value = value;
  new_tracker->next = next;
  new_tracker->prev = prev;
  prev->next = new_tracker;
  next->prev = new_tracker;
  heap->object_tracker_count++;
  return new_tracker;
}

void heap_dispose_object_tracker(heap_t *heap, object_tracker_t *tracker) {
  CHECK_REL("freed too many object trackers", heap->object_tracker_count, >, 0);
  object_tracker_t *prev = tracker->prev;
  CHECK_EQ("wrong tracker prev", tracker, prev->next);
  object_tracker_t *next = tracker->next;
  CHECK_EQ("wrong tracker next", tracker, next->prev);
  allocator_default_free(new_memory_block(tracker, sizeof(object_tracker_t)));
  prev->next = next;
  next->prev = prev;
  heap->object_tracker_count--;
}

value_t heap_validate(heap_t *heap) {
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap);
  object_tracker_t *prev = &heap->root_object_tracker;
  size_t trackers_seen = 0;
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    trackers_seen++;
    SIG_CHECK_EQ("tracker validate", scValidationFailed, prev->next, current);
    SIG_CHECK_EQ("tracker validate", scValidationFailed, current->prev, prev);
    prev = current;
    object_tracker_iter_advance(&iter);
  }
  SIG_CHECK_EQ("tracker validate", scValidationFailed, trackers_seen,
      heap->object_tracker_count);
  return success();
}


// --- H e a p ---

value_t heap_init(heap_t *heap, const runtime_config_t *config) {
  // Initialize new space, leave old space clear; we won't use that until
  // later.
  if (config == NULL)
    config = runtime_config_get_default();
  heap->config = *config;
  TRY(space_init(&heap->to_space, config));
  space_clear(&heap->from_space);
  // Initialize the object tracker loop using the dummy node.
  heap->root_object_tracker.next = heap->root_object_tracker.prev = &heap->root_object_tracker;
  heap->object_tracker_count = 0;
  return success();
}

bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out) {
  return space_try_alloc(&heap->to_space, size, memory_out);
}

void heap_dispose(heap_t *heap) {
  CHECK_EQ("Leaking undisposed trackers", 0, heap->object_tracker_count);
  space_dispose(&heap->to_space);
  space_dispose(&heap->from_space);
}

value_t heap_for_each_object(heap_t *heap, value_callback_t *callback) {
  CHECK_FALSE("traversing empty space", space_is_empty(&heap->to_space));
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap);
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    value_callback_call(callback, current->value);
    object_tracker_iter_advance(&iter);
  }
  return space_for_each_object(&heap->to_space, callback);
}

// Visitor method that invokes a field callback stored as data in the given value
// callback for each value field in the given object.
static value_t visit_object_fields(value_t object, value_callback_t *value_callback) {
  // Visit the object's species first.
  field_callback_t *field_callback = value_callback_get_data(value_callback);
  value_t *header = access_object_field(object, kObjectHeaderOffset);
  // Check that the header isn't a forward pointer -- traversing a space that's
  // being migrated from doesn't work so all headers must be objects. We also
  // know they must be species but the heap may not be in a state that allows
  // us to easily check that.
  CHECK_DOMAIN(vdObject, *header);
  field_callback_call(field_callback, header);
  value_field_iter_t iter;
  value_field_iter_init(&iter, object);
  value_t *field;
  while (value_field_iter_next(&iter, &field))
    TRY(field_callback_call(field_callback, field));
  return success();
}

value_t heap_for_each_field(heap_t *heap, field_callback_t *callback) {
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap);
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    field_callback_call(callback, &current->value);
    object_tracker_iter_advance(&iter);
  }
  value_callback_t object_field_visitor;
  value_callback_init(&object_field_visitor, visit_object_fields, callback);
  return space_for_each_object(&heap->to_space, &object_field_visitor);
}

value_t heap_prepare_garbage_collection(heap_t *heap) {
  CHECK_TRUE("from space not empty", space_is_empty(&heap->from_space));
  CHECK_FALSE("to space empty", space_is_empty(&heap->to_space));
  // Move to-space to from-space so we have a handle on it for later.
  heap->from_space = heap->to_space;
  // Reset to-space so we can use the fields again.
  space_clear(&heap->to_space);
  // Then create a new empty to-space.
  return space_init(&heap->to_space, &heap->config);
}

value_t heap_complete_garbage_collection(heap_t *heap) {
  CHECK_FALSE("from space empty", space_is_empty(&heap->from_space));
  CHECK_FALSE("to space empty", space_is_empty(&heap->to_space));
  space_dispose(&heap->from_space);
  return success();
}

void value_field_iter_init(value_field_iter_t *iter, value_t value) {
  if (get_value_domain(value) != vdObject) {
    iter->limit = iter->next = NULL;
    return;
  }
  object_layout_t layout;
  get_object_layout(value, &layout);
  address_t object_start = get_object_address(value);
  // The first address past this object.
  iter->limit = object_start + layout.size;
  // The address of the first value field (or, if there are no fields, the
  // object limit).
  iter->next = object_start + layout.value_offset;
}

bool value_field_iter_next(value_field_iter_t *iter, value_t **value_out) {
  if (iter->limit == iter->next) {
    return false;
  } else {
    *value_out = (value_t*) iter->next;
    iter->next += kValueSize;
    return true;
  }
}
