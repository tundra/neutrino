// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _VALUE_INL
#define _VALUE_INL

#include "signals.h"
#include "try-inl.h"
#include "utils.h"
#include "value.h"

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

// Returns true iff the given value is some runtime's nothing.
static inline bool is_nothing(value_t value) {
  return in_family(ofNothing, value);
}

// Returns true iff the given value is either nothing or an object within the
// given family.
static inline bool in_family_opt(object_family_t family, value_t value) {
  return is_nothing(value) || in_family(family, value);
}

// Returns true iff the given species belongs to the given division.
static inline bool in_division(species_division_t division, value_t value) {
  return in_family(ofSpecies, value) && (get_species_division(value) == division);
}

// Returns true iff the value is a signal with the specified cause.
static inline bool is_signal(signal_cause_t cause, value_t value) {
  return in_domain(vdSignal, value) && (get_signal_cause(value) == cause);
}


// --- P r i n t i n g ---

// Helper data type for the shorthand for converting a value to a string.
// Converting to a string generates some data that needs to be disposed, this
// structure captures that so it can be disposed using dispose_value_to_string.
typedef struct {
  // The string representation of the value.
  string_t str;
  // The string buffer used to build the result.
  string_buffer_t buf;
} value_to_string_t;

// Converts the given value to a string.
const char *value_to_string(value_to_string_t *data, value_t value);

// Disposes the data buffer.
void dispose_value_to_string(value_to_string_t *data);


// --- V a l i d a t i o n ---

// Checks whether the expression holds and if not returns a validation failure.
#define VALIDATE(EXPR) do {                                                    \
  SIG_CHECK_TRUE("validation", scValidationFailed, EXPR);                      \
} while (false)

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_FAMILY(ofFamily, EXPR)                                        \
VALIDATE(in_family(ofFamily, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_FAMILY_OPT(ofFamily, EXPR)                                    \
VALIDATE(in_family_opt(ofFamily, EXPR))

// --- B e h a v i o r ---

// Expands to a trivial implementation of print_on.
#define TRIVIAL_PRINT_ON_IMPL(Family, family)                                  \
void family##_print_on(value_t value, string_buffer_t *buf,                    \
    print_flags_t flags, size_t depth) {                                       \
  CHECK_FAMILY(of##Family, value);                                             \
  string_buffer_printf(buf, "#<" #family ">");                                 \
}                                                                              \
SWALLOW_SEMI(tpo)

// Expands to an implementation of the built-in method definition function that
// defines no built-ins.
#define NO_BUILTIN_METHODS(family)                                             \
value_t add_##family##_builtin_methods(runtime_t *runtime,                     \
    safe_value_t s_space) {                                                    \
  return success();                                                            \
}

// Expands to an implementation of get_protocol that returns the canonical
// protocol for the value's family.
#define GET_FAMILY_PROTOCOL_IMPL(family)                                       \
value_t get_##family##_protocol(value_t self, runtime_t *runtime) {            \
  return ROOT(runtime, family##_protocol);                                     \
}                                                                              \
SWALLOW_SEMI(gfpi)

// Expands to an implementation of get and set family_mode for a family with a
// fixed mode.
#define FIXED_GET_MODE_IMPL(family, vmMode)                                    \
value_mode_t get_##family##_mode(value_t self) {                               \
  return vmMode;                                                               \
}                                                                              \
value_t set_##family##_mode_unchecked(runtime_t *rt, value_t self, value_mode_t mode) {  \
  CHECK_TRUE("invalid mode change",                                            \
      mode == vmMode || ((mode == vmFrozen) && (vmMode == vmDeepFrozen)));     \
  return success();                                                            \
}                                                                              \
SWALLOW_SEMI(fgmi)


// --- P l a i n   a c c e s s o r s ---

// Expands to a function that gets the specified field in the specified object
// family.
#define GETTER_IMPL(Receiver, receiver, Field, field)                          \
value_t get_##receiver##_##field(value_t self) {                               \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return *access_object_field(self, k##Receiver##Field##Offset);               \
}                                                                              \
SWALLOW_SEMI(gi)

// Accessor check that indicates that no check should be performed. A check that
// the value is not a signal will be performed in any case since that is a
// global invariant.
#define acNoCheck(UNUSED, VALUE)                                               \
  CHECK_FALSE("storing signal in heap", in_domain(vdSignal, (VALUE)))

// Accessor check that indicates that the argument should belong to the family
// specified in the argument.
#define acInFamily(ofFamily, VALUE)                                            \
  CHECK_FAMILY(ofFamily, (VALUE))

// Accessor check that indicates that the argument should either be nothing or
// belong to the family specified in the argument.
#define acInFamilyOpt(ofFamily, VALUE)                                         \
  CHECK_FAMILY_OPT(ofFamily, (VALUE))

// Accessor check that indicates that the argument should either be nothing or
// belong to a syntax family.
#define acIsSyntaxOpt(UNUSED, VALUE)                                           \
  CHECK_SYNTAX_FAMILY_OPT(VALUE)

// Expands to an unchecked setter and a getter for the specified types.
#define ACCESSORS_IMPL(Receiver, receiver, acValueCheck, VALUE_CHECK_ARG, Field, field)  \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  acValueCheck(VALUE_CHECK_ARG, value);                                        \
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

// Expands to an enum getter and setter.
#define ENUM_ACCESSORS_IMPL(Receiver, receiver, type_t, Field, field)          \
__MAPPING_SETTER_IMPL__(Receiver, receiver, type_t, Field, field,              \
    __NEW_INTEGER_MAP__);                                                      \
__MAPPING_GETTER_IMPL__(Receiver, receiver, type_t, Field, field,              \
    __GET_INTEGER_VALUE_MAP__)


// --- S p e c i e s   a c c e s s o r s ---

// Expands to a function that gets the specified field in the specified division
// of species.
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

// Expands to function implementations that get and set checked values on a
// particular kind of species.
#define CHECKED_SPECIES_ACCESSORS_IMPL(Receiver, receiver, ReceiverSpecies,    \
    receiver_species, Family, Field, field)                                    \
void set_##receiver_species##_species_##field(value_t self, value_t value) {   \
  CHECK_DIVISION(sd##ReceiverSpecies, self);                                   \
  CHECK_FAMILY(of##Family, value);                                             \
  *access_object_field(self, k##ReceiverSpecies##Species##Field##Offset) = value; \
}                                                                              \
SPECIES_GETTER_IMPL(Receiver, receiver, ReceiverSpecies, receiver_species,     \
    Field, field)


// --- B u i l t i n s ---

#define ADD_BUILTIN(family, name, argc, impl)                                  \
  TRY(add_methodspace_builtin_method(runtime, deref(s_space),                  \
      ROOT(runtime, family##_protocol), name, argc, impl))


#endif // _VALUE_INL
