// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _TAGGED_INL
#define _TAGGED_INL

#include "tagged.h"
#include "value-inl.h"

// Returns true iff the given value is a custom tagged value within the given
// phylum.
static inline bool in_phylum(custom_tagged_phylum_t phylum, value_t value) {
  return in_domain(vdCustomTagged, value) && (get_custom_tagged_phylum(value) == phylum);
}

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_PHYLUM(tpPhylum, EXPR)                                        \
VALIDATE(in_phylum(tpPhylum, EXPR))

// Accessor check that indicates that the argument should belong to the phylum
// specified in the argument.
#define acInPhylum(tpPhylum, VALUE)                                            \
  CHECK_PHYLUM(tpPhylum, (VALUE))


// --- F l a g   s e t ---

// Returns true iff any of the given flags are set in this flag set. The typical
// case is giving a single flag in which case the result is the value of that
// flag.
static bool get_flag_set_at(value_t self, uint32_t flags) {
  CHECK_PHYLUM(tpFlagSet, self);
  return (get_custom_tagged_payload(self) & flags) != 0;
}

// Returns a flag set identical to the given set on all other flags than the
// given set, and with all the given flags enabled.
static value_t enable_flag_set_flags(value_t self, uint32_t flags) {
  CHECK_PHYLUM(tpFlagSet, self);
  return new_custom_tagged(tpFlagSet, get_custom_tagged_payload(self) | flags);
}

// Returns a flag set identical to the given set on all other flags than the
// given set, and with all the given flags disabled.
static value_t disable_flag_set_flags(value_t self, uint32_t flags) {
  CHECK_PHYLUM(tpFlagSet, self);
  return new_custom_tagged(tpFlagSet, get_custom_tagged_payload(self) & ~flags);
}

// Returns a flag set identical to the given set on all other flags than the
// given set, and with the given flags set to the specified value.
static value_t set_flag_set_at(value_t self, uint32_t flags, bool value) {
  return value
      ? enable_flag_set_flags(self, flags)
      : disable_flag_set_flags(self, flags);
}

#endif // _TAGGED_INL
