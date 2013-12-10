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

// Signature of a function that implements a built-in method.
typedef value_t (*builtin_method_t)(builtin_arguments_t *args);

// Describes the selector for a builtin method.
typedef struct builtin_operation_t {
  // The type.
  operation_type_t type;
  union {
    // The string value.
    const char *c_str;
    struct builtin_operation_t *nested;
  } value;
} builtin_operation_t;

// Macro that produces an infix builtin_operation_t.
#define INFIX(value) STRING_OPERATION(otInfix, value)

// Macro that produces a declaration of an infix builtin_operation_t.
#define DEF_INFIX(NAME, VALUE) DEF_STRING_OPERATION(NAME, otInfix, VALUE)

// Macro that produces a suffix builtin_operation_t.
#define SUFFIX(value) STRING_OPERATION(otSuffix, value)

// Macro that produces a prefix builtin_operation_t.
#define PREFIX(value) STRING_OPERATION(otPrefix, value)

// Macro that produces a declaration of an prefix builtin_operation_t.
#define DEF_PREFIX(NAME, VALUE) DEF_STRING_OPERATION(NAME, otPrefix, VALUE)

// Macro that produces an index builtin_operation_t.
#define INDEX() STRING_OPERATION(otIndex, NULL)

// Macro that produces a declaration of an index builtin_operation_t.
#define DEF_INDEX(NAME) DEF_STRING_OPERATION(NAME, otIndex, NULL)

// Macro that produces an assignment for the given nested operation.
#define ASSIGN(value) NESTED_OPERATION(otAssign, value)

// Macro that produces a builtin_operation_t with a nested operation argument.
#define DEF_ASSIGN(NAME, VALUE) DEF_NESTED_OPERATION(NAME, otAssign, VALUE)

// Macro that produces a call builtin_operation_t.
#define CALL() STRING_OPERATION(otCall, NULL)

// Macro that produces a declaration of a call builtin_operation_t.
#define DEF_CALL(NAME) DEF_STRING_OPERATION(NAME, otCall, NULL)

// Macro that produces a builtin_operation_t with a string value.
#define STRING_OPERATION(type, value) ((builtin_operation_t) {(type), {.c_str=(value)}})

// Macro that produces a declaration of a builtin_operation_t with a string
// value.
#define DEF_STRING_OPERATION(NAME, TYPE, VALUE)                                \
  builtin_operation_t NAME;                                                    \
  NAME.type = (TYPE);                                                          \
  NAME.value.c_str = (VALUE);

// Macro that produces a builtin_operation_t with a nested operation argument.
#define NESTED_OPERATION(type, value) ((builtin_operation_t) {                 \
  (type),                                                                      \
  {.nested=((builtin_operation_t[1]) {(value)})}})

// Macro that produces a declaration of a builtin_operation_t with a nested
// operation argument.
#define DEF_NESTED_OPERATION(NAME, TYPE, VALUE)                                \
  builtin_operation_t NAME;                                                    \
  NAME.type = (TYPE);                                                          \
  NAME.value.nested = &(VALUE);

// Returns an operation value based on the given description.
value_t builtin_operation_to_value(runtime_t *runtime, builtin_operation_t
    *operation);

// Add a method to the given method space with the given receiver type,
// name, number of arguments, and implementation.
value_t add_methodspace_builtin_method(runtime_t *runtime, value_t space,
    value_t receiver, builtin_operation_t operation, size_t arg_count,
    builtin_method_t method);

struct assembler_t;

// Signature of a function that implements a built-in method.
typedef value_t (*custom_method_emitter_t)(struct assembler_t *assm);

// Add a method to the given method space with the given receiver type,
// name, number of arguments, by delegating to the given emitter to generate
// the code.
value_t add_methodspace_custom_method(runtime_t *runtime, value_t space,
    value_t receiver, builtin_operation_t operation, size_t arg_count,
    bool allow_extra, custom_method_emitter_t emitter);

// Adds all built-in methods to the given method space.
value_t add_methodspace_builtin_methods(runtime_t *runtime, safe_value_t s_self);

#endif // _BUILTIN
