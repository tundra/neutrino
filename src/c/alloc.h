// Higher-level allocation routines that allocate and initialize objects in a
// given heap.

#include "behavior.h"
#include "globals.h"
#include "heap.h"
#include "runtime.h"
#include "utils.h"
#include "value.h"

#ifndef _ALLOC
#define _ALLOC

// Allocates a new heap string in the given runtime, if there is room, otherwise
// returns a signal to indicate an error.
value_t new_heap_string(runtime_t *runtime, string_t *contents);

// Allocates a new heap blob in the given runtime, if there is room, otherwise
// returns a signal to indicate an error. The result's data will be reset to
// all zeros.
value_t new_heap_blob(runtime_t *runtime, size_t length);

// Allocates a new species whose instances have the specified instance family.
value_t new_heap_species(runtime_t *runtime, object_family_t instance_family,
    behavior_t *behavior);

// Allocates a new heap array in the given runtime with room for the given
// number of elements. The array will be initialized to null.
value_t new_heap_array(runtime_t *runtime, size_t length);

// Creates a new identity hash map with the given initial capacity.
value_t new_heap_is_hash_map(runtime_t *runtime, size_t init_capacity);

// Creates the singleton null value.
value_t new_heap_null(runtime_t *runtime);

// Creates a boolean singleton.
value_t new_heap_bool(runtime_t *runtime, bool value);

// Allocates a new heap object in the given heap of the given size and
// initializes it with the given type but requires the caller to complete
// initialization.
value_t alloc_heap_object(heap_t *heap, size_t bytes, value_t species);

#endif // _ALLOC
