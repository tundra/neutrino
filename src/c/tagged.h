//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Custom tagged values.


#ifndef _TAGGED
#define _TAGGED

#include "condition.h"
#include "utils/check.h"
#include "value.h"

// Sentry that checks whether a value is in the given phylum.
#define snInPhylum(tpPhylum) (_, in_phylum_sentry_impl, 0, tpPhylum, "inPhylum(" #tpPhylum ")")

static inline bool in_phylum_sentry_impl(custom_tagged_phylum_t phylum, value_t self, value_t *error_out) {
  if (!in_phylum(phylum, self)) {
    *error_out = new_unexpected_type_condition(value_type_info_for_phylum(phylum),
        get_value_type_info(self));
    return false;
  } else {
    return true;
  }
}

// Sentry that checks whether the value is nothing or in the given phylum.
#define snInPhylumOpt(tpPhylum) (_, in_phylum_opt_sentry_impl, 0, tpPhylum, "inPhylumOpt(" #tpPhylum ")")

static inline bool in_phylum_opt_sentry_impl(custom_tagged_phylum_t phylum, value_t self, value_t *error_out) {
  if (!in_phylum_opt(phylum, self)) {
    *error_out = new_unexpected_type_condition(value_type_info_for_phylum(phylum),
        get_value_type_info(self));
    return false;
  } else {
    return true;
  }
}


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


// --- N o t h i n g ---

// Returns the tagged nothing value.
static value_t nothing() {
  return new_custom_tagged(tpNothing, 0);
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

// Returns nothing if the value is null, otherwise returns the value itself.
static inline value_t null_to_nothing(value_t value) {
  return is_null(value) ? nothing() : value;
}

// Returns true iff the given value is either null or an object within the given
// family.
static inline bool in_family_or_null(heap_object_family_t family, value_t value) {
  return is_null(value) || in_family(family, value);
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


// --- F l o a t   3 2 ---

// Creates a new tagged value wrapping a float-32.
static value_t new_float_32(float32_t value) {
  uint32_t binary;
  memcpy(&binary, &value, sizeof(uint32_t));
  return new_custom_tagged(tpFloat32, binary);
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


/// ## Ascii character
///
/// One of the 256 ascii characters with support for ctype based character
/// predicates. Should be replaced with proper unicode support eventually.

// Creates a new tagged stage offset value.
static value_t new_ascii_character(uint8_t value) {
  return new_custom_tagged(tpAsciiCharacter, value);
}


/// ## Hash code
///
/// A wrapper around a generated or calculated hash code.

// Returns a new tagged hash code value. If the code is greater than 48 bits
// (the capacity of a tagged value payload) the top bits will be silently
// discarded. Don't depend on how they're discarded for correctness though.
static value_t new_hash_code(uint64_t value) {
  // Shifting away the top bits like this ensures that the top bit within the
  // range we can actually represent becomes the sign bit of the truncated
  // value. Basically we're smearing the top bit across the whole top of the
  // value.
  size_t bits_to_discard = 64 - kCustomTaggedPayloadSize;
  int64_t truncated = (((int64_t) value) << bits_to_discard) >> bits_to_discard;
  return new_custom_tagged(tpHashCode, truncated);
}


/// ## Transport
///
/// Value that indicates how an invocation should be executed, synchronous or
/// asynchronous.

// Mode of transport.
typedef enum {
  tmSync    = 0,
  tmAsync   = 1
} transport_mode_t;

// Returns a new transport with the given mode.
static value_t new_transport(transport_mode_t mode) {
  return new_custom_tagged(tpTransport, mode);
}

// Returns the asynchronous transport mode.
static value_t transport_async() {
  return new_transport(tmAsync);
}

// Returns the synchronous transport mode.
static value_t transport_sync() {
  return new_transport(tmSync);
}


#endif // _TAGGED
