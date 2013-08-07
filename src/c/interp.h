// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "runtime.h"
#include "utils.h"
#include "value.h"

#ifndef _INTERP
#define _INTERP

// Invokes the given macro for each opcode name.
#define ENUM_OPCODES(F)                                                        \
  F(Literal)                                                                   \
  F(Return)

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
} assembler_t;

// Initializes an assembler.
value_t assembler_init(assembler_t *assm, runtime_t *runtime);

// Disposes of an assembler.
void assembler_dispose(assembler_t *assm);

// Returns a code block object containing the code written to this assembler.
value_t assembler_flush(assembler_t *assm);

// Writes an opcode to this assembler.
void assembler_emit_opcode(assembler_t *assm, opcode_t opcode);

// Writes a reference to a value in the value pool, adding the value to the
// pool if necessary.
value_t assembler_emit_value(assembler_t *assm, value_t value);

#endif // _INTERP
