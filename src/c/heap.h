// Memory management infrastructure.

#include "globals.h"
#include "utils.h"
#include "value.h"

#ifndef _HEAP
#define _HEAP

// Settings to apply when creating a space. This struct gets passed by value
// under some circumstances so be sure it doesn't break anything to do that.
typedef struct {
  // The size in bytes of the space to create.
  size_t size_bytes;
  // The allocator to use to get memory.
  allocator_t allocator;
} space_config_t;

// Initializes the fields of this space config to the defaults.
void space_config_init_defaults(space_config_t *config);

// An allocation space. The heap is made up of several of these.
typedef struct {
  // Next free address in this space. This will always be value pointer
  // aligned.
  address_t next_free;
  // First address past the end of this space. This may not be value pointer
  // aligned.
  address_t limit;
  // The memory to free when disposing this space. The start address may point
  // somewhere inside this memory so we can't free that directly.
  address_t memory;
  // The allocator to get memory from and return it to.
  allocator_t allocator;
} space_t;

// Initialize the given space, assumed to be uninitialized. If this fails for
// whatever reason a signal is returned.
value_ptr_t space_init(space_t *space, space_config_t *config);

// If necessary, dispose the memory held by this space.
void space_dispose(space_t *space);

// Clears out the fields of this space such that space_is_empty will return
// true when called on it.
void space_clear(space_t *space);

// Is this an empty space?
bool space_is_empty(space_t *space);

// Allocate the given number of byte in the given space. The size is not
// required to be value pointer aligned, this function will take care of
// that if necessary. If allocation is successful the result will be stored
// in the out argument and true returned; otherwise false will be returned.
bool space_try_alloc(space_t *space, size_t size, address_t *memory_out);

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
address_t align_address(uint32_t alignment, address_t ptr);

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
size_t align_size(uint32_t alignment, size_t size);


// A full garbage-collectable heap.
typedef struct {
  space_t new_space;
  space_t old_space;
} heap_t;

// Initialize the given heap, returning a signal to indicate success or
// failure. If the config is NULL the default is used.
value_ptr_t heap_init(heap_t *heap, space_config_t *config);

// Allocate the given number of byte in the given space. The size is not
// required to be value pointer aligned, this function will take care of
// that if necessary. If allocation is successful the result will be stored
// in the out argument and true returned; otherwise false will be returned.
bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out);

// Dispose of the given heap.
void heap_dispose(heap_t *heap);

#endif // _HEAP
