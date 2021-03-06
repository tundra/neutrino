//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Higher-level allocation routines that allocate and initialize objects in a
// given heap.


#ifndef _ALLOC
#define _ALLOC

#include "behavior.h"
#include "bind.h"
#include "ctrino.h"
#include "globals.h"
#include "heap.h"
#include "method.h"
#include "process.h"
#include "runtime.h"
#include "syntax.h"
#include "utils.h"
#include "value.h"

// A flags enum that indicates how to handle the allocated value.
typedef enum {
  // The object should be frozen before being returned.
  afFreeze,
  // The value should be left mutable.
  afMutable
} alloc_flags_t;

// Creates a new instance of the roots object. The result will have all fields,
// including the header, set to 0 because it's the very first object to be
// created and the values we need to complete initialization only exist later
// on.
value_t new_heap_uninitialized_roots(runtime_t *runtime);

// Creates a new instance of the mutable roots object. This gets created after
// the roots object has been initialized so the result is fully initialized.
value_t new_heap_mutable_roots(runtime_t *runtime);

// Allocates a new heap string in the given runtime, if there is room, otherwise
// returns a condition to indicate an error.
value_t new_heap_utf8(runtime_t *runtime, utf8_t contents);

// Allocates a new heap string of the given size whose contents are empty.
value_t new_heap_utf8_empty(runtime_t *runtime, size_t length);

// Returns a new ascii view on the given string.
value_t new_heap_ascii_string_view(runtime_t *runtime, value_t value);

// Allocates a new heap blob in the given runtime, if there is room, otherwise
// returns a condition to indicate an error. The result's data will be reset to
// all zeros.
value_t new_heap_blob(runtime_t *runtime, size_t length, alloc_flags_t flags);

// Allocates a new heap blob in the given runtime, if there is room, otherwise
// returns a condition to indicate an error. The result will contain a copy of the
// data in the given contents blob.
value_t new_heap_blob_with_data(runtime_t *runtime, blob_t contents);

// Allocates a new species whose instances have the specified instance family.
value_t new_heap_compact_species(runtime_t *runtime, family_behavior_t *behavior);

// Allocates a new modal species whose instances have the specified instance
// family which indicates that the instance is currently in the specified mode.
value_t new_heap_modal_species(runtime_t *runtime, family_behavior_t *behavior,
    value_mode_t mode, root_key_t base_root);

// Similar to new_heap_modal_species except doesn't sanity check the result
// on the way out. Should only ever be used during initialization since that is
// the only time there's good reason for sanity checking to fail.
value_t new_heap_modal_species_unchecked(runtime_t *runtime,
    family_behavior_t *behavior, value_mode_t mode, root_key_t base_root);

// Creates a new instance species with the specified primary type and instance
// manager.
value_t new_heap_instance_species(runtime_t *runtime, value_t primary,
    value_t manager, value_mode_t mode);

// Creates a new instance species whose state is taken from the given original.
// Note that if the derivatives array is set it will be shared between the
// original and the clone.
value_t clone_heap_instance_species(runtime_t *runtime, value_t original);

// Allocates a new heap array in the given runtime with room for the given
// number of elements. The array will be initialized to null.
value_t new_heap_array(runtime_t *runtime, int64_t length);

// Creates a new array that holds the given contents.
value_t new_heap_array_with_contents(runtime_t *runtime, alloc_flags_t flags,
    value_array_t contents);

// Returns a new reference that initially holds the given value.
value_t new_heap_reference(runtime_t *runtime, value_t value);

// Creates a new 2-element tuple. Currently backed by an array.
value_t new_heap_pair(runtime_t *runtime, value_t e0, value_t e1);

// Creates a new 3-element tuple. Currently backed by an array.
value_t new_heap_triple(runtime_t *runtime, value_t e0, value_t e1, value_t e2);

// Allocates a new array that is going to be used as a pair array containing
// the given number of pairs.
value_t new_heap_pair_array(runtime_t *runtime, int64_t length);

// Allocates a new heap array buffer in the given runtime with the given
// initial capacity.
value_t new_heap_array_buffer(runtime_t *runtime, size_t initial_capacity);

// Allocates a new heap fifo buffer in the given runtime with the given
// width and initial capacity.
value_t new_heap_fifo_buffer(runtime_t *runtime, size_t width,
    size_t initial_capacity);

// Allocates a new heap array buffer in the given runtime backed by the given
// array.
value_t new_heap_array_buffer_with_contents(runtime_t *runtime, value_t array);

// Creates a new identity hash map with the given initial capacity.
value_t new_heap_id_hash_map(runtime_t *runtime, size_t init_capacity);

// Creates and returns a new c-object species.
value_t new_heap_c_object_species(runtime_t *runtime, alloc_flags_t flags,
    const c_object_info_t *info, value_t type);

// Creates a new instance of the given c object species whose data is read from
// the given data pointer and values from the value pointer. The sizes must be
// equal to the sizes stored in the species.
value_t new_heap_c_object(runtime_t *runtime, alloc_flags_t flags,
    value_t species, blob_t init_data, value_array_t init_values);

// Creates a new key with the given display name.
value_t new_heap_key(runtime_t *runtime, value_t display_name);

// Creates a new empty object instance with the given instance species.
value_t new_heap_instance(runtime_t *runtime, value_t species);

// Creates a new instance manager object with the given display name.
value_t new_heap_instance_manager(runtime_t *runtime, value_t display_name);

// Creates a new pointer wrapper around the given value.
value_t new_heap_void_p(runtime_t *runtime, void *value);

// Creates a new factory object backed by the given constructor function.
value_t new_heap_factory(runtime_t *runtime, factory_new_instance_t *new_instance,
    factory_set_contents_t *set_contents, utf8_t name);

// Creates a new argument map trie with the given value and an empty children
// array.
value_t new_heap_argument_map_trie(runtime_t *runtime, value_t value);

// Creates a new code block object with the given bytecode blob and value pool
// array.
value_t new_heap_code_block(runtime_t *runtime, value_t bytecode,
    value_t value_pool, size_t high_water_mark);

// Creates a new type object with the given display name.
value_t new_heap_type(runtime_t *runtime, alloc_flags_t flags,
    value_t display_name);

// Creates a new function object with the given display name.
value_t new_heap_function(runtime_t *runtime, alloc_flags_t flags,
    value_t display_name);

// Creates a new lambda value that supports the given method space methods and
// that holds the given captured variables.
value_t new_heap_lambda(runtime_t *runtime, value_t methods, value_t captures);

// Creates a new block value whose state is located at the given location.
value_t new_heap_block(runtime_t *runtime, value_t section);

// Creates a new empty namespace object.
value_t new_heap_namespace(runtime_t *runtime, value_t value);

// Creates a new module fragment object.
value_t new_heap_module_fragment(runtime_t *runtime, value_t stage, value_t path,
    value_t predecessor, value_t nspace, value_t methodspace, value_t imports);

// Creates a new module fragment private access object.
value_t new_heap_module_fragment_private(runtime_t *runtime, value_t owner);

// Creates a new empty bound module with the given path.
value_t new_heap_empty_module(runtime_t *runtime, value_t path);

// Creates a new operation object.
value_t new_heap_operation(runtime_t *runtime, alloc_flags_t flags,
    operation_type_t type, value_t value);

// Creates a new path with the given head and tail.
value_t new_heap_path(runtime_t *runtime, alloc_flags_t flags, value_t head,
    value_t tail);

// Creates a new path with segments taken from the given array of names,
// starting from the given offset.
value_t new_heap_path_with_names(runtime_t *runtime, alloc_flags_t flags,
    value_t names, size_t offset);

// Creates a new seed with the given header and payload.
value_t new_heap_seed(runtime_t *runtime, value_t header, value_t payload);

// Creates a new module loader initialized with an empty module map.
value_t new_heap_empty_module_loader(runtime_t *runtime);

// Creates a new unbound module with the given path and fragments.
value_t new_heap_unbound_module(runtime_t *runtime, value_t path,
    value_t fragments);

// Creates a new unbound module fragment with the given attributes.
value_t new_heap_unbound_module_fragment(runtime_t *runtime, value_t stage,
    value_t imports, value_t elements);

// Creates a new library with the given display name and module map.
value_t new_heap_library(runtime_t *runtime, value_t display_name,
    value_t modules);

// Creates a new decimal fraction object.
value_t new_heap_decimal_fraction(runtime_t *runtime, value_t numerator,
    value_t denominator, value_t precision);

// Creates a new hard field object with the given display name.
value_t new_heap_hard_field(runtime_t *runtime, value_t display_name);

// Creates a new soft field object with the given display name.
value_t new_heap_soft_field(runtime_t *runtime, value_t display_name);

// Creates a new ambience object within the given runtime.
value_t new_heap_ambience(runtime_t *runtime);

// Creates a new freeze cheat object.
value_t new_heap_freeze_cheat(runtime_t *runtime, value_t value);

// Creates a new pending promise.
value_t new_heap_pending_promise(runtime_t *runtime);

// Returns a new hash source initialized with the given seed.
value_t new_heap_hash_source(runtime_t *runtime, uint64_t seed);

// Returns a new hash oracle backed by the given source.
value_t new_heap_hash_oracle(runtime_t *runtime, value_t stream);


// --- P r o c e s s ---

// Creates a new stack piece of the given size with the given previous segment.
value_t new_heap_stack_piece(runtime_t *runtime, size_t storage_size,
    value_t previous, value_t stack);

// Creates a new empty stack with one piece with the given capacity.
value_t new_heap_stack(runtime_t *runtime, size_t initial_capacity);

// Creates a new captured escape value.
value_t new_heap_escape(runtime_t *runtime, value_t section);

// Creates a new backtrace object with the given set of entries.
value_t new_heap_backtrace(runtime_t *runtime, value_t entries);

// Creates a new backtrace entry.
value_t new_heap_backtrace_entry(runtime_t *runtime, value_t invocation,
    value_t opcode);

// Creates a new empty process.
value_t new_heap_process(runtime_t *runtime);

// Creates a new uninitialized task belonging to the given process.
value_t new_heap_task(runtime_t *runtime, value_t process);

// Creates a new reified arguments value.
value_t new_heap_reified_arguments(runtime_t *runtime, value_t params,
    value_t values, value_t argmap, value_t tags);

// Creates a new native remote instance.
value_t new_heap_foreign_service(runtime_t *runtime, service_descriptor_t *impl);

// Creates a new exported service that delegates to the given handler in the
// given module.
value_t new_heap_exported_service(runtime_t *runtime, value_t process,
    value_t handler, value_t module);

// Creates a new incoming request thunk value.
value_t new_heap_incoming_request_thunk(runtime_t *runtime,
    value_t service, value_t request, value_t promise);


// --- M e t h o d ---

// Creates a new parameter guard.
value_t new_heap_guard(runtime_t *runtime, alloc_flags_t flags, guard_type_t type,
    value_t value);

// Creates a new signature with the specified attributes.
value_t new_heap_signature(runtime_t *runtime, alloc_flags_t flags, value_t tags,
    size_t param_count, size_t mandatory_count, bool allow_extra);

// Creates a new parameter with the specified attributes.
value_t new_heap_parameter(runtime_t *runtime, alloc_flags_t flags, value_t guard,
    value_t tags, bool is_optional, size_t index);

// Creates a new mutable empty signature map.
value_t new_heap_signature_map(runtime_t *runtime);

// Creates a new empty method space.
value_t new_heap_methodspace(runtime_t *runtime, value_t parent);

// Creates a new method with the given signature and implementation.
value_t new_heap_method(runtime_t *runtime, alloc_flags_t alloc_flags,
    value_t signature, value_t syntax, value_t code, value_t fragment,
    value_t method_flags);

// Creates a new call tags object with the given argument vector.
value_t new_heap_call_tags(runtime_t *runtime, alloc_flags_t flags,
    value_t entries);

// Creates a new call data object.
value_t new_heap_call_data(runtime_t *runtime, value_t tags, value_t values);

// Creates a new builtin marker corresponding to the builtin with the given
// name.
value_t new_heap_builtin_marker(runtime_t *runtime, value_t name);

// Creates a new builtin implementation object where the implementation is given
// by the given code object and whose surface binding must take exactly posc
// positional arguments.
value_t new_heap_builtin_implementation(runtime_t *runtime, alloc_flags_t flags,
    value_t name, value_t code, size_t posc, value_t method_flags);


// --- S y n t a x ---

// Creates a new literal syntax tree with the given value.
value_t new_heap_literal_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t value);

// Creates a new array syntax tree with the given element array.
value_t new_heap_array_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t elements);

// Creates a new invocation syntax tree with the given arguments.
value_t new_heap_invocation_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t arguments);

// Creates a new call literal syntax tree with the given arguments.
value_t new_heap_call_literal_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t arguments);

// Creates a new call literal argument.
value_t new_heap_call_literal_argument_ast(runtime_t *runtime,  alloc_flags_t flags,
    value_t tag, value_t value);

// Creates a new signal syntax tree with the given arguments.
value_t new_heap_signal_ast(runtime_t *runtime,  alloc_flags_t flags,
    value_t escape, value_t arguments, value_t defawlt);

// Creates a new signal handler syntax tree.
value_t new_heap_signal_handler_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t body,
    value_t handlers);

// Creates a new ensure syntax tree with the given components.
value_t new_heap_ensure_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t body, value_t on_exit);

// Creates a new argument syntax tree with the given tag and value.
value_t new_heap_argument_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t tag, value_t value, value_t next_guard);

// Creates a new sequence syntax tree with the given values.
value_t new_heap_sequence_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t values);

// Creates a new local declaration syntax tree with the given attributes.
value_t new_heap_local_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t is_mutable, value_t value, value_t body);

// Creates a new block syntax tree with the given attributes.
value_t new_heap_block_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t methods, value_t body);

// Creates a new with_escape syntax tree with the given attributes.
value_t new_heap_with_escape_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t body);

// Creates a new local variable syntax tree with the given symbol.
value_t new_heap_local_variable_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol);

// Creates a new assignment of the given value to the given variable.
value_t new_heap_variable_assignment_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t target, value_t value);

// Creates a new namespace variable syntax tree with the given name.
value_t new_heap_namespace_variable_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t ident);

// Creates a new symbol syntax tree with the given name and origin.
value_t new_heap_symbol_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t name, value_t origin);

// Creates a new lambda syntax tree with the given attributes.
value_t new_heap_lambda_ast(runtime_t *runtime, alloc_flags_t flags, value_t methods);

// Creates a new parameter syntax tree with the given attributes.
value_t new_heap_parameter_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t tags, value_t guard);

// Creates a new guard syntax tree with the given attributes.
value_t new_heap_guard_ast(runtime_t *runtime, alloc_flags_t flags,
    guard_type_t type, value_t value);

// Creates a new signature syntax tree with the given parameters.
value_t new_heap_signature_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t parameters, value_t allow_extra, value_t reified);

// Creates a new method ast with the given attributes.
value_t new_heap_method_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t signature, value_t body);

// Creates a new program syntax tree with the given elements.
value_t new_heap_program_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t entry_point, value_t module);

// Creates a new identifier with the given path and stage.
value_t new_heap_identifier(runtime_t *runtime, alloc_flags_t flags,
    value_t stage, value_t path);

// Creates a new namespace declaration syntax tree with the given path bound
// to the given name.
value_t new_heap_namespace_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t annotations, value_t path, value_t value);

// Creates a new method declaration that declares the given method.
value_t new_heap_method_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t annotations, value_t method);

// Creates a new is declaration that declares the given subtype to have the
// given supertype.
value_t new_heap_is_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t subtype, value_t supertype);

// Creates a new current module accessor ast.
value_t new_heap_current_module_ast(runtime_t *runtime);


/// ## Sync

// Creates a new native pipe value.
value_t new_heap_os_pipe(runtime_t *runtime);

// Creates a new out stream that wraps the given native stream and keeps the
// given lifeline object alive.
value_t new_heap_os_out_stream(runtime_t *runtime, out_stream_t *out, value_t lifeline);

// Creates a new in stream that wraps the given native stream and keeps the
// given lifeline object alive.
value_t new_heap_os_in_stream(runtime_t *runtime, in_stream_t *out, value_t lifeline);


// --- U t i l s ---

// Allocates a new heap object in the given runtime of the given size and
// initializes it with the given type but requires the caller to complete
// initialization.
//
// Note that if the value has finalization behavior you need to explicitly
// create an object tracker that finalizes it.
value_t alloc_heap_object(runtime_t *runtime, size_t bytes, value_t species);

// Creates and returns a clone of the given object. The contents of the object
// will be exactly the same as before so typically you don't want to use this on
// objects that contain derived pointers or that own values, unless you know
// that those owned values should be shared between the cloned instances.
value_t clone_heap_object(runtime_t *runtime, value_t original);

// Sets the given instance field to the given value, replacing the existing
// value if it already exists. Returns a condition on failure.
value_t set_instance_field(runtime_t *runtime, value_t instance, value_t key,
    value_t value);

// Returns a neutrino value that corresponds to the given plankton variant.
value_t import_pton_variant(runtime_t *runtime, pton_variant_t value);

// Gc-safe version of set_id_hash_map_at which will run a gc is the heap runs
// out of space.
value_t safe_set_id_hash_map_at(runtime_t *runtime, safe_value_t map,
    safe_value_t key, safe_value_t value);

// Run a couple of sanity checks before returning the value from a constructor.
// Returns a condition if the check fails, otherwise returns the given value.
value_t post_create_sanity_check(value_t value, size_t size);

#endif // _ALLOC
