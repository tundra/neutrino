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
  if (!(EXPR))                                                                 \
    return new_signal(scValidationFailed);                                     \
} while (false)

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_VALUE_FAMILY(ofFamily, EXPR)                                  \
VALIDATE(in_family(ofFamily, EXPR))

// Declares the identity and identity hash functions for a value family that
// uses object identity.
#define OBJECT_IDENTITY_IMPL(family)                                           \
value_t family##_transient_identity_hash(value_t value) {                      \
  return OBJ_ADDR_HASH(value);                                                 \
}                                                                              \
bool family##_are_identical(value_t a, value_t b) {                            \
  return (a == b);                                                             \
}                                                                              \
SWALLOW_SEMI()

// Declares a set_contents function that returns a signal indicating that this
// family doesn't support setting contents.
#define CANT_SET_CONTENTS(family)                                              \
value_t set_##family##_contents(value_t value, struct runtime_t *runtime,      \
    value_t contents) {                                                        \
  return new_signal(scUnsupportedBehavior);                                    \
}                                                                              \
SWALLOW_SEMI()

// Declares the heap size functions for a fixed-size object.
#define FIXED_SIZE_IMPL(family, Family)                                        \
void get_##family##_layout(value_t value, object_layout_t *layout_out) {       \
  object_layout_set(layout_out, k##Family##Size);                              \
}                                                                              \
SWALLOW_SEMI()

#endif // _VALUE_INL
