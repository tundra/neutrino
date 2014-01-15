// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _TAGGED_INL
#define _TAGGED_INL

#include "tagged.h"

// Returns true iff the given value is a custom tagged value within the given
// phylum.
static inline bool in_phylum(custom_tagged_phylum_t phylum, value_t value) {
  return in_domain(vdCustomTagged, value) && (get_custom_tagged_phylum(value) == phylum);
}

#endif // _TAGGED_INL
