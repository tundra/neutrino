//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "heap.h"
#include "try-inl.h"
#include "utils/check.h"
#include "utils/ook-inl.h"
#include "value-inl.h"


// --- M i s c ---

value_t value_visitor_visit(value_visitor_o *self, value_t value) {
  return METHOD(self, visit)(self, value);
}

value_t field_visitor_visit(field_visitor_o *self, value_t *value) {
  return METHOD(self, visit)(self, value);
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
  1 * kMB,              // semispace_size_bytes
  100 * kMB,            // system_memory_limit
  0,                    // allocation_failure_fuzzer_frequency
  0,                    // allocation_failure_fuzzer_seed,
  NULL,                 // plugins
  0,                    // plugin_count
  NULL,                 // file_system
  0x9d5c326b950e060eULL // random_seed
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
    return new_system_error_condition(seAllocationFailed);
  // Clear the newly allocated memory to a recognizable value.
  memset(memory.memory, kUnusedHeapMarker, bytes);
  address_t aligned = align_address(kValueSize, (address_t) memory.memory);
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
    memset(addr, kAllocatedHeapMarker, aligned);
    *memory_out = addr;
    space->next_free = next;
    return true;
  } else {
    return false;
  }
}

bool space_contains(space_t *space, address_t addr) {
  CHECK_FALSE("space is empty", space_is_empty(space));
  return (((address_t) space->memory.memory) <= addr) && (addr < space->next_free);
}

value_t space_for_each_object(space_t *space, value_visitor_o *visitor) {
  address_t current = space->start;
  while (current < space->next_free) {
    value_t value = new_heap_object(current);
    TRY(value_visitor_visit(visitor, value));
    heap_object_layout_t layout;
    heap_object_layout_init(&layout);
    get_heap_object_layout(value, &layout);
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
  // Include weak references?
  bool include_weak;
} object_tracker_iter_t;

// Returns true if there is a current node to return, false if we've reached the
// end.
static bool object_tracker_iter_has_current(object_tracker_iter_t *iter) {
  return iter->current != iter->limit;
}

// Returns the current object tracker.
static object_tracker_t *object_tracker_iter_get_current(object_tracker_iter_t *iter) {
  CHECK_TRUE("object tracker iter get past end",
      object_tracker_iter_has_current(iter));
  CHECK_TRUE("non-weak iter returning weak",
      iter->include_weak || !object_tracker_is_weak(iter->current));
  return iter->current;
}

// Skip past any trackers we've been asked to ignore.
static void object_tracker_skip_ignored(object_tracker_iter_t *iter) {
  if (iter->include_weak)
    return;
  while (object_tracker_iter_has_current(iter) && object_tracker_is_weak(iter->current))
    // Advance by hand since iter_advance would call this one recursively.
    iter->current = iter->current->next;
}

// Initializes an object tracker iterator so that it's ready to iterate through
// all the handles in the given heap.
static void object_tracker_iter_init(object_tracker_iter_t *iter, heap_t *heap,
    bool include_weak) {
  iter->current = heap->root_object_tracker.next;
  iter->limit = &heap->root_object_tracker;
  iter->include_weak = include_weak;
  object_tracker_skip_ignored(iter);
}

// Advances the iterator to the next node.
static void object_tracker_iter_advance(object_tracker_iter_t *iter) {
  CHECK_TRUE("object tracker iter advance past end",
      object_tracker_iter_has_current(iter));
  iter->current = iter->current->next;
  object_tracker_skip_ignored(iter);
}

object_tracker_t *heap_new_heap_object_tracker(heap_t *heap, value_t value,
    uint32_t flags) {
  CHECK_FALSE("tracker for immediate", value_is_immediate(value));
  memory_block_t memory = allocator_default_malloc(sizeof(object_tracker_t));
  object_tracker_t *new_tracker = (object_tracker_t*) memory.memory;
  object_tracker_t *next = heap->root_object_tracker.next;
  object_tracker_t *prev = next->prev;
  new_tracker->value = value;
  new_tracker->flags = flags;
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
  CHECK_PTREQ("wrong tracker prev", tracker, prev->next);
  object_tracker_t *next = tracker->next;
  CHECK_PTREQ("wrong tracker next", tracker, next->prev);
  allocator_default_free(new_memory_block(tracker, sizeof(object_tracker_t)));
  prev->next = next;
  next->prev = prev;
  heap->object_tracker_count--;
}

value_t heap_validate(heap_t *heap) {
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap, true);
  object_tracker_t *prev = &heap->root_object_tracker;
  size_t trackers_seen = 0;
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    trackers_seen++;
    COND_CHECK_EQ("tracker validate", ccValidationFailed, prev->next, current);
    COND_CHECK_EQ("tracker validate", ccValidationFailed, current->prev, prev);
    prev = current;
    object_tracker_iter_advance(&iter);
  }
  COND_CHECK_EQ("tracker validate", ccValidationFailed, trackers_seen,
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
  heap->creator_ = native_thread_get_current_id();
  return success();
}

bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out) {
  IF_EXPENSIVE_CHECKS_ENABLED(CHECK_TRUE("accessing heap from other thread",
      native_thread_ids_equal(
          heap->creator_, native_thread_get_current_id())));
  return space_try_alloc(&heap->to_space, size, memory_out);
}

void heap_dispose(heap_t *heap) {
  CHECK_EQ("Leaking undisposed trackers", 0, heap->object_tracker_count);
  space_dispose(&heap->to_space);
  space_dispose(&heap->from_space);
}

value_t heap_for_each_object(heap_t *heap, value_visitor_o *visitor) {
  CHECK_FALSE("traversing empty space", space_is_empty(&heap->to_space));
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap, true);
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    TRY(value_visitor_visit(visitor, current->value));
    object_tracker_iter_advance(&iter);
  }
  return space_for_each_object(&heap->to_space, visitor);
}

IMPLEMENTATION(field_delegator_o, value_visitor_o);

struct field_delegator_o {
  IMPLEMENTATION_HEADER(field_delegator_o, value_visitor_o);
  field_visitor_o *field_visitor;
};

// Visitor method that invokes a field callback stored as data in the given value
// callback for each value field in the given object.
static value_t field_delegator_visit(value_visitor_o *super_self, value_t object) {
  field_delegator_o *self = DOWNCAST(field_delegator_o, super_self);
  // Visit the object's species first.
  value_t *header = access_heap_object_field(object, kHeapObjectHeaderOffset);
  // Check that the header isn't a forward pointer -- traversing a space that's
  // being migrated from doesn't work so all headers must be objects. We also
  // know they must be species but the heap may not be in a state that allows
  // us to easily check that.
  CHECK_DOMAIN(vdHeapObject, *header);
  field_visitor_visit(self->field_visitor, header);
  value_field_iter_t iter;
  value_field_iter_init(&iter, object);
  value_t *field;
  while (value_field_iter_next(&iter, &field))
    TRY(field_visitor_visit(self->field_visitor, field));
  return success();
}

VTABLE(field_delegator_o, value_visitor_o) { field_delegator_visit };

value_t heap_for_each_field(heap_t *heap, field_visitor_o *visitor,
    bool include_weak) {
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap, include_weak);
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    field_visitor_visit(visitor, &current->value);
    object_tracker_iter_advance(&iter);
  }
  field_delegator_o delegator;
  VTABLE_INIT(field_delegator_o, UPCAST(&delegator));
  delegator.field_visitor = visitor;
  return space_for_each_object(&heap->to_space, UPCAST(&delegator));
}

value_t heap_post_process_object_trackers(heap_t *heap) {
  object_tracker_iter_t iter;
  object_tracker_iter_init(&iter, heap, true);
  while (object_tracker_iter_has_current(&iter)) {
    object_tracker_t *current = object_tracker_iter_get_current(&iter);
    if (object_tracker_is_weak(current)) {
      value_t header = get_heap_object_header(current->value);
      if (get_value_domain(header) == vdMovedObject) {
        // This is a weak reference whose value is still alive. Update the
        // value ref since the first pass will have skipped this and hence it
        // hasn't been updated yet.
        current->value = get_moved_object_target(header);
      } else {
        // This is a weak reference whose object hasn't been moved so it must
        // be garbage; update the tracker's state accordingly.
        current->value = nothing();
        current->state |= tsGarbage;
      }
    }
    object_tracker_iter_advance(&iter);
  }
  return success();
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
  if (!is_heap_object(value)) {
    iter->limit = iter->next = NULL;
    return;
  }
  heap_object_layout_t layout;
  get_heap_object_layout(value, &layout);
  address_t object_start = get_heap_object_address(value);
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

size_t value_field_iter_offset(value_field_iter_t *iter, value_t value) {
  if (!is_heap_object(value))
    return 0;
  heap_object_layout_t layout;
  get_heap_object_layout(value, &layout);
  address_t start = get_heap_object_address(value);
  CHECK_PTREQ("iter offset using wrong value", iter->limit, start + layout.size);
  // We want the offset of the last value returned, not the next one, so we have
  // to go back one step from the current next.
  return (iter->next - start) - kValueSize;
}
