// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Support for built-in methods.

#include "runtime.h"

#ifndef _BUILTIN
#define _BUILTIN

typedef struct {

} built_in_arguments_t;

// Signature of a function that implements a built-in method.
typedef value_t (*built_in_method_t)(built_in_arguments_t *args);

// Add a method to the given method space with the given receiver protocol,
// name, number of arguments, and implementation.
value_t add_method_space_built_in_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name, size_t arg_count, built_in_method_t method);

// Adds all built-in methods to the given method space.
value_t add_method_space_built_in_methods(runtime_t *runtime, value_t self);

#endif // _BUILTIN
