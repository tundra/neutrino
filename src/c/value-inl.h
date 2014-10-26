//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _VALUE_INL
#define _VALUE_INL

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

// Returns true if the value of the given expression is in the specified
// domain.
static inline bool in_domain(value_domain_t domain, value_t value) {
  return get_value_domain(value) == domain;
}

// Is the given value a tagged integer?
static inline bool is_integer(value_t value) {
  return in_domain(vdInteger, value);
}

// Returns true iff the given value is a condition.
static inline bool is_condition(value_t value) {
  return in_domain(vdCondition, value);
}

// Returns true iff the given value is a heap object.
static inline bool is_heap_object(value_t value) {
  return in_domain(vdHeapObject, value);
}

// Returns true iff the given value is a derived object.
static inline bool is_derived_object(value_t value) {
  return in_domain(vdDerivedObject, value);
}

// Returns true iff the given value is an object within the given family.
static inline bool in_family(heap_object_family_t family, value_t value) {
  return is_heap_object(value) && (get_heap_object_family(value) == family);
}

// Returns true iff the given value is either nothing or an object within the
// given family.
static inline bool in_family_opt(heap_object_family_t family, value_t value) {
  return is_nothing(value) || in_family(family, value);
}

// Returns true iff the given value is either nothing or a value within the
// given domain.
static inline bool in_domain_opt(value_domain_t domain, value_t value) {
  return is_nothing(value) || in_domain(domain, value);
}

// Returns true iff the given species belongs to the given division.
static inline bool in_division(species_division_t division, value_t value) {
  return in_family(ofSpecies, value) && (get_species_division(value) == division);
}

// Returns true iff the value is a condition with the specified cause.
static inline bool in_condition_cause(consition_cause_t cause, value_t value) {
  return is_condition(value) && (get_condition_cause(value) == cause);
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
  CHECK_REL("out of bounds", start + length, <=, get_array_length(self));
  return new_value_array(get_array_elements(self) + start, length);
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
void dispose_value_to_string(value_to_string_t *data);


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
void set_##family##_mode_unchecked(runtime_t *rt, value_t self, value_mode_t mode) { \
  CHECK_TRUE("invalid mode change",                                            \
      mode == vmMode || ((mode == vmFrozen) && (vmMode == vmDeepFrozen)));     \
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

// Accessor check that indicates that no check should be performed. A check that
// the value is not a condition will be performed in any case since that is a
// global invariant.
#define acNoCheck(UNUSED, VALUE)                                               \
  CHECK_FALSE("storing condition in heap", is_condition(VALUE))

// Accessor check that indicates that the argument should belong to the family
// specified in the argument.
#define acInFamily(ofFamily, VALUE)                                            \
  CHECK_FAMILY(ofFamily, (VALUE))

// Accessor check that indicates that the argument should either be nothing or
// belong to the family specified in the argument.
#define acInFamilyOpt(ofFamily, VALUE)                                         \
  CHECK_FAMILY_OPT(ofFamily, (VALUE))

// Accessor check that indicates that the argument should belong to the domain
// specified in the argument.
#define acInDomain(vdDomain, VALUE)                                            \
  CHECK_DOMAIN(vdDomain, (VALUE))

// Accessor check that indicates that the argument should either be nothing or
// belong to the domain specified in the argument.
#define acInDomainOpt(vdDomain, VALUE)                                         \
  CHECK_DOMAIN_OPT(vdDomain, (VALUE))

// Accessor check that indicates that the argument should either be nothing or
// belong to a syntax family.
#define acIsSyntaxOpt(UNUSED, VALUE)                                           \
  CHECK_SYNTAX_FAMILY_OPT(VALUE)

// Expands to the same as ACCESSORS_IMPL but also gives a _frozen_ version of
// setter which allows fields of frozen objects to be set.
#define FROZEN_ACCESSORS_IMPL(Receiver, receiver, acValueCheck, VALUE_CHECK_ARG, Field, field) \
void init_frozen_##receiver##_##field(value_t self, value_t value) {           \
  CHECK_FAMILY(of##Receiver, self);                                            \
  acValueCheck(VALUE_CHECK_ARG, value);                                        \
  *access_heap_object_field(self, k##Receiver##Field##Offset) = value;         \
}                                                                              \
ACCESSORS_IMPL(Receiver, receiver, acValueCheck, VALUE_CHECK_ARG, Field, field)

// Expands to a setter and a getter for the specified types. The setter will
// have a custom check on the value and a mutability check on the instance.
#define ACCESSORS_IMPL(Receiver, receiver, acValueCheck, VALUE_CHECK_ARG, Field, field) \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  CHECK_FAMILY(of##Receiver, self);                                            \
  CHECK_MUTABLE(self);                                                         \
  acValueCheck(VALUE_CHECK_ARG, value);                                        \
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
#define DERIVED_ACCESSORS_IMPL(Receiver, receiver, acValueCheck, VALUE_CHECK_ARG, Field, field) \
void set_##receiver##_##field(value_t self, value_t value) {                   \
  acValueCheck(VALUE_CHECK_ARG, value);                                        \
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
    receiver_species, acValueCheck, VALUE_CHECK_ARG, Field, field)             \
void set_##receiver_species##_species_##field(value_t self, value_t value) {   \
  CHECK_DIVISION(sd##ReceiverSpecies, self);                                   \
  acValueCheck(VALUE_CHECK_ARG, value);                                        \
  *access_heap_object_field(self, k##ReceiverSpecies##Species##Field##Offset) = value; \
}                                                                              \
SPECIES_GETTER_IMPL(Receiver, receiver, ReceiverSpecies, receiver_species,     \
    Field, field)


// --- B u i l t i n s ---

#define ADD_BUILTIN_IMPL(name, argc, impl)                                     \
  TRY(add_builtin_method_impl(runtime, deref(s_map), name, argc, impl, -1))

#define ADD_BUILTIN_IMPL_MAY_ESCAPE(name, argc, leave_argc, impl)              \
  TRY(add_builtin_method_impl(runtime, deref(s_map), name, argc, impl, leave_argc + 2))


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

// Reads successive values from a plankton map, storing them in variables with
// names matching the keys of the values.
#define UNPACK_PLANKTON_MAP(SOURCE, ...)                                       \
  value_t __source__ = (SOURCE);                                               \
  EXPECT_FAMILY(ccInvalidInput, ofIdHashMap, __source__);                      \
  FOR_EACH_VA_ARG(__GET_PLANKTON_MAP_ENTRY__, __VA_ARGS__)                     \
  SWALLOW_SEMI(upm)



#endif // _VALUE_INL
