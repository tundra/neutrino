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


// --- T i n y   b i t   s e t ---

// Returns the value of the index'th bit in the given bit set.
static bool get_tiny_bit_set_at(value_t bits, size_t index) {
  CHECK_REL("bit set out of bounds", index, <, kTinyBitSetMaxSize);
  CHECK_PHYLUM(tpTinyBitSet, bits);
  return (get_custom_tagged_payload(bits) & (1LL << index)) != 0;
}

// Returns a new bit set that is identical to the given set at all other indices
// and where the index'th bit has the given value.
static value_t set_tiny_bit_set_at(value_t bits, size_t index, bool value) {
  CHECK_REL("bit set out of bounds", index, <, kTinyBitSetMaxSize);
  CHECK_PHYLUM(tpTinyBitSet, bits);
  int64_t old_payload = get_custom_tagged_payload(bits);
  return value
      ? new_custom_tagged(tpTinyBitSet, old_payload | (1LL << index))
      : new_custom_tagged(tpTinyBitSet, old_payload & ~(1LL << index));
}

#endif // _TAGGED_INL
