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
  F(DelegateToLambda)                                                          \
  F(GetReference)                                                              \
  F(Invoke)                                                                    \
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
  F(Slap)

// The enum of all opcodes.
typedef enum {
  __ocFirst__ = -1
#define __DECLARE_OPCODE__(Name) , oc##Name
  ENUM_OPCODES(__DECLARE_OPCODE__)
#undef __DECLARE_OPCODE__
} opcode_t;


// Executes the given code block object, returning the result. If any signals
// occur evaluation is interrupted.
value_t run_code_block_until_signal(runtime_t *runtime, value_t code);

// Executes the given code block object, returning the result. This may cause
// the runtime to garbage collect.
value_t run_code_block(runtime_t *runtime, safe_value_t code);


#endif // _INTERP
