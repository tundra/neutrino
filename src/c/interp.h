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
  F(Builtin)

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
typedef struct {
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
} assembler_t;

// Initializes an assembler.
value_t assembler_init(assembler_t *assm, runtime_t *runtime, value_t space);

// Disposes of an assembler.
void assembler_dispose(assembler_t *assm);

// Returns a code block object containing the code written to this assembler.
value_t assembler_flush(assembler_t *assm);

// Emits a push instruction.
value_t assembler_emit_push(assembler_t *assm, value_t value);

// Emits a new-array instruction that builds an array from the top 'length'
// elements.
value_t assembler_emit_new_array(assembler_t *assm, size_t length);

// Emits an invocation using the given record.
value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record);

// Emits a raw call to a builtin with the given implementation and number of
// arguments.
value_t assembler_emit_builtin(assembler_t *assm, built_in_method_t builtin,
    size_t argc);

// Emits a return instruction.
value_t assembler_emit_return(assembler_t *assm);

#endif // _INTERP
