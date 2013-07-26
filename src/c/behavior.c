#include "alloc.h"
#include "behavior.h"
#include "runtime.h"
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


// --- L a y o u t ---

void object_layout_init(object_layout_t *layout) {
  layout->size = 0;
}

void object_layout_set(object_layout_t *layout, size_t size) {
  layout->size = size;
}

void get_object_layout(value_t value, object_layout_t *layout_out) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  family_behavior_t *behavior = get_species_family_behavior(species);
  (behavior->get_object_layout)(value, layout_out);
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


// --- N e w   i n s t a n c e ---

static value_t new_instance_of_factory(runtime_t *runtime, value_t type) {
  value_t constr_wrapper = get_factory_constructor(type);
  void *constr_ptr = get_void_p_value(constr_wrapper);
  factory_constructor_t *constr = (factory_constructor_t*) (intptr_t) constr_ptr;
  return constr(runtime);
}

static value_t new_object_with_object_type(runtime_t *runtime, value_t type) {
  switch (get_object_family(type)) {
    case ofNull:
      // For now we use null to indicate an instance. Later this should be
      // replaced by something else, something species-like possibly.
      return new_heap_instance(runtime);
    case ofFactory:
      return new_instance_of_factory(runtime, type);
    default:
      return new_signal(scUnsupportedBehavior);
  }
}

value_t new_object_with_type(runtime_t *runtime, value_t type) {
  switch (get_value_domain(type)) {
    case vdObject:
      return new_object_with_object_type(runtime, type);
    default:
      return new_signal(scUnsupportedBehavior);
  }
}


// --- P a y l o a d ---

value_t set_object_payload(runtime_t *runtime, value_t object, value_t payload) {
  CHECK_DOMAIN(vdObject, object);
  value_t species = get_object_species(object);
  VALIDATE_VALUE_FAMILY(ofSpecies, species);
  family_behavior_t *behavior = get_species_family_behavior(species);
  return (behavior->set_contents)(object, runtime, payload);
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
  &get_##family##_layout,                                                      \
  NULL,                                                                        \
};
ENUM_OBJECT_FAMILIES(DEFINE_OBJECT_FAMILY_BEHAVIOR)
#undef DEFINE_FAMILY_BEHAVIOR


// Define all the division behaviors. Similarly to families, when you add a new
// division you have to add the methods or this will break.
#define DEFINE_SPECIES_DIVISION_BEHAVIOR(Division, division)                   \
division_behavior_t k##Division##SpeciesBehavior = {                           \
  sd##Division,                                                                \
  &get_##division##_species_layout                                             \
};
ENUM_SPECIES_DIVISIONS(DEFINE_SPECIES_DIVISION_BEHAVIOR)
#undef DEFINE_SPECIES_DIVISION_BEHAVIOR
