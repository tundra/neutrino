// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _INTERP
#define _INTERP

#include "builtin.h"
#include "runtime.h"
#include "utils.h"
#include "value.h"

// Invokes the given macro for each opcode name and argument count.
#define ENUM_OPCODES(F)                                                        \
  F(Builtin,                    2)                                             \
  F(CheckStackHeight,           2)                                             \
  F(CaptureEscape,              2)                                             \
  F(DelegateToLambda,           1)                                             \
  F(DelegateToBlock,            1)                                             \
  F(FireEscape,                 1)                                             \
  F(GetReference,               1)                                             \
  F(Invoke,                     4)                                             \
  F(KillEscape,                 1)                                             \
  F(KillBlock,                  1)                                             \
  F(Lambda,                     3)                                             \
  F(LoadArgument,               2)                                             \
  F(LoadGlobal,                 3)                                             \
  F(LoadLocal,                  2)                                             \
  F(LoadLambdaCapture,          2)                                             \
  F(LoadRefractedArgument,      3)                                             \
  F(LoadRefractedCapture,       3)                                             \
  F(LoadRefractedLocal,         3)                                             \
  F(Block,                      2)                                             \
  F(NewArray,                   2)                                             \
  F(NewReference,               1)                                             \
  F(Pop,                        2)                                             \
  F(Push,                       2)                                             \
  F(Return,                     1)                                             \
  F(SetReference,               1)                                             \
  F(Signal,                     4)                                             \
  F(Slap,                       2)                                             \
  F(StackBottom,                1)                                             \
  F(StackPieceBottom,           1)

// The enum of all opcodes.
typedef enum {
  __ocFirst__ = -1
#define __DECLARE_OPCODE__(Name, ARGC) , oc##Name
  ENUM_OPCODES(__DECLARE_OPCODE__)
#undef __DECLARE_OPCODE__
} opcode_t;

// Declare the opcode size constants.
#define __DECLARE_OPCODE_SIZE__(Name, ARGC)                                    \
  static const size_t k##Name##OperationSize = (ARGC);
  ENUM_OPCODES(__DECLARE_OPCODE_SIZE__)
#undef __DECLARE_OPCODE_SIZE__

// Returns the string name of the opcode with the given index.
const char *get_opcode_name(opcode_t opcode);

// Executes the given code block object, returning the result. If any conditions
// occur evaluation is interrupted.
value_t run_code_block_until_condition(value_t ambience, value_t code);

// Executes the given code block object, returning the result. This may cause
// the runtime to garbage collect.
value_t run_code_block(safe_value_t s_ambience, safe_value_t s_code);


#endif // _INTERP
