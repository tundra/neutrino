// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Loading and binding code.


#ifndef _BIND
#define _BIND

#include "value-inl.h"

// --- M o d u l e   L o a d e r ---

static const size_t kModuleLoaderSize = OBJECT_SIZE(1);
static const size_t kModuleLoaderModulesOffset = OBJECT_FIELD_OFFSET(0);

// The path that identifies this module.
ACCESSORS_DECL(module_loader, modules);

// Configure this loader according to the given options object.
value_t module_loader_process_options(runtime_t *runtime, value_t self,
    value_t options);


// --- U n b o u n d   m o d u l e ---

static const size_t kUnboundModuleSize = OBJECT_SIZE(2);
static const size_t kUnboundModulePathOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kUnboundModuleFragmentsOffset = OBJECT_FIELD_OFFSET(1);

// The path that identifies this module.
ACCESSORS_DECL(unbound_module, path);

// The fragments that make up this module
ACCESSORS_DECL(unbound_module, fragments);

#endif // _BIND
