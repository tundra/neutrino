//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// The runtime structure which holds all the shared state of a single VM
// instance.


#ifndef _RUNTIME
#define _RUNTIME

#include "heap.h"
#include "serialize.h"

// Enumerates the string table strings that will be stored as easily accessible
// roots.
#define ENUM_STRING_TABLE(F)                                                   \
  F(allow_extra,                "allow_extra")                                 \
  F(annotations,                "annotations")                                 \
  F(arguments,                  "arguments")                                   \
  F(ast,                        "ast")                                         \
  F(bindings,                   "bindings")                                    \
  F(body,                       "body")                                        \
  F(core,                       "core")                                        \
  F(ctrino,                     "ctrino")                                      \
  F(default,                    "default")                                     \
  F(denominator,                "denominator")                                 \
  F(display_name,               "display_name")                                \
  F(elements,                   "elements")                                    \
  F(entry_point,                "entry_point")                                 \
  F(environment_reference,      "environment_reference")                       \
  F(escape,                     "escape")                                      \
  F(FlagElement,                "FlagElement")                                 \
  F(fragments,                  "fragments")                                   \
  F(guard,                      "guard")                                       \
  F(handlers,                   "handlers")                                    \
  F(imports,                    "imports")                                     \
  F(inheritance,                "inheritance")                                 \
  F(is_async,                   "is_async")                                    \
  F(is_mutable,                 "is_mutable")                                  \
  F(key,                        "key")                                         \
  F(libraries,                  "libraries")                                   \
  F(method,                     "method")                                      \
  F(methods,                    "methods")                                     \
  F(module_loader,              "module_loader")                               \
  F(module,                     "module")                                      \
  F(modules,                    "modules")                                     \
  F(name,                       "name")                                        \
  F(names,                      "names")                                       \
  F(namespace,                  "namespace")                                   \
  F(numerator,                  "numerator")                                   \
  F(on_exit,                    "on_exit")                                     \
  F(options,                    "options")                                     \
  F(origin,                     "origin")                                      \
  F(parameters,                 "parameters")                                  \
  F(path,                       "path")                                        \
  F(precision,                  "precision")                                   \
  F(selector,                   "selector")                                    \
  F(signature,                  "signature")                                   \
  F(stage,                      "stage")                                       \
  F(subject,                    "subject")                                     \
  F(subtype,                    "subtype")                                     \
  F(supertype,                  "supertype")                                   \
  F(symbol,                     "symbol")                                      \
  F(syntax,                     "syntax")                                      \
  F(tag,                        "tag")                                         \
  F(tags,                       "tags")                                        \
  F(target,                     "target")                                      \
  F(type,                       "type")                                        \
  F(value,                      "value")                                       \
  F(values,                     "values")

// Enumerates the string values of the infix operators in the runtime's selector
// table. Use sparingly since they're basically API names hardcoded in the
// runtime which is a Bad Thing(TM).
#define ENUM_SELECTOR_TABLE(F)                                                 \
  F(out_of_bounds,              "out_of_bounds")                               \
  F(no_such_field,              "no_such_field")                               \
  F(no_such_tag,                "no_such_tag")                                 \
  F(is_frozen,                  "is_frozen")                                   \
  F(promise_not_resolved,       "promise_not_resolved")

// Invokes the argument for each singleton root (that is, roots that are not
// generated from the family list).
#define ENUM_ROOT_SINGLETONS(F)                                                \
  F(any_guard)                                                                 \
  F(array_of_zero)                                                             \
  F(builtin_impls)                                                             \
  F(call_thunk_code_block)                                                     \
  F(ctrino_factory)                                                            \
  F(empty_array_buffer)                                                        \
  F(empty_array)                                                               \
  F(empty_code_block)                                                          \
  F(empty_instance_species)                                                    \
  F(empty_path)                                                                \
  F(escape_records)                                                            \
  F(integer_type)                                                              \
  F(is_async_key)                                                              \
  F(builtin_methodspace)                                                       \
  F(op_call)                                                                   \
  F(plankton_environment)                                                      \
  F(plugin_factories)                                                          \
  F(return_code_block)                                                         \
  F(selector_key)                                                              \
  F(special_imports)                                                           \
  F(stack_bottom_code_block)                                                   \
  F(stack_piece_bottom_code_block)                                             \
  F(subject_key)

// Enum where each entry corresponds to a field in the roots object. The naming
// convention is a bit odd, that's because we're going to be using these to name
// fields so the naming convention matches those, rather than a proper enum.
typedef enum {
  __rk_first__ = -1

  // The singleton values.
#define __EMIT_SINGLETON_ENUM__(name) , rk_##name
  ENUM_ROOT_SINGLETONS(__EMIT_SINGLETON_ENUM__)
#undef __EMIT_SINGLETON_ENUM__

  // Family-related roots.
  // Any arguments to selector macros must themselves be macros because
  // generating an enum value uses a comma which confuses the macro call.
#define __EMIT_FAMILY_TYPE__(family) , rk_##family##_type
#define __EMIT_COMPACT_SPECIES__(family) , rk_##family##_species
#define __EMIT_MODAL_SPECIES__(family) , rk_fluid_##family##_species,          \
  rk_mutable_##family##_species, rk_frozen_##family##_species,                 \
  rk_deep_frozen_##family##_species
#define __EMIT_PER_FAMILY_ENUMS__(Family, family, MD, SR, MINOR, N)            \
  MD(                                                                          \
    __EMIT_MODAL_SPECIES__(family),                                            \
    __EMIT_COMPACT_SPECIES__(family))                                          \
  SR(                                                                          \
    __EMIT_FAMILY_TYPE__(family),                                              \
    )
  ENUM_HEAP_OBJECT_FAMILIES(__EMIT_PER_FAMILY_ENUMS__)
#undef __EMIT_PER_FAMILY_ENUMS__
#undef __EMIT_MODAL_SPECIES__
#undef __EMIT_COMPACT_SPECIES__
#undef __EMIT_FAMILY_TYPE__

  // Phylum-related roots.
#define __EMIT_PHYLUM_TYPE__(phylum) , rk_##phylum##_type
#define __EMIT_PER_PHYLUM_ENUMS__(Phylum, phylum, CM, SR)                      \
  SR(                                                                          \
    __EMIT_PHYLUM_TYPE__(phylum),                                              \
    )
  ENUM_CUSTOM_TAGGED_PHYLUMS(__EMIT_PER_PHYLUM_ENUMS__)
#undef __EMIT_PER_PHYLUM_ENUMS__
#undef __EMIT_PHYLUM_TYPE__

  // The string table
#define __EMIT_STRING_TABLE_ENUM__(name, value) , rk_string_table_##name
  ENUM_STRING_TABLE(__EMIT_STRING_TABLE_ENUM__)
#undef __EMIT_STRING_TABLE_ENUM__

  // The selector table
#define __EMIT_SELECTOR_TABLE_ENUM__(name, value) , rk_selector_table_##name
  ENUM_SELECTOR_TABLE(__EMIT_SELECTOR_TABLE_ENUM__)
#undef __EMIT_SELECTOR_TABLE_ENUM__
, __rk_last__
} root_key_t;

// The total number of root entries.
#define kRootCount __rk_last__

static const size_t kRootsSize = HEAP_OBJECT_SIZE(kRootCount);

// Returns a pointer to the key'th root value.
static inline value_t *access_roots_entry_at(value_t roots, root_key_t key) {
  return access_heap_object_field(roots, HEAP_OBJECT_FIELD_OFFSET(key));
}

// Returns the specified root.
static inline value_t get_roots_entry_at(value_t roots, root_key_t key) {
  return *access_roots_entry_at(roots, key);
}

// Invokes the argument for each mutable root.
#define ENUM_MUTABLE_ROOTS(F)                                                  \
  F(argument_map_trie_root)

typedef enum {
  __mk_first__ = -1

  // The singleton values.
#define __EMIT_MUTABLE_ROOT_ENUM__(name) , mk_##name
  ENUM_MUTABLE_ROOTS(__EMIT_MUTABLE_ROOT_ENUM__)
#undef __EMIT_MUTABLE_ROOT_ENUM__

  , __mk_last__
} mutable_root_key_t;

// The total number of root entries.
#define kMutableRootCount __mk_last__

static const size_t kMutableRootsSize = HEAP_OBJECT_SIZE(kMutableRootCount);

// Returns a pointer to the key'th mutable root value.
static inline value_t *access_mutable_roots_entry_at(value_t roots,
    mutable_root_key_t key) {
  return access_heap_object_field(roots, HEAP_OBJECT_FIELD_OFFSET(key));
}

// Data associated with garbage collection fuzzing.
typedef struct {
  // Random number generator to use.
  pseudo_random_t random;
  // The smallest legal interval between allocation failures.
  size_t min_freq;
  // The range within which to pick random values.
  size_t spread;
  // The number of allocations remaining before the next forced failure.
  size_t remaining;
  // Is fuzzing currently enabled?
  bool is_enabled;
} gc_fuzzer_t;

// Initializes an garbage collection fuzzer according to the given runtime
// config.
void gc_fuzzer_init(gc_fuzzer_t *fuzzer, size_t min_freq, size_t mean_freq,
    size_t seed);

// Returns true if the next allocation should fail. This also advances the state
// of the fuzzer.
bool gc_fuzzer_tick(gc_fuzzer_t *fuzzer);


// All the data associated with a single VM instance.
struct runtime_t {
  // The heap where all the data lives.
  heap_t heap;
  // The root objects.
  value_t roots;
  // The volatile (mutable) roots.
  value_t mutable_roots;
  // The next key index.
  uint64_t next_key_index;
  // Optional allocation failure fuzzer.
  gc_fuzzer_t *gc_fuzzer;
  // Environment mapping to use when deserializing plankton.
  value_mapping_t plankton_mapping;
  // The module loader used by this runtime.
  safe_value_t module_loader;
  // The object that provides access to the file system. This value is never
  // null.
  file_system_t *file_system;
};

// Creates a new runtime object, storing it in the given runtime out parameter.
value_t new_runtime(runtime_config_t *config, runtime_t **runtime);

// Disposes the given runtime and frees the memory.
value_t delete_runtime(runtime_t *runtime);

// Initializes the given runtime according to the given config.
value_t runtime_init(runtime_t *runtime, const runtime_config_t *config);

// Resets this runtime to a well-defined state such that if anything fails
// during the subsequent initialization all fields that haven't been
// initialized are sane.
void runtime_clear(runtime_t *runtime);

// Disposes of the given runtime. If disposing fails a condition is returned.
value_t runtime_dispose(runtime_t *runtime);

// Collect garbage in the given runtime. If anything goes wrong, such as the os
// running out a memory, a condition will be returned.
value_t runtime_garbage_collect(runtime_t *runtime);

// Run a series of sanity checks on the runtime to check that it is consistent.
// Returns a condition iff something is wrong. A runtime will only validate if it
// has been initialized successfully. The cause is an optional value that
// identifies why the runtime is being validated. Can be useful for logging and
// debugging.
value_t runtime_validate(runtime_t *runtime, value_t cause);

// Creates a gc-safe reference to the given value.
safe_value_t runtime_protect_value(runtime_t *runtime, value_t value);

// Deserialize the given data using the environment bindings from this runtime.
value_t runtime_plankton_deserialize(runtime_t *runtime, value_t blob);

// Retrying version of runtime plankton deserialization.
value_t safe_runtime_plankton_deserialize(runtime_t *runtime, safe_value_t blob);

// Disposes a gc-safe reference.
void dispose_safe_value(runtime_t *runtime, safe_value_t value_s);

// Set whether fuzzing is on or off. If there is no fuzzer this has no effect.
void runtime_toggle_fuzzing(runtime_t *runtime, bool enable);

// Returns a modal species with the specified mode which is a sibling of the
// given value, that is, identical except having the specified mode.
// This will allow you to go from a more restrictive mode to a less restrictive
// one. If that's logically unsound you should check that it doesn't happen
// before calling this function.
value_t get_modal_species_sibling_with_mode(runtime_t *runtime, value_t species,
    value_mode_t mode);

// Returns the code block that implements the builtin method with the given name
// or a condition if the name is unknown.
value_t runtime_get_builtin_implementation(runtime_t *runtime, value_t name);

// Initialize this root set.
value_t roots_init(value_t roots, const runtime_config_t *config, runtime_t *runtime);

// Returns the key'th of the given runtime's roots.
static inline value_t get_runtime_root_at(runtime_t *runtime, root_key_t key) {
  return get_roots_entry_at(runtime->roots, key);
}

// Returns the instance factory that was created for the index'th entry in the
// plugin list used to create the given runtime.
value_t get_runtime_plugin_factory_at(runtime_t *runtime, size_t index);

// Accesses a named root directly in the given roots object. Usually you'll want
// to use ROOT instead.
#define RAW_ROOT(roots, name) (*access_roots_entry_at((roots), rk_##name))

// Macro for accessing a named root in the given runtime. For instance, to get
// empty_array you would do ROOT(runtime, empty_array). This can be used as an
// lval.
#define ROOT(runtime, name) RAW_ROOT((runtime)->roots, name)

// Accesses a string table string directly from the roots struct. Usually you'll
// want to use RSTR instead.
#define RAW_RSTR(roots, name) (*access_roots_entry_at((roots), rk_string_table_##name))

// Macro for accessing a named string table string. This can be used as an lval.
#define RSTR(runtime, name) RAW_RSTR((runtime)->roots, name)

// Accesses a selector table entry directly from the roots struct. Usually
// you'll want to use RSEL instead.
#define RAW_RSEL(roots, name) (*access_roots_entry_at((roots), rk_selector_table_##name))

// Macro for accessing a named selector table string. This can be used as an
// lval.
#define RSEL(runtime, name) RAW_RSEL((runtime)->roots, name)

// Accesses a named mutable root directly in the mutable roots object. Usually
// you'll want to use MROOT instead.
#define RAW_MROOT(roots, name) (*access_mutable_roots_entry_at((roots), mk_##name))

// Accesses the given named mutable root in the given runtime.
#define MROOT(runtime, name) RAW_MROOT((runtime)->mutable_roots, name)

#endif // _RUNTIME
