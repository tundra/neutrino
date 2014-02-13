// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _INTERP
#define _INTERP

#include "builtin.h"
#include "runtime.h"
#include "utils.h"
#include "value.h"

// Invokes the given macro for each opcode name.
#define ENUM_OPCODES(F)                                                        \
  F(Builtin)                                                                   \
  F(CheckStackHeight)                                                          \
  F(CaptureEscape)                                                             \
  F(DelegateToLambda)                                                          \
  F(FireEscape)                                                                \
  F(GetReference)                                                              \
  F(Invoke)                                                                    \
  F(KillEscape)                                                                \
  F(Lambda)                                                                    \
  F(LoadArgument)                                                              \
  F(LoadGlobal)                                                                \
  F(LoadLocal)                                                                 \
  F(LoadOuter)                                                                 \
  F(NewArray)                                                                  \
  F(NewReference)                                                              \
  F(Pop)                                                                       \
  F(Push)                                                                      \
  F(Return)                                                                    \
  F(SetReference)                                                              \
  F(Signal)                                                                    \
  F(Slap)                                                                      \
  F(StackBottom)                                                               \
  F(StackPieceBottom)

// The enum of all opcodes.
typedef enum {
  __ocFirst__ = -1
#define __DECLARE_OPCODE__(Name) , oc##Name
  ENUM_OPCODES(__DECLARE_OPCODE__)
#undef __DECLARE_OPCODE__
} opcode_t;

// The number of values in an invoke operation.
static const size_t kInvokeOperationSize = 4;

// Returns the string name of the opcode with the given index.
const char *get_opcode_name(opcode_t opcode);

// Executes the given code block object, returning the result. If any conditions
// occur evaluation is interrupted.
value_t run_code_block_until_condition(value_t ambience, value_t code);

// Executes the given code block object, returning the result. This may cause
// the runtime to garbage collect.
value_t run_code_block(safe_value_t s_ambience, safe_value_t s_code);


#endif // _INTERP
