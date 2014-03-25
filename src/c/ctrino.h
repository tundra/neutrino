//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Ctrino is the proxy object that gives source code access to calling into the
// C runtime. It has no state of its own.

static const size_t kCtrinoSize = HEAP_OBJECT_SIZE(1);

// Adds the ctrino method to the given methodspace.
value_t add_ctrino_builtin_methods(runtime_t *runtime, value_t space);
