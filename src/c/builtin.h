// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Support for built-in methods.

#include "method.h"
#include "runtime.h"

#ifndef _BUILTIN
#define _BUILTIN

// Data that provides access to a built-in's arguments.
typedef struct {
  // The runtime we're running within.
  runtime_t *runtime;
  // The current stack frame.
  frame_t *frame;
} builtin_arguments_t;

// Initialize a built_in_arguments appropriately.
void builtin_arguments_init(builtin_arguments_t *args, runtime_t *runtime,
    frame_t *frame);

// Returns the index'th positional argument to a built-in method.
value_t get_builtin_argument(builtin_arguments_t *args, size_t index);

// Returns the "this" argument to a built-in method.
value_t get_builtin_this(builtin_arguments_t *args);

// Returns the runtime for this builtin invocation.
runtime_t *get_builtin_runtime(builtin_arguments_t *args);

// Signature of a function that implements a built-in method.
typedef value_t (*builtin_method_t)(builtin_arguments_t *args);

// Add a method to the given method space with the given receiver protocol,
// name, number of arguments, and implementation.
value_t add_method_space_builtin_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name, size_t arg_count, builtin_method_t method);

// Adds all built-in methods to the given method space.
value_t add_method_space_builtin_methods(runtime_t *runtime, value_t self);

#endif // _BUILTIN
