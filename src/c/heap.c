#include "heap.h"
#include "value.h"

// --- S p a c e ---

address_t align_address(uint32_t alignment, address_t ptr) {
  address_arith_t addr = (address_arith_t) ptr;
  return (address_t) ((addr + (alignment - 1)) & ~(alignment - 1));
}

size_t align_size(uint32_t alignment, size_t size) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}

void space_config_init_defaults(space_config_t *config) {
  config->size_bytes = 1 * kMB;
}

void space_init(space_t *space, space_config_t *config) {
  // Start out by clearing it, just for good measure.
  space_clear(space);
  // Allocate one word more than strictly necessary to account for possible
  // alignment.
  address_t memory = (address_t) malloc(config->size_bytes + kValuePtrAlignment);
  space->memory = memory;
  space->next_free = align_address(kValuePtrAlignment, memory);
  // If malloc gives us an aligned pointer using only 'size_bytes' of memory
  // wastes the extra word we allocated to make room for alignment. However,
  // making the space size slightly different depending on whether malloc
  // aligns its data or not is a recipe for subtle bugs.
  space->limit = space->next_free + config->size_bytes;
}

void space_dispose(space_t *space) {
  // Even if the space is empty it's safe to free NULL.
  free(space->memory);
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

bool space_alloc(space_t *space, size_t size, address_t *memory_out) {
  size_t aligned = align_size(kValuePtrAlignment, size);
  address_t addr = space->next_free;
  address_t next = addr + aligned;
  if (next <= space->limit) {
    *memory_out = addr;
    space->next_free = next;
    return true;
  } else {
    return false;
  }
}


// --- H e a p ---

void heap_init(heap_t *heap, space_config_t *config) {
  // Initialize new space, leave old space clear; we won't use that until
  // later.
  space_init(&heap->new_space, config);
  space_clear(&heap->old_space);
}

void heap_dispose(heap_t *heap) {
  space_dispose(&heap->new_space);
  space_dispose(&heap->old_space);
}
