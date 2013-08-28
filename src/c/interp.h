// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "builtin.h"
#include "runtime.h"
#include "utils.h"
#include "value.h"

#ifndef _INTERP
#define _INTERP

// Invokes the given macro for each opcode name.
#define ENUM_OPCODES(F)                                                        \
  F(Push)                                                                      \
  F(Return)                                                                    \
  F(NewArray)                                                                  \
  F(Invoke)                                                                    \
  F(Builtin)                                                                   \
  F(Slap)                                                                      \
  F(Pop)                                                                       \
  F(LoadLocal)                                                                 \
  F(Lambda)                                                                    \
  F(DelegateToLambda)

// The enum of all opcodes.
typedef enum {
  __ocFirst__ = -1
#define __DECLARE_OPCODE__(Name) , oc##Name
  ENUM_OPCODES(__DECLARE_OPCODE__)
#undef __DECLARE_OPCODE__
} opcode_t;


// --- I n t e r p r e t e r ---

// Executes the given code block object, returning the result.
value_t run_code_block(runtime_t *runtime, value_t code);


// --- A s s e m b l e r ---

// Bytecode assembler data.
typedef struct assembler_t {
  // The runtime we're generating code within.
  runtime_t *runtime;
  // The buffer that holds the code being built.
  byte_buffer_t code;
  // The value pool map.
  value_t value_pool;
  // The current stack height.
  size_t stack_height;
  // The highest the stack has been at any point.
  size_t high_water_mark;
  // The method space being built.
  value_t space;
  // The map from local symbols to stack indexes.
  value_t local_variable_map;
} assembler_t;

// Initializes an assembler.
value_t assembler_init(assembler_t *assm, runtime_t *runtime, value_t space);

// Disposes of an assembler.
void assembler_dispose(assembler_t *assm);

// Returns a code block object containing the code written to this assembler.
value_t assembler_flush(assembler_t *assm);

// Emits a push instruction.
value_t assembler_emit_push(assembler_t *assm, value_t value);

// Emits a pop instruction. Pops count elements off the stack.
value_t assembler_emit_pop(assembler_t *assm, size_t count);

// Emits a new-array instruction that builds an array from the top 'length'
// elements.
value_t assembler_emit_new_array(assembler_t *assm, size_t length);

// Emits an invocation using the given record.
value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record);

// Emits a raw call to a builtin with the given implementation and number of
// arguments.
value_t assembler_emit_builtin(assembler_t *assm, builtin_method_t builtin);

// Emits a return instruction.
value_t assembler_emit_return(assembler_t *assm);

// Emits a local variable load of the local with the given index.
value_t assembler_emit_load_local(assembler_t *assm, size_t index);

// Emits a lambda that understands the given methods.
value_t assembler_emit_lambda(assembler_t *assm, value_t methods);

// Hacky implementation of calling lambdas. Later this should be replaced by a
// more general delegate operation.
value_t assembler_emit_delegate_lambda_call(assembler_t *assm);

// Adds a binding to the local variable map that records that the value of the
// given symbol can be found at the given stack index. The symbol must not
// already be bound so it's the caller's responsibility to ensure before calling
// that it isn't.
value_t assembler_bind_local_variable(assembler_t *assm, value_t symbol,
    size_t index);

// Removes the binding in the local variable map for the given symbol.
value_t assembler_unbind_local_variable(assembler_t *assm, value_t symbol);

// Returns true if this assembler currently has a binding for the given local
// variable symbol.
bool assembler_is_local_variable_bound(assembler_t *assm, value_t symbol);

// Returns the stack index of the local variable with the given symbol. The
// variable must be bound, it is the caller's responsibility to ensure that it
// is before calling this.
size_t assembler_get_local_variable_binding(assembler_t *assm, value_t symbol);

#endif // _INTERP
