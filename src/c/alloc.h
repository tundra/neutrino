// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Higher-level allocation routines that allocate and initialize objects in a
// given heap.

#include "behavior.h"
#include "globals.h"
#include "heap.h"
#include "method.h"
#include "process.h"
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

// Allocates a new heap blob in the given runtime, if there is room, otherwise
// returns a signal to indicate an error. The result will contain a copy of the
// data in the given contents blob.
value_t new_heap_blob_with_data(runtime_t *runtime, blob_t *contents);

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

// Allocates a new heap array buffer in the given runtime with the given
// initial capacity.
value_t new_heap_array_buffer(runtime_t *runtime, size_t initial_capacity);

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

// Creates a new factory object backed by the given constructor function.
value_t new_heap_factory(runtime_t *runtime, factory_constructor_t *constr);

// Creates a new code block object with the given bytecode blob and value pool
// array.
value_t new_heap_code_block(runtime_t *runtime, value_t bytecode,
    value_t value_pool, size_t high_water_mark);

// Creates a new protocol object with the given display name.
value_t new_heap_protocol(runtime_t *runtime, value_t display_name);


// --- P r o c e s s ---

// Creates a new stack piece of the given size with the given previous segment.
value_t new_heap_stack_piece(runtime_t *runtime, size_t storage_size,
    value_t previous);

// Creates a new empty stack with one piece with the given capacity.
value_t new_heap_stack(runtime_t *runtime, size_t initial_capacity);


// --- M e t h o d ---

// Creates a new parameter guard.
value_t new_heap_guard(runtime_t *runtime, guard_type_t type, value_t value);

// Creates a new empty method space.
value_t new_heap_method_space(runtime_t *runtime);


// --- S y n t a x ---

// Creates a new literal syntax tree with the given value.
value_t new_heap_literal_ast(runtime_t *runtime, value_t value);

// Creates a new array syntax tree with the given element array.
value_t new_heap_array_ast(runtime_t *runtime, value_t elements);


// --- U t i l s ---

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

// Adds an element at the end of the given array buffer, expanding it to a new
// backing array if necessary. Returns a signal on failure.
value_t add_to_array_buffer(runtime_t *runtime, value_t buffer, value_t value);


#endif // _ALLOC
