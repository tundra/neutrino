// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.

#include "value.h"

#ifndef _BEHAVIOR
#define _BEHAVIOR

// A unary method.
typedef value_t (unary_map_t)(value_t value);
typedef bool (binary_predicate_t)(value_t a, value_t b);

// A collection of "virtual" methods that define how a particular species
// behaves.
typedef struct behavior_t {
  // Function for validating an object.
  unary_map_t *validate;
  unary_map_t *transient_identity_hash;
  binary_predicate_t *are_identical;
} behavior_t;

// Validates an object.
value_t object_validate(value_t value);

// Returns the transient identity hash of the given value. This hash is
// transient in the sense that it may be changed by garbage collection. It
// is an identity hash because it must be consistent with object identity,
// so two identical values must have the same hash.
value_t value_transient_identity_hash(value_t value);

// Returns true iff the two values are identical.
bool value_are_identical(value_t a, value_t b);

// Declare the behavior structs for all the families on one fell swoop.
#define DECLARE_BEHAVIOR(Family, family) \
extern behavior_t k##Family##Behavior;
ENUM_FAMILIES(DECLARE_BEHAVIOR)
#undef DECLARE_BEHAVIOR

#endif // _BEHAVIOR
