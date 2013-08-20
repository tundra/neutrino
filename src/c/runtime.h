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
  F(value, "value")                                                            \
  F(elements, "elements")                                                      \
  F(arguments, "arguments")                                                    \
  F(tag, "tag")                                                                \
  F(this, "this")                                                              \
  F(name, "name")

// A collection of all the root objects.
typedef struct {
  // Basic family species and protocols
#define __DECLARE_PER_SPECIES_ROOTS__(Family, family, CMP, CID, CNT, SUR, NOL) \
  value_t family##_species;                                                    \
  SUR(value_t family##_protocol;,)
  ENUM_OBJECT_FAMILIES(__DECLARE_PER_SPECIES_ROOTS__)
#undef __DECLARE_PER_SPECIES_ROOTS__
  // String->factory mapping.
  value_t syntax_factories;
  // Singletons
  value_t null;
  value_t thrue;
  value_t fahlse;
  value_t empty_array;
  value_t empty_array_buffer;
  value_t any_guard;
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


// All the data associated with a single VM instance.
struct runtime_t {
  // The heap where all the data lives.
  heap_t heap;
  // The root objects.
  roots_t roots;
};

// Initializes the given runtime according to the given config.
value_t runtime_init(runtime_t *runtime, space_config_t *config);

// Resets this runtime to a well-defined state such that if anything fails
// during the subsequent initialization all fields that haven't been
// initialized are sane.
void runtime_clear(runtime_t *runtime);

// Disposes of the given runtime. If disposing fails a signal is returned.
value_t runtime_dispose(runtime_t *runtime);

// Returns this runtime's null value.
value_t runtime_null(runtime_t *runtime);

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

#endif // _RUNTIME
