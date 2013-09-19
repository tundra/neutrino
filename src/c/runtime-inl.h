// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "runtime.h"


#ifndef _RUNTIME_INL
#define _RUNTIME_INL

// Expands to the body of a safe_ function that works by calling a delegate and
// if it fails garbage collecting and trying again once.
#define RETRY_ONCE_IMPL(RUNTIME, DELEGATE) do {                                \
  value_t __result__ = (DELEGATE);                                             \
  if (is_signal(scHeapExhausted, __result__)) {                                \
    runtime_garbage_collect(RUNTIME);                                          \
    __result__ = (DELEGATE);                                                   \
    if (is_signal(scHeapExhausted, __result__))                                \
      return new_signal(scOutOfMemory);                                        \
  }                                                                            \
  return __result__;                                                           \
} while (false)

#endif // _RUNTIME_INL
