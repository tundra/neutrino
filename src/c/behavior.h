// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.

#include "value.h"

#ifndef _BEHAVIOR
#define _BEHAVIOR

// A description of the layout of an object. See details about object layout in
// value.md.
typedef struct {
  // Size in bytes of the whole object.
  size_t size;
  // The offset in bytes within the object where the contiguous block of value
  // pointers start.
  size_t value_offset;
} object_layout_t;

// Initializes the fields of an object layout struct.
void object_layout_init(object_layout_t *layout);

// Sets the fields of an object layout struct.
void object_layout_set(object_layout_t *layout, size_t size, size_t value_offset);

// A collection of "virtual" methods that define how a particular family of
// objects behave.
struct family_behavior_t {
  // Function for validating an object.
  value_t (*validate)(value_t value);
  // Calculates the transient identity hash.
  value_t (*transient_identity_hash)(value_t value);
  // Returns true iff the two values are identical.
  bool (*identity_compare)(value_t a, value_t b);
  // Returns a value indicating how a compares relative to b, if this kind of
  // object supports it. If this type doesn't support comparison this field
  // is NULL.
  value_t (*ordering_compare)(value_t a, value_t b);
  // Writes a string representation of the value on a string buffer.
  void (*print_on)(value_t value, string_buffer_t *buf);
  // Writes an atomic (that is, doesn't recurse into contents) string
  // representation on a string buffer.
  void (*print_atomic_on)(value_t value, string_buffer_t *buf);
  // Stores the layout of the given object in the output layout struct.
  void (*get_object_layout)(value_t value, object_layout_t *layout_out);
  // Sets the contents of the given value from the given serialized contents.
  value_t (*set_contents)(value_t value, runtime_t *runtime,
      value_t contents);
  // Returns the protocol object for the given object.
  value_t (*get_protocol)(value_t value, runtime_t *runtime);
  // If non-NULL, performs a fixup step to the new object optionally using the
  // old object which is still intact except for a forward-pointer instead of
  // a header. The old object will not be used again so it can also just be
  // used as a block of memory.
  void (*post_migrate_fixup)(runtime_t *runtime, value_t new_object,
      value_t old_object);
};

// Validates an object. Check fails if validation fails except in soft check
// failure mode where a ValidationFailed signal is returned.
value_t object_validate(value_t value);

// Returns the size in bytes of the given object on the heap.
void get_object_layout(value_t value, object_layout_t *layout_out);

// Returns the transient identity hash of the given value. This hash is
// transient in the sense that it may be changed by garbage collection. It
// is an identity hash because it must be consistent with object identity,
// so two identical values must have the same hash.
value_t value_transient_identity_hash(value_t value);

// Returns true iff the two values are identical.
bool value_identity_compare(value_t a, value_t b);

// Returns a value indicating how a and b relate in the total ordering of
// comparable values. If the values are not both comparable the result is
// undefined, it may return a comparison value but it may also return a signal.
// Don't depend on any particular behavior in that case.
value_t value_ordering_compare(value_t a, value_t b);

// Prints a human-readable representation of the given value on the given
// string buffer.
void value_print_on(value_t value, string_buffer_t *buf);

// Prints a human-readable representation of the given value on the given
// string buffer without descending into objects that may cause cycles.
void value_print_atomic_on(value_t value, string_buffer_t *buf);

// Creates a new empty instance of the given type. Not all types support this,
// in which case an unsupported behavior signal is returned.
value_t new_object_with_type(runtime_t *runtime, value_t type);

// Sets the payload of an object, passing in the object to set and the data to
// inject as the object payload. If somehow the payload is not as the object
// expects a signal should be returned (as well as if anything else fails
// obviously).
value_t set_object_contents(runtime_t *runtime, value_t object,
    value_t payload);

// Returns the primary protocol of the given value.
value_t get_protocol(value_t value, runtime_t *runtime);

// Returns a value suitable to be returned as a hash from the address of an
// object.
#define OBJ_ADDR_HASH(VAL) new_integer((VAL).encoded)

// Declare the behavior structs for all the families on one fell swoop.
#define DECLARE_FAMILY_BEHAVIOR(Family, family, CMP, CID, CNT, SUR, NOL, FIX)  \
extern family_behavior_t k##Family##Behavior;
ENUM_OBJECT_FAMILIES(DECLARE_FAMILY_BEHAVIOR)
#undef DECLARE_FAMILY_BEHAVIOR

// Declare the functions that implement the behaviors too, that way they can be
// implemented wherever.
#define __DECLARE_FAMILY_FUNCTIONS__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)  \
value_t family##_validate(value_t value);                                      \
CID(value_t family##_transient_identity_hash(value_t value);,)                 \
CID(bool family##_identity_compare(value_t a, value_t b);,)                    \
CMP(value_t family##_ordering_compare(value_t a, value_t b);,)                 \
void family##_print_on(value_t value, string_buffer_t *buf);                   \
void family##_print_atomic_on(value_t value, string_buffer_t *buf);            \
NOL(void get_##family##_layout(value_t value, object_layout_t *layout_out);,)  \
CNT(value_t set_##family##_contents(value_t value, runtime_t *runtime,         \
    value_t contents);,)                                                       \
SUR(value_t get_##family##_protocol(value_t value, runtime_t *runtime);,)      \
SUR(value_t add_##family##_builtin_methods(runtime_t *runtime,                 \
    value_t space);,)                                                          \
FIX(void fixup_##family##_post_migrate(runtime_t *runtime, value_t new_object, \
    value_t old_object);,)
ENUM_OBJECT_FAMILIES(__DECLARE_FAMILY_FUNCTIONS__)
#undef __DECLARE_FAMILY_FUNCTIONS__

// Integers don't have a family but have built-in methods too.
value_t add_integer_builtin_methods(runtime_t *runtime, value_t space);

// Virtual methods that control how the species of a particular division behave.
struct division_behavior_t {
  // The division this behavior belongs to.
  species_division_t division;
  // Returns the size in bytes on the heap of species for this division.
  void (*get_species_layout)(value_t species, object_layout_t *layout_out);
};

// Declare the division behavior structs.
#define DECLARE_DIVISION_BEHAVIOR(Division, division)                          \
extern division_behavior_t k##Division##SpeciesBehavior;
ENUM_SPECIES_DIVISIONS(DECLARE_DIVISION_BEHAVIOR)
#undef DECLARE_DIVISION_BEHAVIOR

// Declare all the division behaviors.
#define DECLARE_DIVISION_BEHAVIOR_IMPLS(Division, division)                    \
void get_##division##_species_layout(value_t species, object_layout_t *layout_out);
ENUM_SPECIES_DIVISIONS(DECLARE_DIVISION_BEHAVIOR_IMPLS)
#undef DECLARE_DIVISION_BEHAVIOR_IMPLS


#endif // _BEHAVIOR
