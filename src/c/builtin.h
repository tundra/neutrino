// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Support for built-in methods.


#ifndef _BUILTIN
#define _BUILTIN

#include "method.h"
#include "runtime.h"

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

// Returns the subject argument to a built-in method.
value_t get_builtin_subject(builtin_arguments_t *args);

// Returns the runtime for this builtin invocation.
runtime_t *get_builtin_runtime(builtin_arguments_t *args);

// Raises a signal, leaving execution. Typically you'll want to call this
// through the ESCAPE_BUILTIN macro.
value_t escape_builtin(builtin_arguments_t *args, value_array_t values);

// Convenience macro for escaping from a builtin. Builds the appropriate
// arguments to escape_builtin, calls it and returns the result.
#define __GEN_ESCAPE_ARG__(VAL) , (VAL)
#define ESCAPE_BUILTIN(ARGS, selector, ...) do {                               \
  builtin_arguments_t *__args__ = (ARGS);                                      \
  runtime_t *__runtime__ = get_builtin_runtime(__args__);                      \
  value_t values[VA_ARGC(__VA_ARGS__) + 2] = {                                 \
    null(),                                                                    \
    RSEL(__runtime__, selector)                                                \
    FOR_EACH_VA_ARG(__GEN_ESCAPE_ARG__, __VA_ARGS__)                           \
  };                                                                           \
  return escape_builtin(__args__,                                              \
      new_value_array(values, VA_ARGC(__VA_ARGS__) + 2));                      \
} while (false)

// Signature of a function that implements a built-in method.
typedef value_t (*builtin_method_t)(builtin_arguments_t *args);

// Add a builtin method implementation to the given map with the given name,
// number of arguments, and implementation.
value_t add_builtin_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t arg_count, builtin_method_t method,
    int leave_arg_count);

struct assembler_t;

// Signature of a function that implements a built-in method.
typedef value_t (*custom_method_emitter_t)(struct assembler_t *assm);

// Add a builtin method implementation to the given map with the given name and
// number of arguments by delegating to the given emitter to generate
// the code.
value_t add_custom_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t posc, value_t method_flags,
    custom_method_emitter_t emitter);

// Adds all the builtin method implementations to the given map.
value_t add_builtin_implementations(runtime_t *runtime, safe_value_t s_map);

#endif // _BUILTIN
