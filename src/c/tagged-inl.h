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


// --- S c o r e ---

// Returns the category flag of the given score object.
static score_category_t get_score_category(value_t score) {
  return ((uint64_t) get_custom_tagged_payload(score)) >> kScoreSubscoreWidth;
}

// Returns the subscore of the given score object.
static uint32_t get_score_subscore(value_t score) {
  uint64_t kMask = (1LL << kScoreSubscoreWidth) - 1LL;
  return ((uint64_t) get_custom_tagged_payload(score)) & kMask;
}

// Works the same way as the ordering compare but returns -1, 0, and 1 instead
// of relation values.
static int compare_tagged_scores(value_t a, value_t b) {
  int64_t a_payload = get_custom_tagged_payload(a);
  int64_t b_payload = get_custom_tagged_payload(b);
  return b_payload < a_payload ? -1 : (a_payload == b_payload ? 0 : 1);
}


#endif // _TAGGED_INL
