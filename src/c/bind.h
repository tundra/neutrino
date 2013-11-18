// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Loading and binding code.


#ifndef _BIND
#define _BIND

#include "value-inl.h"

// --- B i n d i n g ---

// Encapsulates the data maintained during the binding process.
typedef struct {
  // Map from module names to bound modules.
  value_t modules;
  // The runtime.
  runtime_t *runtime;
} binding_context_t;

// Given an unbound module creates a bound version, loading and binding
// dependencies from the runtime's module loader as required.
value_t build_bound_module(runtime_t *runtime, value_t unbound_module);

// Given an array buffer of modules, returns a map describing the dependencies
// between the fragments. The keys of the map are the module's paths, the
// values are maps from fragment stage to a list of (path, stage) pairs that
// indicate which modules in which stages the fragment depends.
//
// The map may contain entries for fragments that don't exist. For instance,
// if stage 0 of module A imports B which has stages 0 and -1 there will be
// a dependency from stage -1 of A to -1 of B, even though -1 of A doesn't
// exist.
value_t build_fragment_dependency_map(runtime_t *runtime, value_t modules);

// Given a fragment dependency map and an array buffer of modules, returns an
// array buffer of (path, stage) pairs that specify the load order to apply
// to satify the dependencies.
value_t build_bind_schedule(runtime_t *runtime, value_t modules, value_t deps);


// --- M o d u l e   L o a d e r ---

static const size_t kModuleLoaderSize = OBJECT_SIZE(1);
static const size_t kModuleLoaderModulesOffset = OBJECT_FIELD_OFFSET(0);

// The path that identifies this module.
ACCESSORS_DECL(module_loader, modules);

// Configure this loader according to the given options object.
value_t module_loader_process_options(runtime_t *runtime, value_t self,
    value_t options);

// Looks up a module by path, returning an unbound module. If the loader doesn't
// know any modules with the given path NotFound is returned.
value_t module_loader_lookup_module(value_t self, value_t path);


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
