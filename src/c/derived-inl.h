//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Derived object types


#ifndef _DERIVED_INL
#define _DERIVED_INL

#include "derived.h"
#include "utils.h"
#include "utils/check.h"
#include "value-inl.h"


/// ## Derived objects

// Returns true iff the given value is a derived object within the given genus.
static inline bool in_genus(derived_object_genus_t genus, value_t value) {
  return is_derived_object(value) && (get_derived_object_genus(value) == genus);
}

// Returns true iff the given value is a derived object within the given genus
// or nothing.
static inline bool in_genus_opt(derived_object_genus_t genus, value_t value) {
  return is_nothing(value) || in_genus(genus, value);
}

// Checks whether the value at the end of the given pointer belongs to the
// specified genus. If not, returns a validation failure.
#define VALIDATE_GENUS(dgGenus, EXPR)                                          \
VALIDATE(in_genus(dgGenus, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified genus. If not, returns a validation failure.
#define VALIDATE_GENUS_OPT(dgGenus, EXPR)                                      \
VALIDATE(in_genus_opt(dgGenus, EXPR))

// Is the value in the given genus?
#define snInGenus(dgGenus) (_, in_genus_sentry_impl, dgGenus, "inGenus(" #dgGenus ")")

static inline bool in_genus_sentry_impl(derived_object_genus_t genus, value_t self, value_t *error_out) {
  if (!in_genus(genus, self)) {
    *error_out = new_unexpected_type_condition(value_type_info_for_genus(genus),
        get_value_type_info(self));
    return false;
  } else {
    return true;
  }
}

// Is the value nothing or in a the given genus?
#define snInGenusOpt(dgGenus) (_, in_genus_opt_sentry_impl, dgGenus, "inGenusOpt(" #dgGenus ")")

static inline bool in_genus_opt_sentry_impl(derived_object_genus_t genus, value_t self, value_t *error_out) {
  if (!in_genus_opt(genus, self)) {
    *error_out = new_unexpected_type_condition(value_type_info_for_genus(genus),
        get_value_type_info(self));
    return false;
  } else {
    return true;
  }
}

// Returns the derived object descriptor for the given derived object.
static genus_descriptor_t *get_derived_object_descriptor(value_t self) {
  CHECK_DOMAIN(vdDerivedObject, self);
  return get_genus_descriptor(get_derived_object_genus(self));
}

#endif // _DERIVED_INL
