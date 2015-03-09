//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _RUNTIME_INL
#define _RUNTIME_INL

#include "runtime.h"

// Expands to the body of a safe_ function that works by calling a delegate and
// if it fails garbage collecting and trying again once.
#define RETRY_ONCE_IMPL(RUNTIME, DELEGATE) do {                                \
  __GENERIC_RETRY_DEF__(P_FLAVOR, RUNTIME, value, DELEGATE, P_RETURN);         \
  return value;                                                                \
} while (false)

#endif // _RUNTIME_INL
