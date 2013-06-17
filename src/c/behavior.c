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

static value_t string_validate(value_t value) {
  // Check that the string is null-terminated.
  size_t length = get_string_length(value);
  VALIDATE(get_string_chars(value)[length] == '\0');
  return success();
}

static value_t species_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofSpecies, value);
  return success();
}

static value_t array_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArray, value);
  return success();
}

static value_t map_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofMap, value);
  VALIDATE_VALUE_FAMILY(ofMap, get_map_entry_array(value));
  return success();
}

static value_t null_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNull, value);
  return success();
}

static value_t bool_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofBool, value);
  bool which = get_bool_value(value);
  VALIDATE((which == true) || (which == false));
  return success();
}


// --- H a s h ---

static value_t integer_transient_identity_hash(value_t value) {
  CHECK_DOMAIN(vdInteger, value);
  return value;
}

static value_t object_transient_identity_hash(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  behavior_t *behavior = get_species_behavior(species);
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

static value_t string_transient_identity_hash(value_t value) {
  string_t contents;
  get_string_contents(value, &contents);
  size_t hash = string_hash(&contents);
  return new_integer(hash);
}

static value_t bool_transient_identity_hash(value_t value) {
  static const size_t kTrueHash = 0x3213;
  static const size_t kFalseHash = 0x5423;
  return get_bool_value(value) ? kTrueHash : kFalseHash;
}

static value_t null_transient_identity_hash(value_t value) {
  static const size_t kNullHash = 0x4323;
  return kNullHash;
}

static value_t species_transient_identity_hash(value_t value) {
  return new_signal(scUnsupportedBehavior);
}

static value_t array_transient_identity_hash(value_t value) {
  return new_signal(scUnsupportedBehavior);
}

static value_t map_transient_identity_hash(value_t value) {
  return new_signal(scUnsupportedBehavior);
}


// --- E q u a l s ---

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
  behavior_t *behavior = get_species_behavior(species);
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

static bool string_are_identical(value_t a, value_t b) {
  string_t a_contents;
  get_string_contents(a, &a_contents);
  string_t b_contents;
  get_string_contents(b, &b_contents);
  return string_equals(&a_contents, &b_contents);
}

static bool null_are_identical(value_t a, value_t b) {
  // There is only one null so you should never end up comparing two different
  // ones.
  CHECK_EQ(a, b);
  return true;
}

static bool bool_are_identical(value_t a, value_t b) {
  // There is only one true and false which are both only equal to themselves.
  return (a == b);
}

static bool species_are_identical(value_t a, value_t b) {
  // Species compare using object identity.
  return (a == b);
}

static bool array_are_identical(value_t a, value_t b) {
  // Arrays compare using object identity.
  return (a == b);
}

static bool map_are_identical(value_t a, value_t b) {
  // Maps compare using object identity.
  return (a == b);
}


// --- P r i n t ---

// Print a value atomically, that is, without recursively printing any
// elements contained in the value.
void value_print_atomic_on(value_t value, string_buffer_t *buf);

static void integer_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdInteger, value);
  string_buffer_printf(buf, "%lli", get_integer_value(value));
}

static void object_print_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  behavior_t *behavior = get_species_behavior(species);
  (behavior->print_on)(value, buf);
}

static void object_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdObject, value);
  value_t species = get_object_species(value);
  behavior_t *behavior = get_species_behavior(species);
  (behavior->print_atomic_on)(value, buf);  
}

static void null_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "null");
}

static void null_print_on(value_t value, string_buffer_t *buf) {
  null_print_atomic_on(value, buf);
}

static void bool_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, get_bool_value(value) ? "true" : "false");
}

static void bool_print_on(value_t value, string_buffer_t *buf) {
  bool_print_atomic_on(value, buf);
}

static void species_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<species>");
}

static void species_print_on(value_t value, string_buffer_t *buf) {
  species_print_atomic_on(value, buf);
}

static void array_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<array[%i]>", (int) get_array_length(value));
}

static void array_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "[");
  for (size_t i = 0; i < get_array_length(value); i++) {
    if (i > 0)
      string_buffer_printf(buf, ", ");
    value_print_atomic_on(get_array_at(value, i), buf);
  }
  string_buffer_printf(buf, "]");
}

static void map_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<map{%i}>", (int) get_map_size(value));
}

static void map_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "{");
  map_iter_t iter;
  map_iter_init(&iter, value);
  while (map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    map_iter_get_current(&iter, &key, &value);
    value_print_on(key, buf);
    string_buffer_printf(buf, ": ");
    value_print_on(value, buf);
  }
  string_buffer_printf(buf, "}");
}

static void string_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_t contents;
  get_string_contents(value, &contents);
  string_buffer_printf(buf, "\"%s\"", contents.chars);
}

static void string_print_on(value_t value, string_buffer_t *buf) {
  string_print_atomic_on(value, buf);
}

void value_print_on(value_t value, string_buffer_t *buf) {
  switch (get_value_domain(value)) {
    case vdInteger:
      integer_print_atomic_on(value, buf);
      break;
    case vdObject:
      object_print_on(value, buf);
      break;
    default:
      UNREACHABLE();
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
      UNREACHABLE();
  }
}


// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_BEHAVIOR(Family, family) \
behavior_t k##Family##Behavior = {      \
  &family##_validate,                   \
  &family##_transient_identity_hash,    \
  &family##_are_identical,              \
  &family##_print_on,                   \
  &family##_print_atomic_on             \
};
ENUM_FAMILIES(DEFINE_BEHAVIOR)
#undef DEFINE_BEHAVIOR
