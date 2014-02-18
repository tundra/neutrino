// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _RUNTIME_INL
#define _RUNTIME_INL

#include "runtime.h"


// Expands to the body of a safe_ function that works by calling a delegate and
// if it fails garbage collecting and trying again once.
#define RETRY_ONCE_IMPL(RUNTIME, DELEGATE) do {                                \
  value_t __result__ = (DELEGATE);                                             \
  if (in_condition_cause(ccHeapExhausted, __result__)) {                       \
    runtime_garbage_collect(RUNTIME);                                          \
    runtime_toggle_fuzzing(RUNTIME, false);                                    \
    __result__ = (DELEGATE);                                                   \
    runtime_toggle_fuzzing(RUNTIME, true);                                     \
    if (in_condition_cause(ccHeapExhausted, __result__))                       \
      return new_out_of_memory_condition();                                    \
  }                                                                            \
  return __result__;                                                           \
} while (false)

#endif // _RUNTIME_INL
