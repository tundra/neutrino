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

// Creates a new instance species with the specified primary protocol.
value_t new_heap_instance_species(runtime_t *runtime, value_t primary);

// Allocates a new heap array in the given runtime with room for the given
// number of elements. The array will be initialized to null.
value_t new_heap_array(runtime_t *runtime, size_t length);

// Allocates a new array that is going to be used as a pair array containing
// the given number of pairs.
value_t new_heap_pair_array(runtime_t *runtime, size_t length);

// Allocates a new heap array buffer in the given runtime with the given
// initial capacity.
value_t new_heap_array_buffer(runtime_t *runtime, size_t initial_capacity);

// Creates a new identity hash map with the given initial capacity.
value_t new_heap_id_hash_map(runtime_t *runtime, size_t init_capacity);

// Creates the singleton null value.
value_t new_heap_null(runtime_t *runtime);

// Creates the singleton nothing value.
value_t new_heap_nothing(runtime_t *runtime);

// Creates a boolean singleton.
value_t new_heap_boolean(runtime_t *runtime, bool value);

// Creates a new key with the given display name.
value_t new_heap_key(runtime_t *runtime, value_t display_name);

// Creates a new empty object instance with the given instance species.
value_t new_heap_instance(runtime_t *runtime, value_t species);

// Creates a new pointer wrapper around the given value.
value_t new_heap_void_p(runtime_t *runtime, void *value);

// Creates a new factory object backed by the given constructor function.
value_t new_heap_factory(runtime_t *runtime, factory_constructor_t *constr);

// Creates a new argument map trie with the given value and an empty children
// array.
value_t new_heap_argument_map_trie(runtime_t *runtime, value_t value);

// Creates a new code block object with the given bytecode blob and value pool
// array.
value_t new_heap_code_block(runtime_t *runtime, value_t bytecode,
    value_t value_pool, size_t high_water_mark);

// Creates a new protocol object with the given display name.
value_t new_heap_protocol(runtime_t *runtime, value_t display_name);

// Creates a new lambda value that supports the given method space methods.
value_t new_heap_lambda(runtime_t *runtime, value_t methods);

// Creates a new empty namespace object.
value_t new_heap_namespace(runtime_t *runtime);


// --- P r o c e s s ---

// Creates a new stack piece of the given size with the given previous segment.
value_t new_heap_stack_piece(runtime_t *runtime, size_t storage_size,
    value_t previous);

// Creates a new empty stack with one piece with the given capacity.
value_t new_heap_stack(runtime_t *runtime, size_t initial_capacity);


// --- M e t h o d ---

// Creates a new parameter guard.
value_t new_heap_guard(runtime_t *runtime, guard_type_t type, value_t value);

// Creates a new signature with the specified attributes.
value_t new_heap_signature(runtime_t *runtime, value_t tags, size_t param_count,
    size_t mandatory_count, bool allow_extra);

// Creates a new parameter with the specified attributes.
value_t new_heap_parameter(runtime_t *runtime, value_t guard, bool is_optional,
    size_t index);

// Creates a new empty method space.
value_t new_heap_method_space(runtime_t *runtime);

// Creates a new method with the given signature and implementation.
value_t new_heap_method(runtime_t *runtime, value_t signature, value_t code);

// Creates an invocation record with the given argument vector.
value_t new_heap_invocation_record(runtime_t *runtime, value_t argument_vector);


// --- S y n t a x ---

// Creates a new literal syntax tree with the given value.
value_t new_heap_literal_ast(runtime_t *runtime, value_t value);

// Creates a new array syntax tree with the given element array.
value_t new_heap_array_ast(runtime_t *runtime, value_t elements);

// Creates a new invocation syntax tree with the given arguments.
value_t new_heap_invocation_ast(runtime_t *runtime, value_t arguments);

// Creates a new argument syntax tree with the given tag and value.
value_t new_heap_argument_ast(runtime_t *runtime, value_t tag, value_t value);

// Creates a new sequence syntax tree with the given values.
value_t new_heap_sequence_ast(runtime_t *runtime, value_t values);

// Creates a new local declaration syntax tree with the given attributes.
value_t new_heap_local_declaration_ast(runtime_t *runtime, value_t symbol,
    value_t value, value_t body);

// Creates a new local variable syntax tree with the given symbol.
value_t new_heap_local_variable_ast(runtime_t *runtime, value_t symbol);

// Creates a new namespace variable syntax tree with the given name from the
// given namespace.
value_t new_heap_namespace_variable_ast(runtime_t *runtime, value_t name,
    value_t namespace);

// Creates a new symbol syntax tree with the given name.
value_t new_heap_symbol_ast(runtime_t *runtime, value_t name);

// Creates a new lambda syntax tree with the given attributes.
value_t new_heap_lambda_ast(runtime_t *runtime, value_t params, value_t body);

// Creates a new parameter syntax tree with the given attributes.
value_t new_heap_parameter_ast(runtime_t *runtime, value_t symbol, value_t tags);

// Creates a new program syntax tree with the given elements.
value_t new_heap_program_ast(runtime_t *runtime, value_t elements,
    value_t entry_point, value_t namespace);

// Creates a new toplevel namespace declaration syntax tree with the given name
// and value.
value_t new_heap_namespace_declaration_ast(runtime_t *runtime, value_t name,
    value_t value);

// Creates a new name syntax tree with the given path and phase.
value_t new_heap_name_ast(runtime_t *runtime, value_t path, value_t phase);


// --- U t i l s ---

// Allocates a new heap object in the given heap of the given size and
// initializes it with the given type but requires the caller to complete
// initialization.
value_t alloc_heap_object(heap_t *heap, size_t bytes, value_t species);

// Sets the given instance field to the given value, replacing the existing
// value if it already exists. Returns a signal on failure.
value_t set_instance_field(runtime_t *runtime, value_t instance, value_t key,
    value_t value);

// Adds an element at the end of the given array buffer, expanding it to a new
// backing array if necessary. Returns a signal on failure.
value_t add_to_array_buffer(runtime_t *runtime, value_t buffer, value_t value);


#endif // _ALLOC
