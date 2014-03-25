//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Custom tagged values.


#ifndef _TAGGED
#define _TAGGED

#include "check.h"
#include "value.h"


// --- S t a g e   o f f s e t ---

// Creates a new tagged stage offset value.
static value_t new_stage_offset(int32_t offset) {
  return new_custom_tagged(tpStageOffset, offset);
}

// Returns a value representing the present stage.
static value_t present_stage() {
  return new_stage_offset(0);
}

// Returns a value representing the past stage.
static value_t past_stage() {
  return new_stage_offset(-1);
}

// Returns a value representing the past stage.
static value_t past_past_stage() {
  return new_stage_offset(-2);
}

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


// --- N o t h i n g ---

// Returns the tagged nothing value.
static value_t nothing() {
  return new_custom_tagged(tpNothing, 0);
}

// A compile-time constant that is equal to the encoded representation of the
// nothing value.
#define ENCODED_NOTHING ((encoded_value_t) ((tpNothing << kDomainTagSize) | vdCustomTagged))

// Returns true iff the given value is the nothing value.
static inline bool is_nothing(value_t value) {
  return value.encoded == ENCODED_NOTHING;
}


// --- N u l l ---

// Returns the tagged null value.
static value_t null() {
  return new_custom_tagged(tpNull, 0);
}

// Returns true iff the given value is some runtime's null.
static inline bool is_null(value_t value) {
  return is_same_value(value, null());
}


// --- B o o l e a n s ---

// Returns the tagged true value. To avoid confusion with the sort-of built-in
// C true value this is called yes.
static value_t yes() {
  return new_custom_tagged(tpBoolean, 1);
}

// Returns the tagged true value. To avoid confusion with the sort-of built-in
// C false value this is called no.
static value_t no() {
  return new_custom_tagged(tpBoolean, 0);
}

// Returns the tagged boolean corresponding to the given C boolean.
static value_t new_boolean(bool value) {
  return new_custom_tagged(tpBoolean, value ? 1 : 0);
}

// Returns whether the given bool is true.
static bool get_boolean_value(value_t value) {
  CHECK_PHYLUM(tpBoolean, value);
  return get_custom_tagged_payload(value);
}


// --- R e l a t i o n s ---

typedef enum {
  reLessThan = 0x1,
  reEqual = 0x2,
  reGreaterThan = 0x4,
  reUnordered = 0x8
} relation_t;

// Creates a relation value representing the given relation.
static value_t new_relation(relation_t rel) {
  return new_custom_tagged(tpRelation, rel);
}

// Returns a relation value representing <.
static value_t less_than() {
  return new_relation(reLessThan);
}

// Returns a relation value representing >.
static value_t greater_than() {
  return new_relation(reGreaterThan);
}

// Returns a relation value representing ==.
static value_t equal() {
  return new_relation(reEqual);
}

// Returns a relation value representing the arguments not being related.
static value_t unordered() {
  return new_relation(reUnordered);
}

// Returns the relation the represents the comparison between the two given
// signed integers.
static value_t compare_signed_integers(int64_t a, int64_t b) {
  if (a < b) {
    return less_than();
  } else if (a == b) {
    return equal();
  } else {
    return greater_than();
  }
}

// Given an integer which is either negative for smaller, 0 for equal, or
// positive for greater, returns a relation that represents the same thing.
static value_t integer_to_relation(int64_t value) {
  return compare_signed_integers(value, 0);
}

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


// --- F l o a t   3 2 ---

// Creates a new tagged value wrapping a float-32.
static value_t new_float_32(float32_t value) {
  uint32_t binary;
  memcpy(&binary, &value, sizeof(uint32_t));
  return new_custom_tagged(tpFloat32, binary);
}

// Returns the value stored in a tagged float-32.
static float32_t get_float_32_value(value_t self) {
  CHECK_PHYLUM(tpFloat32, self);
  uint32_t binary = get_custom_tagged_payload(self);
  float32_t result;
  memcpy(&result, &binary, sizeof(float));
  return result;
}

// Returns the float-32 value representing infinity.
value_t float_32_infinity();

// Returns the float-32 value representing minus infinity.
value_t float_32_minus_infinity();

// Returns a float-32 value representing NaN.
value_t float_32_nan();

// Returns a relation giving how a and b relate to each other.
relation_t compare_float_32(float32_t a, float32_t b);

// Returns true if value is a float-32 other than NaN and the infinities.
bool is_float_32_finite(value_t value);

// Returns true if the value is the float-32 representation of NaN.
bool is_float_32_nan(value_t value);


// --- A m b i e n c e   r e d i r e c t ---

// Creates a new ambience redirect that corresponds to the field with the given
// offset. An ambient redirect is a point where otherwise ambience-independent
// code (typically the runtime) can indicate that an ambience-specific value
// should be used. These can't be used everywhere, only in places that
// explicitly look for them.
static value_t new_ambience_redirect(size_t offset) {
  return new_custom_tagged(tpAmbienceRedirect, offset);
}

// Returns the field this ambience redirect points to.
static size_t get_ambience_redirect_offset(value_t self) {
  CHECK_PHYLUM(tpAmbienceRedirect, self);
  return get_custom_tagged_payload(self);
}


/// ## Flag set
///
/// A flag set is a custom tagged set of up to 32 different flags. In principle
/// you could just use an int but having this as a separate type allows them to
/// be type checked at runtime and provides a convenient place to have the
/// functions that work with flag bits.
///
/// The max number of bits that can be stored in a flag set, 32, is deliberately
/// somewhat smaller than the payload size because this way it is a round number
/// and we don't have to worry about boundary conditions.

// The max number of flags in a flag set.
static const size_t kFlagSetMaxSize = 32;

// Initializer that has all flags of a flag set enabled.
static const uint32_t kFlagSetAllOn = 0xFFFFFFFF;

// Initializer that has all flags of a flag set disabled.
static const uint32_t kFlagSetAllOff = 0;

// Creates a new tiny bit set with all bits set to the given initial value.
static value_t new_flag_set(uint32_t initial_value) {
  return new_custom_tagged(tpFlagSet, initial_value);
}


// --- S c o r e ---

// The category or bracked of a score. This corresponds to the fact that any
// Eq score is considered better than any Is score, which is again better than
// any Any score. So we compare based on category first and then on the subscore
// within the category only if two scores belong to the same category. (That's
// not how it's implemented though but it behaves as if it were).
typedef enum {
  scEq    = 0,
  scIs    = 1,
  scAny   = 2,
  scExtra = 3,
  scNone  = 4
} score_category_t;

// The number of bits used for the subscore within a tagged score.
static const size_t kScoreSubscoreWidth = 32;

// Returns a new score value belonging to the given category with the given
// subscore.
static value_t new_score(score_category_t category, uint32_t subscore) {
  return new_custom_tagged(tpScore, ((uint64_t) category << kScoreSubscoreWidth) | subscore);
}


/// ## Derived object anchor
///
/// A derived object anchor describes a derived object. It's like a species for
/// a derived object. The anchor is embedded in the derived object's host, which
/// is why it's called an "anchor".

// We only allow 41 bits for the offset because the 42nd bit is the sign and
// it's not worth the hassle to handle full unsigned custom tagged payloads
// correctly yet.
static const uint64_t kDerivedObjectAnchorOffsetLimit = ((uint64_t) 1) << 41;

// Creates a new derived object anchor for an object of the given genus that's
// located at the given offset within the host.
static value_t new_derived_object_anchor(derived_object_genus_t genus,
    uint64_t host_offset) {
  CHECK_REL("derived offset too wide", host_offset, <, kDerivedObjectAnchorOffsetLimit);
  int64_t payload = (host_offset << kDerivedObjectGenusTagSize) | genus;
  return new_custom_tagged(tpDerivedObjectAnchor, payload);
}

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


#endif // _TAGGED
