//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _VALUE_INL
#define _VALUE_INL

#include "check.h"
#include "condition.h"
#include "derived.h"
#include "runtime.h"
#include "tagged.h"
#include "try-inl.h"
#include "utils-inl.h"
#include "utils/strbuf.h"
#include "utils/string-inl.h"
#include "value.h"

// There are two flavors of tests: in-tests and is-tests. Both tests whether
// a value is within a particular group of values but where in-tests take the
// group to test for as an argument, is-tests hardcode a particular group. So
// is-tests are really just shorthands for an in-test with a particular argument
// but are shorter and it may be possible to optimize them when the value is
// hardcoded.

// Returns true iff the given species belongs to the given division.
static inline bool in_division(species_division_t division, value_t value) {
  return in_family(ofSpecies, value) && (get_species_division(value) == division);
}

// Returns true iff the value is a condition with the specified cause.
static inline bool in_condition_cause(condition_cause_t cause, value_t value) {
  return is_condition(value) && (get_condition_cause(value) == cause);
}

// Is the given value a heap exhausted condition?
static inline bool is_heap_exhausted_condition(value_t value) {
  return in_condition_cause(ccHeapExhausted, value);
}


// --- T u p l e   s h o r t h a n d s ---

// Returns the first entry in the given tuple.
static value_t get_tuple_first(value_t self) {
  return get_tuple_at(self, 0);
}

// Returns the second entry in the given tuple.
static value_t get_tuple_second(value_t self) {
  return get_tuple_at(self, 1);
}


/// ## Array

// Returns a value array pointing at the given range within the given array.
static value_array_t alloc_array_block(value_t self, size_t start, size_t length) {
  CHECK_TRUE("out of bounds", limit_within_array_bounds(self, start + length));
  return new_value_array(get_array_start(self) + start, length);
}


// ## Fifo buffer

// Returns the size of an individual fifo buffer node, given the width of the
// fifo buffer.
static inline size_t get_fifo_buffer_node_length_for_width(size_t width) {
  return width + kFifoBufferNodeHeaderSize;
}


// --- P r i n t i n g ---

// Helper data type for the shorthand for converting a value to a string.
// Converting to a string generates some data that needs to be disposed, this
// structure captures that so it can be disposed using dispose_value_to_string.
typedef struct {
  // The string representation of the value.
  utf8_t str;
  // The string buffer used to build the result.
  string_buffer_t buf;
} value_to_string_t;

// Converts the given value to a string.
const char *value_to_string(value_to_string_t *data, value_t value);

// Disposes the data buffer.
void value_to_string_dispose(value_to_string_t *data);


// --- V a l i d a t i o n ---

// Checks whether the expression holds and if not returns a validation failure.
#define VALIDATE(EXPR) do {                                                    \
  COND_CHECK_TRUE("validation", ccValidationFailed, EXPR);                     \
} while (false)

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_FAMILY(ofFamily, EXPR)                                        \
VALIDATE(in_family(ofFamily, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_FAMILY_OPT(ofFamily, EXPR)                                    \
VALIDATE(in_family_opt(ofFamily, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_FAMILY_OR_NULL(ofFamily, EXPR)                                \
VALIDATE(in_family_or_null(ofFamily, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified domain. If not, returns a validation failure.
#define VALIDATE_DOMAIN(vdDomain, EXPR)                                        \
VALIDATE(in_domain(vdDomain, EXPR))

// Checks whether the value at the end of the given pointer belongs to the
// specified domain. If not, returns a validation failure.
#define VALIDATE_DOMAIN_OPT(vdDomain, EXPR)                                    \
VALIDATE(in_domain_opt(vdDomain, EXPR))


// --- B e h a v i o r ---

// Expands to a trivial implementation of print_on.
#define TRIVIAL_PRINT_ON_IMPL(Family, family)                                  \
void family##_print_on(value_t value, print_on_context_t *context) {           \
  CHECK_FAMILY(of##Family, value);                                             \
  string_buffer_printf(context->buf, "#<" #family " ~%w>", value.encoded);     \
}                                                                              \
SWALLOW_SEMI(tpo)

// Expands to an implementation of the built-in method definition function that
// defines no built-ins.
#define NO_BUILTIN_METHODS(family)                                             \
value_t add_##family##_builtin_implementations(runtime_t *runtime,             \
    safe_value_t s_map) {                                                      \
  return success();                                                            \
}

// Expands to an implementation of get_primary_type that returns the canonical
// type for the value's family.
#define GET_FAMILY_PRIMARY_TYPE_IMPL(family)                                   \
value_t get_##family##_primary_type(value_t self, runtime_t *runtime) {        \
  return ROOT(runtime, family##_type);                                         \
}                                                                              \
SWALLOW_SEMI(gfpti)

// Expands to an implementation of get and set family_mode for a family with a
// fixed mode.
#define FIXED_GET_MODE_IMPL(family, vmMode)                                    \
value_mode_t get_##family##_mode(value_t self) {                               \
  return vmMode;                                                               \
}                                                                              \
value_t set_##family##_mode_unchecked(runtime_t *rt, value_t self, value_mode_t mode) { \
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
  return *access_heap_object_field(self, k##Receiver##Field##Offset);          \
}                                                                              \
SWALLOW_SEMI(gi)

// Expands to the same as ACCESSORS_IMPL but also gives a _frozen_ version of
// setter which allows fields of frozen objects to be set.
#define FROZEN_ACCESSORS_IMPL(Receiver, receiver, SENTRY, Field, field)        \
void init_frozen_##receiver##_##field(value_t self, value_t value) {           \
  CHECK_FAMILY(of##Receiver, self);                                            \
  CHECK_DEEP_FROZEN(value);                                                    \
  CHECK_SENTRY(SENTRY, value);                                                 \
  *access_heap_object_field(self, k##Receiver##Field##Offset) = value;         \
}                                                                              \
ACCESSORS_IMPL(Receiver, receiver, SENTRY, Field, field)

// Expands to a setter and a getter for the specified types. The setter will
// have a custom check on the value and a mutability check on the instance.
#define ACCESSORS_IMPL(Receiver, receiver, SENTRY, Field, field)               \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  CHECK_MUTABLE(self);                                                         \
  CHECK_SENTRY(SENTRY, value);                                                 \
  *access_heap_object_field(self, k##Receiver##Field##Offset) = value;         \
}                                                                              \
GETTER_IMPL(Receiver, receiver, Field, field)

// Expands to a function that gets the specified field in the specified object
// family.
#define DERIVED_GETTER_IMPL(Receiver, receiver, Field, field)                  \
value_t get_##receiver##_##field(value_t self) {                               \
  return *access_derived_object_field(self, k##Receiver##Field##Offset);       \
}                                                                              \
SWALLOW_SEMI(gi)

// Expands to an unchecked setter and a getter for the specified types.
#define DERIVED_ACCESSORS_IMPL(Receiver, receiver, SENTRY, Field, field)       \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_SENTRY(SENTRY, value);                                                 \
  *access_derived_object_field(self, k##Receiver##Field##Offset) = value;      \
}                                                                              \
DERIVED_GETTER_IMPL(Receiver, receiver, Field, field)


// --- I n t e g e r / e n u m   a c c e s s o r s ---

#define __MAPPING_SETTER_IMPL__(Receiver, receiver, type_t, Field, field, MAP) \
void set_##receiver##_##field(value_t self, type_t value) {                    \
  CHECK_FAMILY(of##Receiver, self);                                            \
  *access_heap_object_field(self, k##Receiver##Field##Offset) = MAP(type_t, value); \
}                                                                              \
SWALLOW_SEMI(msi)

#define __MAPPING_GETTER_IMPL__(Receiver, receiver, type_t, Field, field, MAP) \
type_t get_##receiver##_##field(value_t self) {                                \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return (type_t) MAP(type_t, *access_heap_object_field(self, k##Receiver##Field##Offset)); \
}                                                                              \
SWALLOW_SEMI(mgi)

#define __NEW_INTEGER_MAP__(T, N) new_integer(N)
#define __GET_INTEGER_VALUE_MAP__(T, N) get_integer_value(N)

// Expands to an integer getter and setter.
#define INTEGER_ACCESSORS_IMPL(Receiver, receiver, Field, field)               \
__MAPPING_SETTER_IMPL__(Receiver, receiver, int64_t, Field, field,             \
    __NEW_INTEGER_MAP__);                                                      \
__MAPPING_GETTER_IMPL__(Receiver, receiver, int64_t, Field, field,             \
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
  return *access_heap_object_field(self,                                       \
      k##ReceiverSpecies##Species##Field##Offset);                             \
}                                                                              \
value_t get_##receiver##_##field(value_t self) {                               \
  CHECK_FAMILY(of##Receiver, self);                                            \
  return get_##receiver_species##_species_##field(get_heap_object_species(self)); \
}                                                                              \
SWALLOW_SEMI(sgi)

// Expands to function implementations that get and set checked values on a
// particular kind of species.
#define CHECKED_SPECIES_ACCESSORS_IMPL(Receiver, receiver, ReceiverSpecies,    \
    receiver_species, SENTRY, Field, field)                                     \
void set_##receiver_species##_species_##field(value_t self, value_t value) {   \
  CHECK_DIVISION(sd##ReceiverSpecies, self);                                   \
  CHECK_SENTRY(SENTRY, value);                                                 \
  *access_heap_object_field(self, k##ReceiverSpecies##Species##Field##Offset) = value; \
}                                                                              \
SPECIES_GETTER_IMPL(Receiver, receiver, ReceiverSpecies, receiver_species,     \
    Field, field)


// --- B u i l t i n s ---

#define ADD_BUILTIN_IMPL(name, argc, impl)                                     \
  TRY(add_builtin_method_impl(runtime, deref(s_map), name, argc, impl, -1))

#define ADD_BUILTIN_IMPL_MAY_ESCAPE(name, argc, leave_argc, impl)              \
  TRY(add_builtin_method_impl(runtime, deref(s_map), name, argc, impl,         \
      leave_argc + kImplicitArgumentCount))


// --- P l a n k t o n ---

#define __CHECK_MAP_ENTRY_FOUND__(name) do {                                   \
  if (in_condition_cause(ccNotFound, name)) {                                  \
    string_hint_t __hint__ = STRING_HINT_INIT(#name);                          \
    return new_invalid_input_condition_with_hint(__hint__);                    \
  }                                                                            \
} while (false)

// Reads a single entry from a plankton map.
#define __GET_PLANKTON_MAP_ENTRY__(name)                                       \
  value_t name##_value = get_id_hash_map_at(__source__, RSTR(runtime, name));  \
  __CHECK_MAP_ENTRY_FOUND__(name##_value);

#define UNPACK_PLANKTON_FIELD(CHECK, name)                                     \
    value_t name##_value;                                                      \
    do {                                                                       \
      name##_value = get_id_hash_map_at(__source__, RSTR(runtime, name));      \
      __CHECK_MAP_ENTRY_FOUND__(name##_value);                                 \
      C_TRY(CHECK, name##_value);                                              \
    } while (false)

#define BEGIN_UNPACK_PLANKTON(SOURCE)                                          \
  value_t __source__ = (SOURCE);                                               \
  EXPECT_FAMILY(ofIdHashMap, __source__)

// Reads successive values from a plankton map, storing them in variables with
// names matching the keys of the values.
#define UNPACK_PLANKTON_MAP(SOURCE, ...)                                       \
  value_t __source__ = (SOURCE);                                               \
  EXPECT_FAMILY(ofIdHashMap, __source__);                                      \
  FOR_EACH_VA_ARG(__GET_PLANKTON_MAP_ENTRY__, __VA_ARGS__)                     \
  SWALLOW_SEMI(upm)


#endif // _VALUE_INL
