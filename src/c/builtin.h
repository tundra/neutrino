// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Support for built-in methods.

#include "method.h"
#include "runtime.h"

#ifndef _BUILTIN
#define _BUILTIN

// Data that provides access to a built-in's arguments.
typedef struct {
  // The current stack frame.
  frame_t *frame;
  // The offset from the top of the previous frame to the "this" argument.
  size_t this_offset;
} built_in_arguments_t;

// Initialize a built_in_arguments appropriately.
void built_in_arguments_init(built_in_arguments_t *args, frame_t *frame, size_t argc);

// Returns the index'th positional argument to a built-in method.
value_t get_builtin_argument(built_in_arguments_t *args, size_t index);

// Returns the "this" argument to a built-in method.
value_t get_builtin_this(built_in_arguments_t *args);

// Signature of a function that implements a built-in method.
typedef value_t (*built_in_method_t)(built_in_arguments_t *args);

// Add a method to the given method space with the given receiver protocol,
// name, number of arguments, and implementation.
value_t add_method_space_builtin_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name, size_t arg_count, built_in_method_t method);

// Adds all built-in methods to the given method space.
value_t add_method_space_builtin_methods(runtime_t *runtime, value_t self);

#endif // _BUILTIN
