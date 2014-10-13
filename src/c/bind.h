//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Loading and binding code.


#ifndef _BIND
#define _BIND

#include "value-inl.h"

// --- B i n d i n g ---

// Encapsulates the data maintained during the binding process.
typedef struct {
  // Map from paths -> bound modules.
  value_t bound_module_map;
  // Map from paths -> stages -> entries where each entry describes a
  // corresponding fragment to be bound.
  value_t fragment_entry_map;
  // The ambience.
  value_t ambience;
  // Cache of the ambience's runtime.
  runtime_t *runtime;
} binding_context_t;

// Initializes a binding context appropriately.
void binding_context_init(binding_context_t *context, value_t ambience);

// Given an unbound module creates a bound version, loading and binding
// dependencies from the runtime's module loader as required.
value_t build_bound_module(value_t ambience, value_t unbound_module);

// Given an array buffer of modules, initialized the fragment_entry_map of
// the context. See bind.md for details.
value_t build_fragment_entry_map(binding_context_t *context, value_t modules);

// Given a context whose fragment entry map has been computed, returns an
// array buffer of identifiers that specify the load order to apply to satisfy
// the dependencies.
value_t build_binding_schedule(binding_context_t *context);


// --- M o d u l e   L o a d e r ---

static const size_t kModuleLoaderSize = HEAP_OBJECT_SIZE(1);
static const size_t kModuleLoaderModulesOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The path that identifies this module.
ACCESSORS_DECL(module_loader, modules);

// Configure this loader according to the given options object.
value_t module_loader_process_options(runtime_t *runtime, value_t self,
    value_t options);

// Looks up a module by path, returning an unbound module. If the loader doesn't
// know any modules with the given path NotFound is returned.
value_t module_loader_lookup_module(value_t self, value_t path);


// --- L i b r a r y ---

static const size_t kLibrarySize = HEAP_OBJECT_SIZE(2);
static const size_t kLibraryDisplayNameOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kLibraryModulesOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The name used to identify the library in logging etc.
ACCESSORS_DECL(library, display_name);

// The map from names to unbound modules.
ACCESSORS_DECL(library, modules);


// --- U n b o u n d   m o d u l e ---

static const size_t kUnboundModuleSize = HEAP_OBJECT_SIZE(2);
static const size_t kUnboundModulePathOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kUnboundModuleFragmentsOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The path that identifies this module.
ACCESSORS_DECL(unbound_module, path);

// The fragments that make up this module
ACCESSORS_DECL(unbound_module, fragments);


// --- U n b o u n d   m o d u l e   f r a g m e n t ---

static const size_t kUnboundModuleFragmentSize = HEAP_OBJECT_SIZE(3);
static const size_t kUnboundModuleFragmentStageOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kUnboundModuleFragmentImportsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kUnboundModuleFragmentElementsOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// Which stage does this fragment represent?
ACCESSORS_DECL(unbound_module_fragment, stage);

// List of this fragment's imports.
ACCESSORS_DECL(unbound_module_fragment, imports);

// The elements/declarations that implement this fragment.
ACCESSORS_DECL(unbound_module_fragment, elements);


#endif // _BIND
