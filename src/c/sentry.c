//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "sentry.h"
#include "value-inl.h"

bool is_array_of_family_sentry_impl(heap_object_family_t family, value_t self, value_t *error_out) {
  if (!in_family_sentry_impl(ofArray, self, error_out))
    return false;
  for (int64_t i = 0; i < get_array_length(self); i++) {
    if (!in_family_sentry_impl(family, get_array_at(self, i), error_out))
      return false;
  }
  return true;
}
