// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// The runtime structure which holds all the shared state of a single VM
// instance.

#include "heap.h"

#ifndef _RUNTIME
#define _RUNTIME

// Enumerates the string table strings that will be stored as easily accessible
// roots.
#define ENUM_STRING_TABLE(F)                                                   \
  F(arguments,          "arguments")                                           \
  F(ast,                "ast")                                                 \
  F(bindings,           "bindings")                                            \
  F(body,               "body")                                                \
  F(core,               "core")                                                \
  F(elements,           "elements")                                            \
  F(entry_point,        "entry_point")                                         \
  F(inheritance,        "inheritance")                                         \
  F(methods,            "methods")                                             \
  F(methodspace,        "methodspace")                                         \
  F(name,               "name")                                                \
  F(namespace,          "namespace")                                           \
  F(parameters,         "parameters")                                          \
  F(path,               "path")                                                \
  F(phase,              "phase")                                               \
  F(sausages,           "()")                                                  \
  F(symbol,             "symbol")                                              \
  F(tag,                "tag")                                                 \
  F(tags,               "tags")                                                \
  F(value,              "value")                                               \
  F(values,             "values")

// A collection of all the root objects.
typedef struct {
  // Basic family species and protocols
#define __DECLARE_PER_SPECIES_ROOTS__(Family, family, CMP, CID, CNT, SUR, NOL, FIX, EMT) \
  value_t family##_species;                                                    \
  SUR(value_t family##_protocol;,)
  ENUM_OBJECT_FAMILIES(__DECLARE_PER_SPECIES_ROOTS__)
#undef __DECLARE_PER_SPECIES_ROOTS__
  value_t plankton_environment;
  // Singletons
  value_t null;
  value_t nothing;
  value_t thrue;
  value_t fahlse;
  value_t empty_array;
  value_t empty_array_buffer;
  value_t any_guard;
  value_t argument_map_trie_root;
  value_t subject_key;
  value_t selector_key;
  // Special protocols
  value_t integer_protocol;
  value_t empty_instance_species;
  // The string table
  struct {
#define __DECLARE_STRING_TABLE_ENTRY__(name, value) value_t name;
    ENUM_STRING_TABLE(__DECLARE_STRING_TABLE_ENTRY__)
#undef __DECLARE_STRING_TABLE_ENTRY__
  } string_table;
  // Basic family protocols.
} roots_t;


// Data associated with fuzzing allocation failures.
typedef struct {
  // Random number generator to use.
  pseudo_random_t random;
  // The smallest legal interval between allocation failures.
  size_t min_freq;
  // The range within which to pick random values.
  size_t spread;
  // The number of allocations remaining before the next forced failure.
  size_t remaining;
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
  roots_t roots;
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

// Creates a new gc-safe reference within the given runtime to the given value.
gc_safe_t *runtime_new_gc_safe(runtime_t *runtime, value_t value);

// Disposes a gc-safe value belonging to the given runtime.
void runtime_dispose_gc_safe(runtime_t *runtime, gc_safe_t *gc_safe);


// Initialize this root set.
value_t roots_init(roots_t *roots, runtime_t *runtime);

// Clears all the fields to a well-defined value.
void roots_clear(roots_t *roots);

// Checks that the roots are correctly initialized.
value_t roots_validate(roots_t *roots);

// Invokes the given field callback for each field in the set of roots.
value_t roots_for_each_field(roots_t *roots, field_callback_t *callback);

// Accesses a named root directly in the given roots struct. Usually you'll want
// to your ROOT instead.
#define RAW_ROOT(roots, name) ((roots)->name)

// Macro for accessing a named root in the given runtime. For instance, to get
// null you would do ROOT(runtime, null). This can be used as an lval.
#define ROOT(runtime, name) RAW_ROOT(&(runtime)->roots, name)

// Accesses a string table string directly from the roots struct. Usually you'll
// want to use RSTR instead.
#define RAW_RSTR(roots, name) ((roots)->string_table.name)

// Macro for accessing a named string table string. This can be used as an lval.
#define RSTR(runtime, name) RAW_RSTR(&(runtime)->roots, name)

#endif // _RUNTIME
