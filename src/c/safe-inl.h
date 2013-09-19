// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Inline methods for working with safe values.

#include "safe.h"
#include "value.h"


#ifndef _SAFE_INL
#define _SAFE_INL

// Returns true if the value of the given safe value protects a value that is in
// the specified domain.
static inline bool safe_value_in_domain(value_domain_t domain, safe_value_t s_value) {
  return get_value_domain(s_value.as_value) == domain;
}

// Stack-allocates a new safe value pool with room for N values. A pointer to
// pool is stored in a new variable called 'name'. Storing the pointer instead
// of the stack-allocated value simplifies things because absolutely everything
// you use to interact with a pool requires a pointer.
#define CREATE_SAFE_VALUE_POOL(RUNTIME, N, name)                               \
  safe_value_t __##name##_values__[(N)];                                       \
  safe_value_pool_t __##name##_struct__;                                       \
  safe_value_pool_init(&__##name##_struct__, __##name##_values__, (N), RUNTIME); \
  safe_value_pool_t *name = &__##name##_struct__;

// Disposes the stack-allocated safe value pool with the given name. This is
// equivalent to just calling safe_value_pool_dispose but it's also defined as
// a macro for consistency with CREATE_SAFE_VALUE_POOL.
#define DISPOSE_SAFE_VALUE_POOL(name)                                          \
  safe_value_pool_dispose(name)

#endif // _SAFE_INL
