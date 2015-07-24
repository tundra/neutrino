//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _TAGGED_INL
#define _TAGGED_INL

#include "tagged.h"
#include "value-inl.h"

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_PHYLUM(tpPhylum, EXPR)                                        \
VALIDATE(in_phylum(tpPhylum, EXPR))


/// ## Flag set

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

// Returns true iff the given flag set has no flags set at all.
static bool is_flag_set_empty(value_t self) {
  CHECK_PHYLUM(tpFlagSet, self);
  return get_custom_tagged_payload(self) == 0;
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
  return (score_category_t) (((uint64_t) get_custom_tagged_payload(score)) >> kScoreSubscoreWidth);
}

// Returns the subscore of the given score object.
static uint32_t get_score_subscore(value_t score) {
  uint64_t kMask = (1LL << kScoreSubscoreWidth) - 1LL;
  return (uint32_t) (((uint64_t) get_custom_tagged_payload(score)) & kMask);
}

// Returns true if a is a better score than b.
static bool is_score_better(value_t a, value_t b) {
  return a.encoded < b.encoded;
}

// Works the same way as the ordering compare but returns -1, 0, and 1 instead
// of relation values.
static int compare_tagged_scores(value_t a, value_t b) {
  return is_score_better(a, b) ? 1 : (is_same_value(a, b) ? 0 : -1);
}

// Returns a score that belongs to the same category as the given one with a
// subscore that is one epsilon worse than the given value, so compares less
// than.
static value_t get_score_successor(value_t value) {
  CHECK_PHYLUM(tpScore, value);
  value_t result;
  result.encoded = value.encoded + kCustomTaggedPayloadOne;
  return result;
}

// Returns true if the given score represents a match.
static bool is_score_match(value_t score) {
  return is_score_better(score, new_score(scNone, 0));
}

// This guard matched perfectly.
static value_t new_identical_match_score() {
  return new_score(scEq, 0);
}

// It's not an identical match but the closest possible instanceof-match.
static value_t new_perfect_is_match_score() {
  return new_score(scIs, 0);
}

// Score that signifies that a guard didn't match at all.
static value_t new_no_match_score() {
  return new_score(scNone, 0);
}

// There was a match but only because extra arguments are allowed so anything
// more specific would match better.
static value_t new_extra_match_score() {
  return new_score(scExtra, 0);
}

// The guard matched the given value but only because it matches any value so
// anything more specific would match better.
static value_t new_any_match_score() {
  return  new_score(scAny, 0);
}


/// ## Stage offset

// Returns the integer value of the given stage offset.
static int32_t get_stage_offset_value(value_t value) {
  CHECK_PHYLUM(tpStageOffset, value);
  return (int32_t) get_custom_tagged_payload(value);
}

// Returns a value representing the next stage after the given stage. For
// instance, the successor of the past is the present.
static value_t get_stage_offset_successor(value_t stage) {
  return new_stage_offset(get_stage_offset_value(stage) + 1);
}

// Returns a new tagged integer which is the sum of the two given tagged
// integers.
static value_t add_stage_offsets(value_t a, value_t b) {
  return new_stage_offset(get_stage_offset_value(a) + get_stage_offset_value(b));
}


/// ## Boolean

// Returns whether the given bool is true.
static bool get_boolean_value(value_t value) {
  CHECK_PHYLUM(tpBoolean, value);
  return get_custom_tagged_payload(value);
}


/// ## Relation

// Returns the enum value indicating the type of this relation.
static relation_t get_relation_value(value_t value) {
  CHECK_PHYLUM(tpRelation, value);
  return (relation_t) get_custom_tagged_payload(value);
}

// Given a relation, returns an integer that represents the same relation such
// that -1 is smaller, 0 is equal, and 1 is greater. If the value is unordered
// an arbitrary value is returned.
static int relation_to_integer(value_t value) {
  switch (get_relation_value(value)) {
    case reLessThan: return -1;
    case reEqual: return 0;
    case reGreaterThan: return 1;
    default: return 2;
  }
}

// Tests what kind of relation the given value is. For instance, if you call
// test_relation(x, reLessThan | reEqual) the result will be true iff x is
// the relation less_than() or equal().
static bool test_relation(value_t relation, unsigned mask) {
  return (get_relation_value(relation) & mask) != 0;
}


/// ## Float 32

// Returns the value stored in a tagged float-32.
static float32_t get_float_32_value(value_t self) {
  CHECK_PHYLUM(tpFloat32, self);
  uint32_t binary = (uint32_t) get_custom_tagged_payload(self);
  float32_t result;
  memcpy(&result, &binary, sizeof(float));
  return result;
}


/// ## Derived object anchor

// Returns the genus of the given derived object anchor.
static derived_object_genus_t get_derived_object_anchor_genus(value_t self) {
  CHECK_PHYLUM(tpDerivedObjectAnchor, self);
  int64_t payload = get_custom_tagged_payload(self);
  return (derived_object_genus_t) (payload & ((1 << kDerivedObjectGenusTagSize) - 1));
}

// Returns the raw offset (in bytes) within the host of the derived object which
// is anchored by the given anchor.
static uint64_t get_derived_object_anchor_host_offset(value_t self) {
  CHECK_PHYLUM(tpDerivedObjectAnchor, self);
  int64_t payload = get_custom_tagged_payload(self);
  return payload >> kDerivedObjectGenusTagSize;
}


/// ## Ascii character

// Returns the ordinal of the given ascii character.
static uint8_t get_ascii_character_value(value_t value) {
  CHECK_PHYLUM(tpAsciiCharacter, value);
  return (uint8_t) get_custom_tagged_payload(value);
}


/// ## Hash code

// Returns the integer value of the given hash code.
static uint64_t get_hash_code_value(value_t value) {
  CHECK_PHYLUM(tpHashCode, value);
  return get_custom_tagged_payload(value) & ((1ULL << kCustomTaggedPayloadSize) - 1);
}


/// ## Transport mode

// Returns the mode of the given transport.
static transport_mode_t get_transport_mode(value_t value) {
  CHECK_PHYLUM(tpTransport, value);
  return (transport_mode_t) get_custom_tagged_payload(value);
}

// Is the given value the synchronous transport value?
static bool is_transport_sync(value_t value) {
  return get_transport_mode(value) == tmSync;
}


#endif // _TAGGED_INL
