// Higher-level allocation routines that allocate and initialize objects in a
// given heap.

#include "behavior.h"
#include "globals.h"
#include "heap.h"
#include "runtime.h"
#include "syntax.h"
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
value_t new_heap_compact_species(runtime_t *runtime, object_family_t instance_family,
    family_behavior_t *behavior);

// Allocates a new species whose instances have the specified instance family.
// The result will not be sanity checked.
value_t new_heap_compact_species_unchecked(runtime_t *runtime,
    object_family_t instance_family, family_behavior_t *behavior);

// Allocates a new heap array in the given runtime with room for the given
// number of elements. The array will be initialized to null.
value_t new_heap_array(runtime_t *runtime, size_t length);

// Creates a new identity hash map with the given initial capacity.
value_t new_heap_id_hash_map(runtime_t *runtime, size_t init_capacity);

// Creates the singleton null value.
value_t new_heap_null(runtime_t *runtime);

// Creates a boolean singleton.
value_t new_heap_bool(runtime_t *runtime, bool value);

// Creates a new empty object instance.
value_t new_heap_instance(runtime_t *runtime);

// Creates a new pointer wrapper around the given value.
value_t new_heap_void_p(runtime_t *runtime, void *value);


// --- S y n t a x ---

// Creates a new literal syntax tree with the given value.
value_t new_heap_literal(runtime_t *runtime, value_t value);

// Allocates a new heap object in the given heap of the given size and
// initializes it with the given type but requires the caller to complete
// initialization.
value_t alloc_heap_object(heap_t *heap, size_t bytes, value_t species);

// Adds a binding from the given key to the given value to this map, replacing
// the existing one if it already exists. Returns a signal on failure, either
// if the key cannot be hashed or there isn't enough memory in the runtime to
// extend the map.
value_t set_id_hash_map_at(runtime_t *runtime, value_t map, value_t key, value_t value);

// Sets the given instance field to the given value, replacing the existing
// value if it already exists. Returns a signal on failure.
value_t set_instance_field(runtime_t *runtime, value_t instance, value_t key,
    value_t value);

#endif // _ALLOC
