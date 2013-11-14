// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Loading and binding code.


#ifndef _BIND
#define _BIND

#include "value-inl.h"

// --- B i n d i n g ---

// Given an unbound module creates a bound version, loading and binding
// dependencies from the runtime's module loader as required.
value_t build_bound_module(runtime_t *runtime, value_t unbound_module);


// --- M o d u l e   L o a d e r ---

static const size_t kModuleLoaderSize = OBJECT_SIZE(1);
static const size_t kModuleLoaderModulesOffset = OBJECT_FIELD_OFFSET(0);

// The path that identifies this module.
ACCESSORS_DECL(module_loader, modules);

// Configure this loader according to the given options object.
value_t module_loader_process_options(runtime_t *runtime, value_t self,
    value_t options);


// --- L i b r a r y ---

static const size_t kLibrarySize = OBJECT_SIZE(2);
static const size_t kLibraryDisplayNameOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kLibraryModulesOffset = OBJECT_FIELD_OFFSET(1);

// The name used to identify the library in logging etc.
ACCESSORS_DECL(library, display_name);

// The map from names to unbound modules.
ACCESSORS_DECL(library, modules);


// --- U n b o u n d   m o d u l e ---

static const size_t kUnboundModuleSize = OBJECT_SIZE(2);
static const size_t kUnboundModulePathOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kUnboundModuleFragmentsOffset = OBJECT_FIELD_OFFSET(1);

// The path that identifies this module.
ACCESSORS_DECL(unbound_module, path);

// The fragments that make up this module
ACCESSORS_DECL(unbound_module, fragments);

// Returns the most recent fragment before the given stage, if that is well-
// defined, otherwise a NotFound signal.
value_t get_unbound_module_fragment_before(value_t self, value_t stage);


// --- U n b o u n d   m o d u l e   f r a g m e n t ---

static const size_t kUnboundModuleFragmentSize = OBJECT_SIZE(3);
static const size_t kUnboundModuleFragmentStageOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kUnboundModuleFragmentImportsOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kUnboundModuleFragmentElementsOffset = OBJECT_FIELD_OFFSET(2);

// Which stage does this fragment represent?
ACCESSORS_DECL(unbound_module_fragment, stage);

// List of this fragment's imports.
ACCESSORS_DECL(unbound_module_fragment, imports);

// The elements/declarations that implement this fragment.
ACCESSORS_DECL(unbound_module_fragment, elements);


#endif // _BIND
