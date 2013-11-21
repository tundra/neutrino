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
static value_t boolean(bool value) {
  return new_custom_tagged(tpBoolean, value ? 1 : 0);
}

// Returns whether the given bool is true.
static bool get_boolean_value(value_t value) {
  CHECK_PHYLUM(tpBoolean, value);
  return get_custom_tagged_payload(value);
}


#endif // _TAGGED
