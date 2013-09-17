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
  F(Builtin)                                                                   \
  F(DelegateToLambda)                                                          \
  F(Invoke)                                                                    \
  F(Lambda)                                                                    \
  F(LoadArgument)                                                              \
  F(LoadGlobal)                                                                \
  F(LoadLocal)                                                                 \
  F(LoadOuter)                                                                 \
  F(NewArray)                                                                  \
  F(Pop)                                                                       \
  F(Push)                                                                      \
  F(Return)                                                                    \
  F(Slap)

// The enum of all opcodes.
typedef enum {
  __ocFirst__ = -1
#define __DECLARE_OPCODE__(Name) , oc##Name
  ENUM_OPCODES(__DECLARE_OPCODE__)
#undef __DECLARE_OPCODE__
} opcode_t;


// Executes the given code block object, returning the result.
value_t run_code_block(runtime_t *runtime, value_t code);


#endif // _INTERP
