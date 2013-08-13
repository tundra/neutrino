// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Methods and method lookup. See details in method.md.

#include "value-inl.h"

#ifndef _METHOD
#define _METHOD


// --- S i g n a t u r e ---

static const size_t kSignatureSize = OBJECT_SIZE(5);
static const size_t kSignatureTagsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kSignatureDescriptorsOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kSignatureParameterCountOffset = OBJECT_FIELD_OFFSET(2);
static const size_t kSignatureMandatoryCountOffset = OBJECT_FIELD_OFFSET(3);
static const size_t kSignatureAllowExtraOffset = OBJECT_FIELD_OFFSET(4);

// The sorted array of signature tags.
ACCESSORS_DECL(signature, tags);

// The matching array of parameter descriptors.
ACCESSORS_DECL(signature, descriptors);

// The number of parameters defined by this signature.
INTEGER_ACCESSORS_DECL(signature, parameter_count);

// The number of mandatory parameters required by this signature.
INTEGER_ACCESSORS_DECL(signature, mandatory_count);

// Are extra arguments allowed?
INTEGER_ACCESSORS_DECL(signature, allow_extra);


// --- P a r a m e t e r ---

static const size_t kParameterSize = OBJECT_SIZE(3);
static const size_t kParameterGuardOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kParameterIsOptionalOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kParameterIndexOffset = OBJECT_FIELD_OFFSET(2);

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
static const size_t kGuardTypeOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kGuardValueOffset = OBJECT_FIELD_OFFSET(1);

// The value of this guard used to match by gtId and gtIs and unused for gtAny.
ACCESSORS_DECL(guard, value);

// The type of match to perform for this guard.
TYPED_ACCESSORS_DECL(guard, guard_type_t, type);

// A guard score against a value.
typedef uint32_t score_t;

// This guard matched perfectly.
static const score_t gsIdenticalMatch = 0;

// It's not an identical match but the closest possible instanceof-match.
static const score_t gsPerfectIsMatch = 1;

// Score that signifies that a guard didn't match at all.
static const score_t gsNoMatch = 0xFFFFFFFF;

// There was a match but only because extra arguments are allowed so anything
// more specific would match better.
static const score_t gsExtraMatch = 0xFFFFFFFE;

// The guard matched the given value but only because it matches any value so
// anything more specific would match better.
static const score_t gsAnyMatch = 0xFFFFFFFD;

// Matches the given guard against the given value, returning a score for how
// well it matched within the given method space.
score_t guard_match(runtime_t *runtime, value_t guard, value_t value,
    value_t method_space);

// Returns true if the given score represents a match.
bool is_score_match(score_t score);

// Compares which score is best. If a is better than b then it will compare
// smaller, equally good compares equal, and worse compares greater than.
int compare_scores(score_t a, score_t b);


// --- M e t h o d   s p a c e ---

static const size_t kMethodSpaceSize = OBJECT_SIZE(1);
static const size_t kMethodSpaceInheritanceMapOffset = OBJECT_FIELD_OFFSET(0);

// The size of the inheritance map in an empty method space.
static const size_t kInheritanceMapInitialSize = 16;

// The mapping that defines the inheritance hierarchy within this method space.
ACCESSORS_DECL(method_space, inheritance_map);

// Records in the given method space that the subtype inherits directly from
// the supertype. Returns a signal if adding fails, for instance of we run
// out of memory to increase the size of the map.
value_t add_method_space_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype);

// Returns the array buffer of parents of the given protocol.
value_t get_protocol_parents(runtime_t *runtime, value_t space, value_t protocol);


#endif // _METHOD
