// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.

#include "value.h"

#ifndef _BEHAVIOR
#define _BEHAVIOR

// A collection of "virtual" methods that define how a particular family of
// objects behave.
typedef struct family_behavior_t {
  // Function for validating an object.
  value_t (*validate)(value_t value);
  // Calculates the transient identity hash.
  value_t (*transient_identity_hash)(value_t value);
  // Returns true iff the two values are identical.
  bool (*are_identical)(value_t a, value_t b);
  // Writes a string representation of the value on a string buffer.
  void (*print_on)(value_t value, string_buffer_t *buf);
  // Writes an atomic (that is, doesn't recurse into contents) string
  // representation on a string buffer.
  void (*print_atomic_on)(value_t value, string_buffer_t *buf);
  // Returns the size in bytes on the heap of the given object.
  size_t (*get_object_heap_size)(value_t value);
} family_behavior_t;

// Validates an object.
value_t object_validate(value_t value);

// Returns the size in bytes of the given object on the heap.
size_t get_object_heap_size(value_t value);

// Returns the transient identity hash of the given value. This hash is
// transient in the sense that it may be changed by garbage collection. It
// is an identity hash because it must be consistent with object identity,
// so two identical values must have the same hash.
value_t value_transient_identity_hash(value_t value);

// Returns true iff the two values are identical.
bool value_are_identical(value_t a, value_t b);

// Prints a human-readable representation of the given value on the given
// string buffer.
void value_print_on(value_t value, string_buffer_t *buf);

// Declare the behavior structs for all the families on one fell swoop.
#define DECLARE_FAMILY_BEHAVIOR(Family, family) \
extern family_behavior_t k##Family##Behavior;
ENUM_OBJECT_FAMILIES(DECLARE_FAMILY_BEHAVIOR)
#undef DECLARE_FAMILY_BEHAVIOR


// Virtual methods that control how the species of a particular division behave.
typedef struct division_behavior_t {
  // The division this behavior belongs to.
  species_division_t division;
  // Returns the size in bytes on the heap of species for this division.
  size_t (*get_species_heap_size)(value_t species);
} division_behavior_t;

#define DECLARE_DIVISION_BEHAVIOR(Division, division) \
extern division_behavior_t k##Division##SpeciesBehavior;
ENUM_SPECIES_DIVISIONS(DECLARE_DIVISION_BEHAVIOR)
#undef DECLARE_DIVISION_BEHAVIOR

#endif // _BEHAVIOR
