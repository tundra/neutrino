// Higher-level allocation routines that allocate and initialize objects in a
// given heap.

#include "globals.h"
#include "heap.h"
#include "utils.h"
#include "value.h"

#ifndef _ALLOC
#define _ALLOC

// Allocates a new heap string in the given heap, if there is room, otherwise
// returns a signal to indicate an error.
value_ptr_t new_heap_string(heap_t *heap, string_t *contents);

// Allocates a new heap object in the given heap of the given size and
// initializes it to the given type but requires the caller to complete
// initialization.
value_ptr_t alloc_heap_object(heap_t *heap, size_t bytes, object_type_t type);

#endif // _ALLOC
