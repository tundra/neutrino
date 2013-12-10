// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

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

// Returns true iff the given value is the nothing value.
static inline bool is_nothing(value_t value) {
  return is_same_value(value, nothing());
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

#endif // _TAGGED
