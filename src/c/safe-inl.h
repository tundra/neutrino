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

#endif // _SAFE_INL
