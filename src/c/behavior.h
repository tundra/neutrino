// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.


#ifndef _BEHAVIOR
#define _BEHAVIOR

#include "safe.h"
#include "value.h"

// A description of the layout of an object. See details about object layout in
// value.md.
typedef struct {
  // Size in bytes of the whole object.
  size_t size;
  // The offset in bytes within the object where the contiguous block of value
  // pointers start.
  size_t value_offset;
} object_layout_t;

// Returns the current mode of the given value.
value_mode_t get_value_mode(value_t value);

// Sets the object's value mode. Values may do this in any number of ways,
// some of which may require the runtime which is why it is present. Returns
// a non-signal if setting succeeded, an InvalidModeChange if mode discipline
// was violated, which contains the current mode of the value, and possibly
// any other signals if for instance allocation was required which failed.
value_t set_value_mode(runtime_t *runtime, value_t self, value_mode_t mode);

// Sets the object's value mode without checking mode discipline.
value_t set_value_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode);

// Initializes the fields of an object layout struct.
void object_layout_init(object_layout_t *layout);

// Sets the fields of an object layout struct.
void object_layout_set(object_layout_t *layout, size_t size, size_t value_offset);

// Flags that control how values are printed. These can be or'ed together.
typedef enum {
  pfNone = 0x0,
  pfUnquote = 0x1
} print_flags_t;

// A collection of "virtual" methods that define how a particular family of
// objects behave.
struct family_behavior_t {
  // The family this behavior belongs to.
  object_family_t family;
  // Function for validating an object.
  value_t (*validate)(value_t value);
  // Calculates the transient identity hash.
  value_t (*transient_identity_hash)(value_t value, hash_stream_t *stream,
      cycle_detector_t *detector);
  // Returns true iff the two values are identical.
  value_t (*identity_compare)(value_t a, value_t b, cycle_detector_t *detector);
  // Returns a value indicating how a compares relative to b, if this kind of
  // object supports it. If this type doesn't support comparison this field
  // is NULL.
  value_t (*ordering_compare)(value_t a, value_t b);
  // Writes a string representation of the value on a string buffer. If the
  // depth is 0 you're not allowed to print other objects recursively, otherwise
  // it's fine as long as you decrease the depth by 1 when you do.
  void (*print_on)(value_t value, string_buffer_t *buf, print_flags_t flags,
      size_t depth);
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
  // Returns the current mode of the given value.
  value_mode_t (*get_mode)(value_t self);
  // Set the current mode of the given value to the given mode, possibly using
  // the given runtime. This must not check mode discipline.
  value_t (*set_mode_unchecked)(runtime_t *runtime, value_t self,
      value_mode_t mode);
  // Ensures that all values owned by this one are frozen. This should not
  // fail because of mode discipline but may fail if interacting with the
  // runtime fails.
  value_t (*ensure_owned_values_frozen)(runtime_t *runtime, value_t self);
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
//
// This should not be used to implement hash functions themselves, use
// value_transient_identity_hash_cycle_protect for that.
value_t value_transient_identity_hash(value_t value);

// Works the same as value_transient_identity_hash except that it catches
// cycles. If the hash of one object is calculated in terms of the hashes of
// others it must obtain those hashes by calling this, not
// value_transient_identity_hash.
value_t value_transient_identity_hash_cycle_protect(value_t value,
    hash_stream_t *stream, cycle_detector_t *detector);

// Returns true iff the two values are identical.
//
// This should not be used to implement identity comparison functions, use
// value_identity_compare_cycle_protect instead for that.
bool value_identity_compare(value_t a, value_t b);

// Works the same way as value_identity_compare except that it catches potential
// cycles.
value_t value_identity_compare_cycle_protect(value_t a, value_t b,
    cycle_detector_t *detector);

// Returns a value indicating how a and b relate in the total ordering of
// comparable values. If the values are not both comparable the result is
// undefined, it may return a comparison value but it may also return a signal.
// Don't depend on any particular behavior in that case.
value_t value_ordering_compare(value_t a, value_t b);

// Prints a human-readable representation of the given value on the given
// string buffer. The flags control how the value is printed, the depth
// indicates how deep into the object structure we want to go.
void value_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth);

// Same as value_print_on but passes default values for the flags and depth.
void value_print_default_on(value_t value, string_buffer_t *buf);

// Does the same as value_print_on but doesn't print quotes around a
// string.
void value_print_on_unquoted(value_t value, string_buffer_t *buf);

// The default depth to traverse values when printing.
static const size_t kDefaultPrintDepth = 3;

// The string printed when there is no depth left to print a value.
#define kBottomValuePlaceholder "-"

// Works the same as value_print_on but keeps track of recursion depth such
// that we can print subobjects without worrying about cycles.
void value_print_on_cycle_protect(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth);

// A shorthand for printing an inner value if the depth allows it and otherwise
// a marker, "-".
void value_print_inner_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth);

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
#define DECLARE_FAMILY_BEHAVIOR(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW) \
family_behavior_t k##Family##Behavior;
ENUM_OBJECT_FAMILIES(DECLARE_FAMILY_BEHAVIOR)
#undef DECLARE_FAMILY_BEHAVIOR

// Declare the functions that implement the behaviors too, that way they can be
// implemented wherever.
#define __DECLARE_FAMILY_FUNCTIONS__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW) \
value_t family##_validate(value_t value);                                      \
ID(                                                                            \
  value_t family##_transient_identity_hash(value_t value,                      \
    hash_stream_t *stream, cycle_detector_t *detector);,                       \
  )                                                                            \
ID(                                                                            \
  value_t family##_identity_compare(value_t a, value_t b,                      \
    cycle_detector_t *detector);,                                              \
  )                                                                            \
CM(                                                                            \
  value_t family##_ordering_compare(value_t a, value_t b);,                    \
  )                                                                            \
void family##_print_on(value_t value, string_buffer_t *buf,                    \
    print_flags_t flags, size_t depth);                                        \
NL(                                                                            \
  void get_##family##_layout(value_t value, object_layout_t *layout_out);,     \
  )                                                                            \
PT(                                                                            \
  value_t plankton_set_##family##_contents(value_t value, runtime_t *runtime,  \
    value_t contents);,                                                        \
  )                                                                            \
PT(                                                                            \
  value_t plankton_new_##family(runtime_t *runtime);,                          \
  )                                                                            \
SR(                                                                            \
  value_t get_##family##_protocol(value_t value, runtime_t *runtime);,         \
  )                                                                            \
SR(                                                                            \
  value_t add_##family##_builtin_methods(runtime_t *runtime,                   \
    safe_value_t s_space);,                                                    \
  )                                                                            \
FU(                                                                            \
  void fixup_##family##_post_migrate(runtime_t *runtime, value_t new_object,   \
    value_t old_object);,                                                      \
  )                                                                            \
MD(,                                                                           \
  value_mode_t get_##family##_mode(value_t self);                              \
  value_t set_##family##_mode_unchecked(runtime_t *runtime, value_t self,      \
      value_mode_t mode);)                                                     \
OW(                                                                            \
  value_t ensure_##family##_owned_values_frozen(runtime_t *runtime, value_t self);,\
  )
ENUM_OBJECT_FAMILIES(__DECLARE_FAMILY_FUNCTIONS__)
#undef __DECLARE_FAMILY_FUNCTIONS__

// Integers don't have a family but have built-in methods too.
value_t add_integer_builtin_methods(runtime_t *runtime, safe_value_t s_space);

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
