// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Derived object types


#ifndef _DERIVED_INL
#define _DERIVED_INL

#include "check.h"
#include "derived.h"
#include "utils.h"
#include "value-inl.h"


/// ## Derived objects

// Returns true iff the given value is a derived object within the given genus.
static inline bool in_genus(derived_object_genus_t genus, value_t value) {
  return is_derived_object(value) && (get_derived_object_genus(value) == genus);
}

#endif // _DERIVED_INL
