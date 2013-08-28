// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "log.h"
#include "runtime.h"
#include "syntax.h"
#include "value-inl.h"


// --- V a l i d a t e ---

value_t object_validate(value_t value) {
  family_behavior_t *behavior = get_object_family_behavior(value);
  return (behavior->validate)(value);
}


// --- L a y o u t ---

void object_layout_init(object_layout_t *layout) {
  layout->size = 0;
  layout->value_offset = 0;
}

void object_layout_set(object_layout_t *layout, size_t size, size_t value_offset) {
  layout->size = size;
  layout->value_offset = value_offset;
}

void get_object_layout(value_t self, object_layout_t *layout_out) {
  // This has to work during gc so some of the normal behavior checks are
  // disabled.
  CHECK_DOMAIN(vdObject, self);
  // We only get the layout of objects that have already been moved so this
  // gives a proper species.
  value_t species = get_object_species(self);
  // The species itself may have been moved but in any its memory will still be
  // intact enough that we can get the behavior out.
  family_behavior_t *behavior = get_species_family_behavior(species);
  (behavior->get_object_layout)(self, layout_out);
}

// Declares the heap size functions for a fixed-size object that don't have any
// non-value fields.
#define __TRIVIAL_LAYOUT_FUNCTION__(Family, family)                            \
static void get_##family##_layout(value_t value, object_layout_t *layout_out) {\
  object_layout_set(layout_out, k##Family##Size, kObjectHeaderSize);           \
}

// Generate all the trivial layout functions since we know what they'll look
// like.
#define __DEFINE_TRIVIAL_LAYOUT_FUNCTION__(Family, family, CMP, CID, CNT, SUR, NOL, FIX) \
NOL(,__TRIVIAL_LAYOUT_FUNCTION__(Family, family))
  ENUM_OBJECT_FAMILIES(__DEFINE_TRIVIAL_LAYOUT_FUNCTION__)
#undef __DEFINE_TRIVIAL_LAYOUT_FUNCTION__
#undef __TRIVIAL_LAYOUT_FUNCTION__

// --- I d e n t i t y   h a s h ---

static value_t integer_transient_identity_hash(value_t self) {
  CHECK_DOMAIN(vdInteger, self);
  return self;
}

static value_t default_object_transient_identity_hash(value_t value) {
  return OBJ_ADDR_HASH(value);
}

static value_t object_transient_identity_hash(value_t self) {
  family_behavior_t *behavior = get_object_family_behavior(self);
  return (behavior->transient_identity_hash)(self);
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

static bool integer_identity_compare(value_t a, value_t b) {
  return (a.encoded == b.encoded);
}

static bool default_object_identity_compare(value_t a, value_t b) {
  return (a.encoded == b.encoded);
}

static bool object_identity_compare(value_t a, value_t b) {
  CHECK_DOMAIN(vdObject, a);
  CHECK_DOMAIN(vdObject, b);
  object_family_t a_family = get_object_family(a);
  object_family_t b_family = get_object_family(b);
  if (a_family != b_family)
    return false;
  family_behavior_t *behavior = get_object_family_behavior(a);
  return (behavior->identity_compare)(a, b);
}

bool value_identity_compare(value_t a, value_t b) {
  // First check that they even belong to the same domain. Values can be equal
  // across domains.
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return false;
  // Then dispatch to the domain equals functions.
  switch (a_domain) {
    case vdInteger:
      return integer_identity_compare(a, b);
    case vdObject:
      return object_identity_compare(a, b);
    default:
      return false;
  }
}


// --- C o m p a r i n g ---

static value_t integer_ordering_compare(value_t a, value_t b) {
  // TODO: make this work at the boundaries.
  return int_to_ordering(get_integer_value(a) - get_integer_value(b));
}

static value_t object_ordering_compare(value_t a, value_t b) {
  CHECK_DOMAIN(vdObject, a);
  CHECK_DOMAIN(vdObject, b);
  object_family_t a_family = get_object_family(a);
  object_family_t b_family = get_object_family(b);
  if (a_family != b_family)
    // This may cause us to return a valid result even when a and b are not
    // comparable.
    return int_to_ordering(a_family - b_family);
  family_behavior_t *behavior = get_object_family_behavior(a);
  value_t (*ordering_compare)(value_t a, value_t b) = behavior->ordering_compare;
  if (ordering_compare == NULL) {
    return new_signal(scNotComparable);
  } else {
    return ordering_compare(a, b);
  }
}

value_t value_ordering_compare(value_t a, value_t b) {
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain) {
    // This may cause us to return a valid result even when a and b are not
    // comparable.
    return int_to_ordering(a_domain - b_domain);
  } else {
    switch (a_domain) {
      case vdInteger:
        return integer_ordering_compare(a, b);
      case vdObject:
        return object_ordering_compare(a, b);
      default:
        return new_signal(scNotComparable);
    }
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
  uint32_t details = get_signal_details(value);
  string_buffer_printf(buf, "%%<signal: %s dt@%i>", get_signal_cause_name(cause),
      details);
}

static void object_print_on(value_t value, string_buffer_t *buf) {
  family_behavior_t *behavior = get_object_family_behavior(value);
  (behavior->print_on)(value, buf);
}

static void object_print_atomic_on(value_t value, string_buffer_t *buf) {
  family_behavior_t *behavior = get_object_family_behavior(value);
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
      return new_heap_instance(runtime, runtime->roots.empty_instance_species);
    case ofFactory:
      return new_instance_of_factory(runtime, type);
    default: {
      value_to_string_t data;
      WARN("Invalid type %s", value_to_string(&data, type));
      dispose_value_to_string(&data);
      return new_signal(scUnsupportedBehavior);
    }
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

value_t set_object_contents(runtime_t *runtime, value_t object, value_t payload) {
  family_behavior_t *behavior = get_object_family_behavior(object);
  return (behavior->set_contents)(object, runtime, payload);
}

// A function compatible with set_contents that always returns unsupported.
static value_t set_contents_unsupported(value_t value, runtime_t *runtime,
    value_t contents) {
  return new_signal(scUnsupportedBehavior);
}


// --- P r o t o c o l ---

static value_t get_object_protocol(value_t self, runtime_t *runtime) {
  family_behavior_t *behavior = get_object_family_behavior(self);
  return (behavior->get_protocol)(self, runtime);
}

value_t get_protocol(value_t self, runtime_t *runtime) {
  switch (get_value_domain(self)) {
    case vdInteger:
      return runtime->roots.integer_protocol;
    case vdObject:
      return get_object_protocol(self, runtime);
    default:
      return new_signal(scUnsupportedBehavior);
  }
}

static value_t get_internal_object_protocol(value_t self, runtime_t *runtime) {
  return new_signal(scInternalFamily);
}


// --- F r a m e w o r k ---

// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_OBJECT_FAMILY_BEHAVIOR(Family, family, CMP, CID, CNT, SUR, NOL, FIX) \
family_behavior_t k##Family##Behavior = {                                      \
  &family##_validate,                                                          \
  CID(&family##_transient_identity_hash, default_object_transient_identity_hash), \
  CID(&family##_identity_compare, &default_object_identity_compare),           \
  CMP(&family##_ordering_compare, NULL),                                       \
  &family##_print_on,                                                          \
  &family##_print_atomic_on,                                                   \
  &get_##family##_layout,                                                      \
  CNT(&set_##family##_contents, &set_contents_unsupported),                    \
  SUR(&get_##family##_protocol, &get_internal_object_protocol),                \
  FIX(&fixup_##family##_post_migrate, NULL)                                    \
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
