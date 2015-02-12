//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Memory management infrastructure.


#ifndef _HEAP
#define _HEAP

#include "globals.h"
#include "io/file.h"
#include "safe.h"
#include "sync/thread.h"
#include "utils.h"
#include "utils/ook.h"
#include "value.h"


static const byte_t kUnusedHeapMarker = 0xA4;
static const byte_t kAllocatedHeapMarker = 0xB4;
static const byte_t kFreedHeapMarker = 0xC4;

FORWARD(c_object_info_t);


// --- M i s c ---

INTERFACE(value_visitor_o);

// The type of a value callback function.
typedef value_t (*value_visitor_visit_m)(value_visitor_o *self, value_t value);

struct value_visitor_o_vtable_t {
  value_visitor_visit_m visit;
};

// A virtual visitor type that can be used to traverse values in the heap.
struct value_visitor_o {
  INTERFACE_HEADER(value_visitor_o);
};

// Invokes the given callback with the given value.
value_t value_visitor_visit(value_visitor_o *self, value_t value);


INTERFACE(field_visitor_o);

// The type of a field (pointer to value) function.
typedef value_t (*field_visitor_visit_m)(field_visitor_o *self, value_t *field);

struct field_visitor_o_vtable_t {
  field_visitor_visit_m visit;
};

// A callback along with a data pointer that can be used to iterate through a
// set of fields.
struct field_visitor_o {
  INTERFACE_HEADER(field_visitor_o);
};

// Invokes the given callback with the given value.
value_t field_visitor_visit(field_visitor_o *self, value_t *field);


// Settings to apply when creating a runtime. This struct gets passed by value
// under some circumstances so be sure it doesn't break anything to do that.
typedef struct {
  // The size in bytes of the space to create.
  size_t semispace_size_bytes;
  // The max amount of memory we'll allocate from the system. This is mainly a
  // failsafe in case a bug causes the runtime to allocate out of control, which
  // has happened, because the OS doesn't necessarily handle that very well.
  size_t system_memory_limit;
  // How often, on average, to simulate an allocation failure when fuzzing?
  size_t gc_fuzz_freq;
  // Random seed used to initialize the pseudo random generator used to
  // determine when to simulate a failure when fuzzing.
  size_t gc_fuzz_seed;
  // The plugins to install in runtimes created from this config.
  const c_object_info_t **plugins;
  size_t plugin_count;
  // The object that provides access to the file system. Null means use the
  // system default.
  file_system_t *file_system;
  // The seed used for the pseudo-random number generator used within this
  // runtime. Note that this seed is not the only source of nondeterminism in
  // a runtime so running the same program twice with the same seed will not
  // necessarily give the same result.
  uint64_t random_seed;
} runtime_config_t;

// Initializes the fields of this runtime config to the defaults. These defaults
// aren't necessarily appropriate for any particular use, they are just a set
// of well-defined values such that the config has at least been initialized
// with something. For any particular use you typically want to perform an
// additional initialization step appropriate for that use.
void runtime_config_init_defaults(runtime_config_t *config);

// Returns a pointer to a runtime config that holds the default values.
const runtime_config_t *runtime_config_get_default();

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
  memory_block_t memory;
} space_t;

// Initialize the given space, assumed to be uninitialized. If this fails for
// whatever reason a condition is returned.
value_t space_init(space_t *space, const runtime_config_t *config);

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
value_t space_for_each_object(space_t *space, value_visitor_o *visitor);

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
address_t align_address(address_arith_t alignment, address_t ptr);

// Returns true if the given address is within the given space.
bool space_contains(space_t *space, address_t addr);


// A full garbage-collectable heap.
typedef struct {
  // The space configuration this heap gets it settings from.
  runtime_config_t config;
  // The space where we allocate new objects.
  space_t to_space;
  // The space that, during gc, holds existing object and from which values are
  // copied into to-space.
  space_t from_space;
  // A the object trackers are kept in a linked list cycle where this node is
  // always linked in.
  object_tracker_t root_object_tracker;
  // The number of object trackers allocated.
  size_t object_tracker_count;
  // The thread that created this heap.
  native_thread_id_t creator_;
} heap_t;

// Initialize the given heap, returning a condition to indicate success or
// failure. If the config is NULL the default is used.
value_t heap_init(heap_t *heap, const runtime_config_t *config);

// Allocate the given number of byte in the given space. The size is not
// required to be value pointer aligned, this function will take care of
// that if necessary. If allocation is successful the result will be stored
// in the out argument and true returned; otherwise false will be returned.
bool heap_try_alloc(heap_t *heap, size_t size, address_t *memory_out);

// Invokes the given callback for each object in the heap.
value_t heap_for_each_object(heap_t *heap, value_visitor_o *visitor);

// Invokes the given callback for each object field in the space. It is safe to
// allocate new object while traversing the space, new objects will have their
// fields visited in order of allocation. The include_weak flag controls whether
// weak references are visited.
value_t heap_for_each_field(heap_t *heap, field_visitor_o *visitor,
    bool include_weak);

// Update the state of trackers post migration but before the gc has been
// finalized.
value_t heap_post_process_object_trackers(heap_t *heap);

// Returns true if the heap, in its current state, must be garbage collected
// before it can be disposed.
bool heap_collect_before_dispose(heap_t *heap);

// Dispose of the given heap. If there is a validation problem a condition will
// be returned but the heap will still be disposed, at least to the extent the
// problem allows.
value_t heap_dispose(heap_t *heap);

// Checks that the heap's data structures are consistent.
value_t heap_validate(heap_t *heap);

// Prepares this heap to be garbage collected by creating a new arena and swapping
// it in as the new allocation space.
value_t heap_prepare_garbage_collection(heap_t *heap);

// Wraps up an in-progress garbage collection by discarding from-space.
value_t heap_complete_garbage_collection(heap_t *heap);

// Creates a new object tracker that holds the specified value.
object_tracker_t *heap_new_heap_object_tracker(heap_t *heap, value_t value,
    uint32_t flags);

// Disposes an object tracker.
void heap_destroy_object_tracker(heap_t *heap, object_tracker_t *gc_safe);


// --- F i e l d   i t e r a t i o n ---

// Data for iterating through all the value fields in an object.
typedef struct {
  // Points to the next field to return.
  address_t next;
  // Points immediately past the end of the object's fields.
  address_t limit;
} value_field_iter_t;

// Initializes the iterator such that it is ready to scan through the fields
// of the given object. Note that the header is not counted as a field -- if
// you want to scan the header too then that has to be done separately.
void value_field_iter_init(value_field_iter_t *iter, value_t value);

// If the iterator has more fields stores the next field in the given out
// argument, advances the iterator, and returns true. Otherwise returns false.
bool value_field_iter_next(value_field_iter_t *iter, value_t **field_out);

// Returns the offset within the given object of the last field returned from
// this iterator.
size_t value_field_iter_offset(value_field_iter_t *iter, value_t value);

#endif // _HEAP
