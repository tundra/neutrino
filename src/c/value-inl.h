// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "value.h"

#ifndef _VALUE_INL
#define _VALUE_INL

// Evaluates the given expression; if it yields a signal returns it otherwise
// ignores the value.
#define TRY(EXPR) do {                                                         \
  value_t __result__ = (EXPR);                                                 \
  if (in_domain(vdSignal, __result__))                                         \
    return __result__;                                                         \
} while (false)

// Evaluates the value and if it yields a signal bails out, otherwise assigns
// the result to the given target.
#define TRY_SET(TARGET, VALUE) do {                                            \
  value_t __value__ = (VALUE);                                                 \
  TRY(__value__);                                                              \
  TARGET = __value__;                                                          \
} while (false)

// Declares a new variable to have the specified value. If the initializer
// yields a signal we bail out and return that value.
#define TRY_DEF(name, INIT)                                                    \
value_t name;                                                                  \
TRY_SET(name, INIT)

// Returns true if the value of the given expression is in the specified
// domain.
static inline bool in_domain(value_domain_t domain, value_t value) {
  return get_value_domain(value) == domain;
}

// Returns true iff the given value is an object within the given family.
static inline bool in_family(object_family_t family, value_t value) {
  return in_domain(vdObject, value) && (get_object_family(value) == family);
}

// Returns true iff the given species belongs to the given division.
static inline bool in_division(species_division_t division, value_t value) {
  return in_family(ofSpecies, value) && (get_species_division(value) == division);
}

// Returns true iff the value is a signal with the specified cause.
static inline bool is_signal(signal_cause_t cause, value_t value) {
  return in_domain(vdSignal, value) && (get_signal_cause(value) == cause);
}

// Checks whether the expression holds and if not returns a validation failure.
#define VALIDATE(EXPR) do {                                                    \
  SIG_CHECK_TRUE("validation", scValidationFailed, EXPR);                      \
} while (false)

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_VALUE_FAMILY(ofFamily, EXPR)                                  \
VALIDATE(in_family(ofFamily, EXPR))


// --- B e h a v i o r ---

// Declares the identity and identity hash functions for a value family that
// uses object identity.
#define OBJECT_IDENTITY_IMPL(family)                                           \
value_t family##_transient_identity_hash(value_t value) {                      \
  return OBJ_ADDR_HASH(value);                                                 \
}                                                                              \
bool family##_are_identical(value_t a, value_t b) {                            \
  return (a.encoded == b.encoded);                                             \
}                                                                              \
SWALLOW_SEMI(oii)

// Declares a set_contents function that returns a signal indicating that this
// family doesn't support setting contents.
#define CANT_SET_CONTENTS(family)                                              \
value_t set_##family##_contents(value_t value, struct runtime_t *runtime,      \
    value_t contents) {                                                        \
  return new_signal(scUnsupportedBehavior);                                    \
}                                                                              \
SWALLOW_SEMI(csc)

// Declares the heap size functions for a fixed-size object that don't have any
// non-value fields.
#define FIXED_SIZE_PURE_VALUE_IMPL(family, Family)                             \
void get_##family##_layout(value_t value, object_layout_t *layout_out) {       \
  object_layout_set(layout_out, k##Family##Size, kValueSize);                  \
}                                                                              \
SWALLOW_SEMI(fspvi)


// --- A c c e s s o r s ---

// Expands to a function that sets the given field on the given receiver,
// checking that both the receiver and the argument have the expected types.
#define CHECKED_SETTER_IMPL(Receiver, receiver, Family, Field, field)          \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  CHECK_FAMILY(of##Family, value);                                             \
  *access_object_field(self, k##Receiver##Field##Offset) = value;              \
}                                                                              \
SWALLOW_SEMI(csi)

// Expands to a function that gets the specified field in the specified object
// family.
#define GETTER_IMPL(Receiver, receiver, Field, field)                          \
value_t get_##receiver##_##field(value_t self) {                               \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return *access_object_field(self, k##Receiver##Field##Offset);               \
}                                                                              \
SWALLOW_SEMI(gi)

// Expands to a checked setter and a getter for the specified types.
#define CHECKED_GETTER_SETTER_IMPL(Receiver, receiver, Family, Field, field)   \
CHECKED_SETTER_IMPL(Receiver, receiver, Family, Field, field);                 \
GETTER_IMPL(Receiver, receiver, Field, field)

// Expands to a setter that sets an integer-valued field to an integer value,
// wrapping the value as a tagged integer.
#define INTEGER_SETTER_IMPL(Receiver, receiver, Field, field)                  \
void set_##receiver##_##field(value_t self, size_t value) {                    \
  CHECK_FAMILY(of##Receiver, self);                                            \
  *access_object_field(self, k##Receiver##Field##Offset) = new_integer(value); \
}                                                                              \
SWALLOW_SEMI(isi)

// Expands to a getter function that returns a integer-valued field, unwrapping
// the tagged value appropriately.
#define INTEGER_GETTER_IMPL(Receiver, receiver, Field, field)                  \
size_t get_##receiver##_##field(value_t self) {                                \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return get_integer_value(*access_object_field(self, k##Receiver##Field##Offset)); \
}                                                                              \
SWALLOW_SEMI(igi)

// Expands to an integer getter and setter.
#define INTEGER_GETTER_SETTER_IMPL(Receiver, receiver, Field, field)           \
INTEGER_SETTER_IMPL(Receiver, receiver, Field, field);                         \
INTEGER_GETTER_IMPL(Receiver, receiver, Field, field)

#endif // _VALUE_INL
