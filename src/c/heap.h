// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Memory management infrastructure.

#include "globals.h"
#include "utils.h"
#include "value.h"

#ifndef _HEAP
#define _HEAP

struct value_callback_t;
struct field_callback_t;


static const byte_t kBlankHeapMarker = 0xBE;
static const byte_t kAllocedHeapMarker = 0xFA;
static const byte_t kFreedHeapMarker = 0xD0;


// --- M i s c ---

// The type of a value callback function.
typedef value_t (value_callback_function_t)(value_t value, struct value_callback_t *self);

// A callback along with a data pointer that can be used to iterate through a
// set of objects.
typedef struct value_callback_t {
  // The callback to invoke for each value.
  value_callback_function_t *function;
  // Some extra data accessible from the callback.
  void *data;
} value_callback_t;

// Initialize the given callback to call the given function with the given data.
void value_callback_init(value_callback_t *callback, value_callback_function_t *function,
    void *data);

// Invokes the given callback with the given value.
value_t value_callback_call(value_callback_t *callback, value_t value);

// Returns the data stored with this value callback.
void *value_callback_get_data(value_callback_t *callback);


// The type of a field (pointer to value) function.
typedef value_t (field_callback_function_t)(value_t *field, struct field_callback_t *self);

// A callback along with a data pointer that can be used to iterate through a
// set of fields.
typedef struct field_callback_t {
  // The callback to invoke.
  field_callback_function_t *function;
  // Some extra data accessible from the callback.
  void *data;
} field_callback_t;

// Initializes the given callback to call the given function with the given data.
void field_callback_init(field_callback_t *callback, field_callback_function_t *function,
    void *data);

// Invokes the given callback with the given value.
value_t field_callback_call(field_callback_t *callback, value_t *value);

// Returns the data stored with this field callback.
void *field_callback_get_data(field_callback_t *callback);


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
  // Address of the first object in this space.
  address_t start;
  // Next free address in this space. This will always be value pointer
  // aligned.
  address_t next_free;
  // First address past the end of this space. This may not be value pointer
  // aligned.
  address_t limit;
  // The memory to free when disposing this space. The start address may point
  // somewhere inside this memory so we can't free that directly.
  address_t memory;
  // The size of the allocated memory block.
  size_t memory_size;
  // The allocator to get memory from and return it to.
  allocator_t allocator;
} space_t;

// Initialize the given space, assumed to be uninitialized. If this fails for
// whatever reason a signal is returned.
value_t space_init(space_t *space, space_config_t *config);

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

// Invokes the given callback for each object in the space. It is safe to allocate
// new objects while traversing the space, new objects will be visited in order
// of allocation.
value_t space_for_each_object(space_t *space, value_callback_t *callback);

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
address_t align_address(address_arith_t alignment, address_t ptr);

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
size_t align_size(uint32_t alignment, size_t size);

// Opaque datatype that can be used to unpin a pinned value.
// Extra data associated with a gc-safe value. These handles form a doubly-linked
// list such that nodes can add and remove themselves from the chain of all safe
// values.
typedef struct gc_safe_t {
  // The next pin descriptor.
  struct gc_safe_t *next;
  // The previous pin descriptor.
  struct gc_safe_t *prev;
  // The pinned value.
  value_t value;
} gc_safe_t;

// Returns the value stored in the given gc-safe handle.
value_t gc_safe_get_value(gc_safe_t *handle);


// A full garbage-collectable heap.
typedef struct {
  // The space configuration this heap gets it settings from.
  space_config_t config;
  // The space where we allocate new objects.
  space_t to_space;
  // The space that, during gc, holds existing object and from which values are
  // copied into to-space.
  space_t from_space;
  // A the gc safe nodes are kept in a linked list cycle where this node is
  // always linked in.
  gc_safe_t root_gc_safe;
  // The number of gc safe handles allocated.
  size_t gc_safe_count;
} heap_t;

// Initialize the given heap, returning a signal to indicate success or
// failure. If the config is NULL the default is used.
value_t heap_init(heap_t *heap, space_config_t *config);

// Allocate the given number of byte in the given space. The size is not
// required to be value pointer aligned, this function will take care of
// that if necessary. If allocation is successful the result will be stored
// in the out argument and true returned; otherwise false will be returned.
bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out);

// Invokes the given callback for each object in the heap.
value_t heap_for_each_object(heap_t *heap, value_callback_t *callback);

// Invokes the given callback for each object field in the space. It is safe to
// allocate new object while traversing the space, new objects will have their
// fields visited in order of allocation.
value_t heap_for_each_field(heap_t *heap, field_callback_t *callback);

// Dispose of the given heap.
void heap_dispose(heap_t *heap);

// Checks that the heap's data structures are consistent.
value_t heap_validate(heap_t *heap);

// Prepares this heap to be garbage collected by creating a new arena and swapping
// it in as the new allocation space.
value_t heap_prepare_garbage_collection(heap_t *heap);

// Wraps up an in-progress garbage collection by discarding from-space.
value_t heap_complete_garbage_collection(heap_t *heap);

// Creates a new GC safe handle that holds the specified value.
gc_safe_t *heap_new_gc_safe(heap_t *heap, value_t value);

// Disposes a GC safe handle.
void heap_dispose_gc_safe(heap_t *heap, gc_safe_t *gc_safe);

#endif // _HEAP
