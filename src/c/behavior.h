// Behavior specific to each family of objects. It's just like virtual methods
// except done manually using function pointers.

#include "value.h"

#ifndef _BEHAVIOR
#define _BEHAVIOR

// The type of the validate behavior.
typedef value_t (validate_t)(value_t value);

// A collection of "virtual" methods that define how a particular species
// behaves.
typedef struct behavior_t {
  // Function for validating an object.
  validate_t *validate;
} behavior_t;

// Validates an object.
value_t object_validate(value_t value);

// Returns the hash of the given value or a signal if the value cannot be
// hashed.
value_t value_hash(value_t value);

// Returns true iff the two values are equal.
bool value_equals(value_t a, value_t b);

// Declare the behavior structs for all the families on one fell swoop.
#define DECLARE_BEHAVIOR(Family, family) \
extern behavior_t k##Family##Behavior;
ENUM_FAMILIES(DECLARE_BEHAVIOR)
#undef DECLARE_BEHAVIOR

#endif // _BEHAVIOR
