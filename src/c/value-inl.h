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

// Returns true iff the given value is some runtime's null.
static inline bool is_null(value_t value) {
  return in_family(ofNull, value);
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
value_t set_##family##_contents(value_t value, runtime_t *runtime,             \
    value_t contents) {                                                        \
  return new_signal(scUnsupportedBehavior);                                    \
}                                                                              \
SWALLOW_SEMI(csc)

// Declares the heap size functions for a fixed-size object that don't have any
// non-value fields.
#define FIXED_SIZE_PURE_VALUE_IMPL(Family, family)                             \
void get_##family##_layout(value_t value, object_layout_t *layout_out) {       \
  object_layout_set(layout_out, k##Family##Size, kValueSize);                  \
}                                                                              \
SWALLOW_SEMI(fspvi)

// Expands to a trivial implementation of print_on and print_atomic_on that just
// prints the family's name within brackets.
#define TRIVIAL_PRINT_ON_IMPL(Family, family)                                  \
void family##_print_on(value_t value, string_buffer_t *buf) {                  \
  family##_print_atomic_on(value, buf);                                        \
}                                                                              \
void family##_print_atomic_on(value_t value, string_buffer_t *buf) {           \
  CHECK_FAMILY(of##Family, value);                                             \
  string_buffer_printf(buf, "#<" #family ">");                                 \
}                                                                              \
SWALLOW_SEMI(tpo)

// Expands to an implementation of get_protocol that returns the canonical
// protocol for the value's family.
#define GET_FAMILY_PROTOCOL_IMPL(family)                                       \
value_t get_##family##_protocol(value_t self, runtime_t *runtime) {            \
  return runtime->roots.family##_protocol;                                     \
}                                                                              \
SWALLOW_SEMI(gfpi)


// Expands to an implementation of get_protocol that returns a signal. Use this
// for families that are not intended to be available to the surface language.
#define NO_FAMILY_PROTOCOL_IMPL(family)                                        \
value_t get_##family##_protocol(value_t self, runtime_t *runtime) {            \
  return new_signal(scUnsupportedBehavior);                                    \
}                                                                              \
SWALLOW_SEMI(nfpi)


// --- P l a i n   a c c e s s o r s ---

// Expands to a function that gets the specified field in the specified object
// family.
#define GETTER_IMPL(Receiver, receiver, Field, field)                          \
value_t get_##receiver##_##field(value_t self) {                               \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return *access_object_field(self, k##Receiver##Field##Offset);               \
}                                                                              \
SWALLOW_SEMI(gi)

// Expands to a checked setter and a getter for the specified types.
#define CHECKED_ACCESSORS_IMPL(Receiver, receiver, Family, Field, field)       \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  CHECK_FAMILY(of##Family, value);                                             \
  *access_object_field(self, k##Receiver##Field##Offset) = value;              \
}                                                                              \
GETTER_IMPL(Receiver, receiver, Field, field)

// Expands to an unchecked setter and a getter for the specified types.
#define UNCHECKED_ACCESSORS_IMPL(Receiver, receiver, Field, field)             \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  *access_object_field(self, k##Receiver##Field##Offset) = value;              \
}                                                                              \
GETTER_IMPL(Receiver, receiver, Field, field)


// --- I n t e g e r / e n u m   a c c e s s o r s ---

#define __MAPPING_SETTER_IMPL__(Receiver, receiver, type_t, Field, field, MAP) \
void set_##receiver##_##field(value_t self, type_t value) {                    \
  CHECK_FAMILY(of##Receiver, self);                                            \
  *access_object_field(self, k##Receiver##Field##Offset) = MAP(type_t, value); \
}                                                                              \
SWALLOW_SEMI(msi)

#define __MAPPING_GETTER_IMPL__(Receiver, receiver, type_t, Field, field, MAP) \
type_t get_##receiver##_##field(value_t self) {                                \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return MAP(type_t, *access_object_field(self, k##Receiver##Field##Offset));  \
}                                                                              \
SWALLOW_SEMI(mgi)

#define __NEW_INTEGER_MAP__(T, N) new_integer(N)
#define __GET_INTEGER_VALUE_MAP__(T, N) get_integer_value(N)

// Expands to an integer getter and setter.
#define INTEGER_ACCESSORS_IMPL(Receiver, receiver, Field, field)               \
__MAPPING_SETTER_IMPL__(Receiver, receiver, size_t, Field, field,              \
    __NEW_INTEGER_MAP__);                                                      \
__MAPPING_GETTER_IMPL__(Receiver, receiver, size_t, Field, field,              \
    __GET_INTEGER_VALUE_MAP__)

#define ENUM_ACCESSORS_IMPL(Receiver, receiver, type_t, Field, field)          \
__MAPPING_SETTER_IMPL__(Receiver, receiver, type_t, Field, field,              \
    __NEW_INTEGER_MAP__);                                                      \
__MAPPING_GETTER_IMPL__(Receiver, receiver, type_t, Field, field,              \
    __GET_INTEGER_VALUE_MAP__)


// --- S p e c i e s   a c c e s s o r s ---

// Expands to a function that gets the specified field in the specified object
// family.
#define SPECIES_GETTER_IMPL(Receiver, receiver, ReceiverSpecies,               \
    receiver_species, Field, field)                                            \
value_t get_##receiver_species##_species_##field(value_t self) {               \
  CHECK_FAMILY(ofSpecies, self);                                               \
  CHECK_DIVISION(sd##ReceiverSpecies, self);                                   \
  return *access_object_field(self,                                            \
      k##ReceiverSpecies##Species##Field##Offset);                             \
}                                                                              \
value_t get_##receiver##_##field(value_t self) {                               \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return get_##receiver_species##_species_##field(get_object_species(self));   \
}                                                                              \
SWALLOW_SEMI(sgi)

#define CHECKED_SPECIES_ACCESSORS_IMPL(Receiver, receiver, ReceiverSpecies,    \
    receiver_species, Family, Field, field)                                    \
void set_##receiver_species##_species_##field(value_t self, value_t value) {   \
  CHECK_DIVISION(sd##ReceiverSpecies, self);                                   \
  CHECK_FAMILY(of##Family, value);                                             \
  *access_object_field(self, k##ReceiverSpecies##Species##Field##Offset) = value; \
}                                                                              \
SPECIES_GETTER_IMPL(Receiver, receiver, ReceiverSpecies, receiver_species,     \
    Field, field)

#endif // _VALUE_INL
