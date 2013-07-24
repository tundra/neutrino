#include "behavior.h"
#include "heap.h"
#include "value-inl.h"

#include <string.h>


// --- M i s c ---

void value_callback_init(value_callback_t *callback, value_callback_function_t *function,
    void *data) {
  callback->function = function;
  callback->data = data;
}

value_t value_callback_call(value_callback_t *callback, value_t value) {
  return (callback->function)(value, callback);
}


// --- S p a c e ---

static const byte_t kBlankHeapMarker = 0xBE;
static const byte_t kAllocedHeapMarker = 0xFA;

address_t align_address(uint32_t alignment, address_t ptr) {
  address_arith_t addr = (address_arith_t) ptr;
  return (address_t) ((addr + (alignment - 1)) & ~(alignment - 1));
}

size_t align_size(uint32_t alignment, size_t size) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

// Returns true if the given size value is aligned to the given boundary.
static size_t is_size_aligned(uint32_t alignment, size_t size) {
  return (size & (alignment - 1)) == 0;
}

// The default space config.
static const space_config_t kDefaultConfig = {
  1 * kMB,
  {NULL, NULL, NULL}
};

void space_config_init_defaults(space_config_t *config) {
  *config = kDefaultConfig;
}

value_t space_init(space_t *space, space_config_t *config_or_null) {
  const space_config_t *config = (config_or_null
      ? config_or_null
      : &kDefaultConfig);
  // Start out by clearing it, just for good measure.
  space_clear(space);
  if (config->allocator.malloc == NULL) {
    // If we haven't been given an allocator explicitly use the system one.
    init_system_allocator(&space->allocator);
  } else {
    // Otherwise copy the one we've been given into this space.
    space->allocator = config->allocator;
  }
  // Allocate one word more than strictly necessary to account for possible
  // alignment.
  size_t bytes = config->size_bytes + kValueSize;
  address_t memory = allocator_malloc(&space->allocator, bytes);
  if (memory == NULL)
    return new_signal(scSystemError);
  // Clear the newly allocated memory to a recognizable value.
  memset(memory, kBlankHeapMarker, bytes);
  space->memory = memory;
  space->next_free = space->start = align_address(kValueSize, memory);
  // If malloc gives us an aligned pointer using only 'size_bytes' of memory
  // wastes the extra word we allocated to make room for alignment. However,
  // making the space size slightly different depending on whether malloc
  // aligns its data or not is a recipe for subtle bugs.
  space->limit = space->next_free + config->size_bytes;
  return success();
}

void space_dispose(space_t *space) {
  if (space->memory == NULL)
    return;
  allocator_free(&space->allocator, space->memory);
  space_clear(space);
}

void space_clear(space_t *space) {
  space->next_free = NULL;
  space->limit = NULL;
  space->memory = NULL;
}

bool space_is_empty(space_t *space) {
  return space->next_free == NULL;
}

bool space_try_alloc(space_t *space, size_t size, address_t *memory_out) {
  size_t aligned = align_size(kValueSize, size);
  address_t addr = space->next_free;
  address_t next = addr + aligned;
  if (next <= space->limit) {
    // Clear the newly allocated memory to a different value, again to make the
    // contents recognizable.
    printf("%p %li %li\n", addr, size, aligned);
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


// --- H e a p ---

value_t heap_init(heap_t *heap, space_config_t *config) {
  // Initialize new space, leave old space clear; we won't use that until
  // later.
  TRY(space_init(&heap->new_space, config));
  space_clear(&heap->old_space);
  return success();
}

bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out) {
  return space_try_alloc(&heap->new_space, size, memory_out);
}

void heap_dispose(heap_t *heap) {
  space_dispose(&heap->new_space);
  space_dispose(&heap->old_space);
}

value_t heap_for_each_object(heap_t *heap, value_callback_t *callback) {
  return space_for_each_object(&heap->new_space, callback);
}
