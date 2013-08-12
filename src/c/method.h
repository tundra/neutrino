// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Methods and method lookup. See details in method.md.

#include "value-inl.h"

#ifndef _METHOD
#define _METHOD


// --- S i g n a t u r e ---

static const size_t kSignatureSize = OBJECT_SIZE(5);
static const size_t kSignatureTagsOffset = 1;
static const size_t kSignatureDescriptorsOffset = 2;
static const size_t kSignatureParameterCountOffset = 3;
static const size_t kSignatureMandatoryCountOffset = 4;
static const size_t kSignatureAllowExtraOffset = 5;

// The sorted array of signature tags.
ACCESSORS_DECL(signature, tags);

// The matching array of parameter descriptors.
ACCESSORS_DECL(signature, descriptors);


// --- P a r a m e t e r ---

static const size_t kParameterSize = OBJECT_SIZE(3);
static const size_t kParameterGuardOffset = 1;
static const size_t kParameterIsOptionalOffset = 2;
static const size_t kParameterIndexOffset = 3;

// This parameter's guard.
ACCESSORS_DECL(parameter, guard);


// --- G u a r d ---

// A parameter guard.

// How this guard matches.
typedef enum {
  // Match by value identity.
  gtId,
  // Match by 'instanceof'.
  gtIs,
  // Always match.
  gtAny
} guard_type_t;

static const size_t kGuardSize = OBJECT_SIZE(2);
static const size_t kGuardTypeOffset = 1;
static const size_t kGuardValueOffset = 2;

// The value of this guard used to match by gtId and gtIs and unused for gtAny.
ACCESSORS_DECL(guard, value);

// The type of match to perform for this guard.
TYPED_ACCESSORS_DECL(guard, guard_type_t, type);

// A guard score against a value.
typedef uint32_t score_t;

// This guard matched perfectly.
static const score_t gsIdenticalMatch = 0;

// Score that signifies that a guard didn't match at all.
static const score_t gsNoMatch = 0xFFFFFFFF;

// There was a match but only because extra arguments are allowed so anything
// more specific would match better.
static const score_t gsExtraMatch = 0xFFFFFFFE;

// The guard matched the given value but only because it matches any value so
// anything more specific would match better.
static const score_t gsAnyMatch = 0xFFFFFFFD;

// Matches the given guard against the given value, returning a score for how
// well it matched.
score_t guard_match(value_t guard, value_t value);

// Returns true if the given score represents a match.
bool is_score_match(score_t score);

#endif // _METHOD
