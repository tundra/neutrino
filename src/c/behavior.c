#include "behavior.h"
#include "syntax.h"
#include "value-inl.h"


// --- V a l i d a t e ---

value_t object_validate(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  VALIDATE_VALUE_FAMILY(ofSpecies, species);
  family_behavior_t *behavior = get_species_family_behavior(species);
  return (behavior->validate)(value);
}


// --- H e a p   s i z e ---

size_t get_object_heap_size(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  family_behavior_t *behavior = get_species_family_behavior(species);
  return (behavior->get_object_heap_size)(value);
}


// --- I d e n t i t y   h a s h ---

static value_t integer_transient_identity_hash(value_t value) {
  CHECK_DOMAIN(vdInteger, value);
  return value;
}

static value_t object_transient_identity_hash(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  family_behavior_t *behavior = get_species_family_behavior(species);
  return (behavior->transient_identity_hash)(value);
}

value_t value_transient_identity_hash(value_t value) {
  switch (get_value_domain(value)) {
    case vdInteger:
      return integer_transient_identity_hash(value);
    case vdObject:
      return object_transient_identity_hash(value);
    default:
      return new_signal(scUnsupportedBehavior);
  }
}


// --- I d e n t i t y ---

static bool integer_are_identical(value_t a, value_t b) {
  return (a == b);
}

static bool object_are_identical(value_t a, value_t b) {
  CHECK_DOMAIN(vdObject, a);
  CHECK_DOMAIN(vdObject, b);
  object_family_t a_family = get_object_family(a);
  object_family_t b_family = get_object_family(b);
  if (a_family != b_family)
    return false;
  value_t species = get_object_species(a);
  family_behavior_t *behavior = get_species_family_behavior(species);
  return (behavior->are_identical)(a, b);
}

bool value_are_identical(value_t a, value_t b) {
  // First check that they even belong to the same domain. Values can be equal
  // across domains.
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return false;
  // Then dispatch to the domain equals functions.
  switch (a_domain) {
    case vdInteger:
      return integer_are_identical(a, b);
    case vdObject:
      return object_are_identical(a, b);
    default:
      return false;
  }
}


// --- P r i n t i n g ---

static void integer_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdInteger, value);
  string_buffer_printf(buf, "%lli", get_integer_value(value));
}

static void signal_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdSignal, value);
  signal_cause_t cause = get_signal_cause(value);
  string_buffer_printf(buf, "%%<signal: %s>", signal_cause_name(cause));
}

static void object_print_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  family_behavior_t *behavior = get_species_family_behavior(species);
  (behavior->print_on)(value, buf);
}

static void object_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  family_behavior_t *behavior = get_species_family_behavior(species);
  (behavior->print_atomic_on)(value, buf);
}

void value_print_on(value_t value, string_buffer_t *buf) {
  switch (get_value_domain(value)) {
    case vdInteger:
      integer_print_atomic_on(value, buf);
      break;
    case vdObject:
      object_print_on(value, buf);
      break;
    case vdSignal:
      signal_print_atomic_on(value, buf);
      break;
    default:
      UNREACHABLE("value print on");
      break;
  }
}

void value_print_atomic_on(value_t value, string_buffer_t *buf) {
  switch (get_value_domain(value)) {
    case vdInteger:
      integer_print_atomic_on(value, buf);
      break;
    case vdObject:
      object_print_atomic_on(value, buf);
      break;
    default:
      UNREACHABLE("value print atomic on");
      break;
  }
}


// --- F r a m e w o r k ---

// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_OBJECT_FAMILY_BEHAVIOR(Family, family)                          \
family_behavior_t k##Family##Behavior = {                                      \
  &family##_validate,                                                          \
  &family##_transient_identity_hash,                                           \
  &family##_are_identical,                                                     \
  &family##_print_on,                                                          \
  &family##_print_atomic_on,                                                   \
  &get_##family##_heap_size                                                    \
};
ENUM_OBJECT_FAMILIES(DEFINE_OBJECT_FAMILY_BEHAVIOR)
#undef DEFINE_FAMILY_BEHAVIOR


// Define all the division behaviors. Similarly to families, when you add a new
// division you have to add the methods or this will break.
#define DEFINE_SPECIES_DIVISION_BEHAVIOR(Division, division)                   \
division_behavior_t k##Division##SpeciesBehavior = {                           \
  sd##Division,                                                                \
  &get_##division##_species_heap_size                                          \
};
ENUM_SPECIES_DIVISIONS(DEFINE_SPECIES_DIVISION_BEHAVIOR)
#undef DEFINE_SPECIES_DIVISION_BEHAVIOR
