// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// The runtime structure which holds all the shared state of a single VM
// instance.


#ifndef _RUNTIME
#define _RUNTIME

#include "heap.h"

// Enumerates the string table strings that will be stored as easily accessible
// roots.
#define ENUM_STRING_TABLE(F)                                                   \
  F(arguments,                  "arguments")                                   \
  F(ast,                        "ast")                                         \
  F(bindings,                   "bindings")                                    \
  F(body,                       "body")                                        \
  F(core,                       "core")                                        \
  F(display_name,               "display_name")                                \
  F(elements,                   "elements")                                    \
  F(entry_point,                "entry_point")                                 \
  F(environment_reference,      "environment_reference")                       \
  F(guard,                      "guard")                                       \
  F(imports,                    "imports")                                     \
  F(inheritance,                "inheritance")                                 \
  F(method,                     "method")                                      \
  F(methods,                    "methods")                                     \
  F(methodspace,                "methodspace")                                 \
  F(name,                       "name")                                        \
  F(names,                      "names")                                       \
  F(namespace,                  "namespace")                                   \
  F(options,                    "options")                                     \
  F(parameters,                 "parameters")                                  \
  F(path,                       "path")                                        \
  F(protocol,                   "protocol")                                    \
  F(selector,                   "selector")                                    \
  F(signature,                  "signature")                                   \
  F(stage,                      "stage")                                       \
  F(subject,                    "subject")                                     \
  F(symbol,                     "symbol")                                      \
  F(syntax,                     "syntax")                                      \
  F(tag,                        "tag")                                         \
  F(tags,                       "tags")                                        \
  F(type,                       "type")                                        \
  F(value,                      "value")                                       \
  F(values,                     "values")

// Invokes the argument for each singleton root (that is, roots that are not
// generated from the family list).
#define ENUM_ROOT_SINGLETONS(F)                                                \
  F(any_guard)                                                                 \
  F(builtin_methodspace)                                                       \
  F(ctrino)                                                                    \
  F(empty_array_buffer)                                                        \
  F(empty_array)                                                               \
  F(empty_instance_species)                                                    \
  F(empty_path)                                                                \
  F(fahlse)                                                                    \
  F(integer_protocol)                                                          \
  F(nothing)                                                                   \
  F(null)                                                                      \
  F(op_call)                                                                   \
  F(plankton_environment)                                                      \
  F(selector_key)                                                              \
  F(subject_key)                                                               \
  F(thrue)

// Enum where each entry corresponds to a field in the roots object. The naming
// convention is a bit odd, that's because we're going to be using these to name
// fields so the naming convention matches those, rather than a proper enum.
typedef enum {
  __rk_first__ = -1

  // The singleton values.
#define __EMIT_SINGLETON_ENUM__(name) , rk_##name
  ENUM_ROOT_SINGLETONS(__EMIT_SINGLETON_ENUM__)
#undef __EMIT_SINGLETON_ENUM__

  // Any arguments to selector macros must themselves be macros because
  // generating an enum value uses a comma which confuses the macro call.
#define __EMIT_FAMILY_PROTOCOL__(family) , rk_##family##_protocol
#define __EMIT_COMPACT_SPECIES__(family) , rk_##family##_species
#define __EMIT_MODAL_SPECIES__(family) , rk_fluid_##family##_species,          \
  rk_mutable_##family##_species, rk_frozen_##family##_species,                 \
  rk_deep_frozen_##family##_species
#define __EMIT_PER_FAMILY_ENUMS__(Family, family, CM, ID, CT, SR, NL, FU, EM, MD, OW) \
  MD(                                                                          \
    __EMIT_MODAL_SPECIES__(family),                                            \
    __EMIT_COMPACT_SPECIES__(family))                                          \
  SR(                                                                          \
    __EMIT_FAMILY_PROTOCOL__(family),                                          \
    )
  ENUM_OBJECT_FAMILIES(__EMIT_PER_FAMILY_ENUMS__)
#undef __EMIT_PER_FAMILY_ENUMS__
#undef __EMIT_MODAL_SPECIES__
#undef __EMIT_COMPACT_SPECIES__
#undef __EMIT_FAMILY_PROTOCOL__

  // The string table
#define __EMIT_STRING_TABLE_ENUM__(name, value) , rk_string_table_##name
  ENUM_STRING_TABLE(__EMIT_STRING_TABLE_ENUM__)
#undef __EMIT_STRING_TABLE_ENUM__
  , __rk_last__
} root_key_t;

// The total number of root entries.
#define kRootCount __rk_last__

static const size_t kRootsSize = OBJECT_SIZE(kRootCount);

// Returns a pointer to the key'th root value.
static inline value_t *access_roots_entry_at(value_t roots, root_key_t key) {
  return access_object_field(roots, OBJECT_FIELD_OFFSET(key));
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

static const size_t kMutableRootsSize = OBJECT_SIZE(kMutableRootCount);

// Returns a pointer to the key'th mutable root value.
static inline value_t *access_mutable_roots_entry_at(value_t roots,
    mutable_root_key_t key) {
  return access_object_field(roots, OBJECT_FIELD_OFFSET(key));
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

// Disposes of the given runtime. If disposing fails a signal is returned.
value_t runtime_dispose(runtime_t *runtime);

// Returns either this runtime's true or false value, depending on 'value'.
value_t runtime_bool(runtime_t *runtime, bool value);

// Collect garbage in the given runtime. If anything goes wrong, such as the os
// running out a memory, a signal will be returned.
value_t runtime_garbage_collect(runtime_t *runtime);

// Run a series of sanity checks on the runtime to check that it is consistent.
// Returns a signal iff something is wrong. A runtime will only validate if it
// has been initialized successfully.
value_t runtime_validate(runtime_t *runtime);

// Creates a gc-safe reference to the given value.
safe_value_t runtime_protect_value(runtime_t *runtime, value_t value);

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


// Initialize this root set.
value_t roots_init(value_t roots, runtime_t *runtime);

// Accesses a named root directly in the given roots object. Usually you'll want
// to your ROOT instead.
#define RAW_ROOT(roots, name) (*access_roots_entry_at((roots), rk_##name))

// Macro for accessing a named root in the given runtime. For instance, to get
// null you would do ROOT(runtime, null). This can be used as an lval.
#define ROOT(runtime, name) RAW_ROOT((runtime)->roots, name)

// Accesses a string table string directly from the roots struct. Usually you'll
// want to use RSTR instead.
#define RAW_RSTR(roots, name) (*access_roots_entry_at((roots), rk_string_table_##name))

// Macro for accessing a named string table string. This can be used as an lval.
#define RSTR(runtime, name) RAW_RSTR((runtime)->roots, name)

// Accesses a named mutable root directly in the mutable roots object. Usually
// you'll want to use MROOT instead.
#define RAW_MROOT(roots, name) (*access_mutable_roots_entry_at((roots), mk_##name))

// Accesses the given named mutable root in the given runtime.
#define MROOT(runtime, name) RAW_MROOT((runtime)->mutable_roots, name)

#endif // _RUNTIME
