#include "behavior.h"
#include "value-inl.h"

// --- V a l i d a t i o n ---

// Checks whether the expression holds and if not returns a validation failure.
#define VALIDATE(EXPR) do {                \
  if (!(EXPR))                             \
    return new_signal(scValidationFailed); \
} while (false)

// Checks whether the value at the end of the given pointer belongs to the
// specified family. If not, returns a validation failure.
#define VALIDATE_VALUE_FAMILY(ofFamily, EXPR) \
VALIDATE(in_family(ofFamily, EXPR))

value_t object_validate(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  VALIDATE_VALUE_FAMILY(ofSpecies, species);
  behavior_t *behavior = get_species_behavior(species);
  return (behavior->validate)(value);
}

static value_t validate_string(value_t value) {
  // Check that the string is null-terminated.
  size_t length = get_string_length(value);
  VALIDATE(get_string_chars(value)[length] == '\0');
  return non_signal();
}

static value_t validate_species(value_t value) {
  VALIDATE_VALUE_FAMILY(ofSpecies, value);
  return non_signal();
}

static value_t validate_array(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArray, value);
  return non_signal();
}

static value_t validate_null(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNull, value);
  return non_signal();
}

// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_BEHAVIOR(Family, family) \
behavior_t k##Family##Behavior = {      \
  &validate_##family                    \
};
ENUM_FAMILIES(DEFINE_BEHAVIOR)
#undef DEFINE_BEHAVIOR
