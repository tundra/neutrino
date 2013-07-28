// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE)

// The runtime structure which holds all the shared state of a single VM
// instance.

#include "heap.h"

#ifndef _RUNTIME
#define _RUNTIME

// Enumerates the string table strings that will be stored as easily accessible
// roots.
#define ENUM_STRING_TABLE(F)                                                   \
  F(value, "value")

// A collection of all the root objects.
typedef struct {
  // Basic family species.
#define __DECLARE_SPECIES_FIELD__(Family, family) value_t family##_species;
  ENUM_OBJECT_FAMILIES(__DECLARE_SPECIES_FIELD__)
#undef __DECLARE_SPECIES_FIELD__
  // String->factory mapping.
  value_t syntax_factories;
  // Singletons
  value_t null;
  value_t thrue;
  value_t fahlse;
  // The string table
  struct {
#define __DECLARE_STRING_TABLE_ENTRY__(name, value) value_t name;
    ENUM_STRING_TABLE(__DECLARE_STRING_TABLE_ENTRY__)
#undef __DECLARE_STRING_TABLE_ENTRY__
  } string_table;
} roots_t;


// All the data associated with a single VM instance.
typedef struct runtime_t {
  // The heap where all the data lives.
  heap_t heap;
  // The root objects.
  roots_t roots;
} runtime_t;

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


// Initialize this root set.
value_t roots_init(roots_t *roots, runtime_t *runtime);

// Clears all the fields to a well-defined value.
void roots_clear(roots_t *roots);

// Checks that the roots are correctly initialized.
value_t roots_validate(roots_t *roots);

// Invokes the given field callback for each field in the set of roots.
value_t roots_for_each_field(roots_t *roots, field_callback_t *callback);

#endif // _RUNTIME
