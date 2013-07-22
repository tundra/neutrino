// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.

#include "value.h"

#ifndef _BEHAVIOR
#define _BEHAVIOR

// Forward declare the runtime struct to make it available as an argument.
struct runtime_t;

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

// Prints a human-readable representation of the given value on the given
// string buffer without descending into objects that may cause cycles.
void value_print_atomic_on(value_t value, string_buffer_t *buf);

// Creates a new empty instance of the given type. Not all types support this,
// in which case an unsupported behavior signal is returned.
value_t new_instance_of_value(struct runtime_t *runtime, value_t type);

// Returns a value suitable to be returned as a hash from the address of an
// object.
#define OBJ_ADDR_HASH(VAL) new_integer((size_t) (VAL))

// Declare the behavior structs for all the families on one fell swoop.
#define DECLARE_FAMILY_BEHAVIOR(Family, family) \
extern family_behavior_t k##Family##Behavior;
ENUM_OBJECT_FAMILIES(DECLARE_FAMILY_BEHAVIOR)
#undef DECLARE_FAMILY_BEHAVIOR

// Declare the functions that implement the behaviors too, that way they can be
// implemented wherever.
#define DECLARE_FAMILY_BEHAVIOR_IMPLS(Family, family)                          \
value_t family##_validate(value_t value);                                      \
value_t family##_transient_identity_hash(value_t value);                       \
bool family##_are_identical(value_t a, value_t b);                             \
void family##_print_on(value_t value, string_buffer_t *buf);                   \
void family##_print_atomic_on(value_t value, string_buffer_t *buf);            \
size_t get_##family##_heap_size(value_t value);
ENUM_OBJECT_FAMILIES(DECLARE_FAMILY_BEHAVIOR_IMPLS)
#undef DECLARE_FAMILY_BEHAVIOR_IMPLS


// Virtual methods that control how the species of a particular division behave.
typedef struct division_behavior_t {
  // The division this behavior belongs to.
  species_division_t division;
  // Returns the size in bytes on the heap of species for this division.
  size_t (*get_species_heap_size)(value_t species);
} division_behavior_t;

// Declare the division behavior structs.
#define DECLARE_DIVISION_BEHAVIOR(Division, division) \
extern division_behavior_t k##Division##SpeciesBehavior;
ENUM_SPECIES_DIVISIONS(DECLARE_DIVISION_BEHAVIOR)
#undef DECLARE_DIVISION_BEHAVIOR

// Declare all the division behaviors.
#define DECLARE_DIVISION_BEHAVIOR_IMPLS(Division, division)                    \
size_t get_##division##_species_heap_size(value_t species);
ENUM_SPECIES_DIVISIONS(DECLARE_DIVISION_BEHAVIOR_IMPLS)
#undef DECLARE_DIVISION_BEHAVIOR_IMPLS


#endif // _BEHAVIOR
