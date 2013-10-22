// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "ctrino.h"
#include "log.h"
#include "runtime.h"
#include "syntax.h"
#include "try-inl.h"
#include "value-inl.h"


// --- V a l i d a t e ---

// Is the given family in a modal division?
static bool in_modal_division(object_family_t family) {
  switch (family) {
#define __GEN_CASE__(Family, family, CM, ID, CT, SR, NL, FU, EM, MD, OW)       \
    MD(                                                                        \
      case of##Family: return true;,                                           \
      )
    ENUM_OBJECT_FAMILIES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return false;
  }
}

// Validate that a given deep frozen object only points to other deep frozen
// objects.
static void deep_frozen_object_validate(value_t value) {
  value_field_iter_t iter;
  value_field_iter_init(&iter, value);
  value_t *field = NULL;
  while (value_field_iter_next(&iter, &field))
    CHECK_TRUE("deep frozen reference not deep frozen", peek_deep_frozen(*field));
}

value_t object_validate(value_t value) {
  family_behavior_t *behavior = get_object_family_behavior(value);
  CHECK_FALSE("Modal value with non-modal species",
      in_modal_division(behavior->family) && get_object_division(value) != sdModal);
  if (peek_deep_frozen(value))
    deep_frozen_object_validate(value);
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
#define __DEFINE_TRIVIAL_LAYOUT_FUNCTION__(Family, family, CM, ID, CT, SR, NL, FU, EM, MD, OW) \
NL(                                                                            \
  ,                                                                            \
  __TRIVIAL_LAYOUT_FUNCTION__(Family, family))
  ENUM_OBJECT_FAMILIES(__DEFINE_TRIVIAL_LAYOUT_FUNCTION__)
#undef __DEFINE_TRIVIAL_LAYOUT_FUNCTION__
#undef __TRIVIAL_LAYOUT_FUNCTION__


// --- M o d e ---

value_mode_t get_value_mode(value_t self) {
  if (get_value_domain(self) == vdObject) {
    family_behavior_t *behavior = get_object_family_behavior(self);
    return (behavior->get_mode)(self);
  } else {
    return vmDeepFrozen;
  }
}

value_t set_value_mode(runtime_t *runtime, value_t self, value_mode_t mode) {
  value_mode_t current_mode = get_value_mode(self);
  if (mode == current_mode) {
    // If we're already in the target mode this trivially succeeds.
    return success();
  } else if (mode > current_mode) {
    // It's always okay to set the object to a more restrictive mode.
    return set_value_mode_unchecked(runtime, self, mode);
  } else if (mode == vmFrozen) {
    // As a special case, it's okay to try to freeze an object that is already
    // deep frozen. It's a no-op.
    return success();
  } else {
    return new_invalid_mode_change_signal(current_mode);
  }
}

value_t set_value_mode_unchecked(runtime_t *runtime, value_t self, value_mode_t mode) {
  if (get_value_domain(self) == vdObject) {
    family_behavior_t *behavior = get_object_family_behavior(self);
    return (behavior->set_mode_unchecked)(runtime, self, mode);
  } else {
    CHECK_EQ("non-object not frozen", vmDeepFrozen, get_value_mode(self));
    CHECK_REL("invalid mode change", mode, >=, vmFrozen);
    return success();
  }
}


// --- I d e n t i t y   h a s h ---

static value_t integer_transient_identity_hash(value_t self,
    hash_stream_t *stream) {
  CHECK_DOMAIN(vdInteger, self);
  hash_stream_write_tags(stream, vdInteger, __ofUnknown__);
  hash_stream_write_int64(stream, get_integer_value(self));
  return success();
}

static value_t default_object_transient_identity_hash(value_t value,
    hash_stream_t *stream, cycle_detector_t *detector) {
  // object_transient_identity_hash has already written the tags.
  hash_stream_write_int64(stream, value.encoded);
  return success();
}

static value_t object_transient_identity_hash(value_t self,
    hash_stream_t *stream, cycle_detector_t *detector) {
  // The toplevel delegator functions are responsible for writing the tags,
  // that way the individual hashing functions don't all have to do that.
  family_behavior_t *behavior = get_object_family_behavior(self);
  hash_stream_write_tags(stream, vdObject, behavior->family);
  return (behavior->transient_identity_hash)(self, stream, detector);
}

value_t value_transient_identity_hash(value_t value) {
  hash_stream_t stream;
  hash_stream_init(&stream);
  cycle_detector_t detector;
  cycle_detector_init_bottom(&detector);
  TRY(value_transient_identity_hash_cycle_protect(value, &stream, &detector));
  int64_t hash = hash_stream_flush(&stream);
  // Discard the top three bits to make it fit in a tagged integer.
  return new_integer(hash >> 3);
}

value_t value_transient_identity_hash_cycle_protect(value_t value,
    hash_stream_t *stream, cycle_detector_t *detector) {
  value_domain_t domain = get_value_domain(value);
  switch (domain) {
    case vdInteger:
      return integer_transient_identity_hash(value, stream);
    case vdObject:
      return object_transient_identity_hash(value, stream, detector);
    default:
      return new_unsupported_behavior_signal(domain, __ofUnknown__,
          ubTransientIdentityHash);
  }
}


// --- I d e n t i t y ---

static value_t integer_identity_compare(value_t a, value_t b) {
  return to_internal_boolean(is_same_value(a, b));
}

static value_t default_object_identity_compare(value_t a, value_t b,
    cycle_detector_t *detector) {
  return to_internal_boolean(is_same_value(a, b));
}

static value_t object_identity_compare(value_t a, value_t b,
    cycle_detector_t *detector) {
  CHECK_DOMAIN(vdObject, a);
  CHECK_DOMAIN(vdObject, b);
  // Fast case when a and b are the same object.
  if (is_same_value(a, b))
    return internal_true_value();
  object_family_t a_family = get_object_family(a);
  object_family_t b_family = get_object_family(b);
  if (a_family != b_family)
    return internal_false_value();
  family_behavior_t *behavior = get_object_family_behavior(a);
  return (behavior->identity_compare)(a, b, detector);
}

bool value_identity_compare(value_t a, value_t b) {
  cycle_detector_t detector;
  cycle_detector_init_bottom(&detector);
  value_t protected = value_identity_compare_cycle_protect(a, b, &detector);
  return is_internal_true_value(protected);
}

value_t value_identity_compare_cycle_protect(value_t a, value_t b,
    cycle_detector_t *detector) {
  // First check that they even belong to the same domain. Values can be equal
  // across domains.
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return internal_false_value();
  // Then dispatch to the domain equals functions.
  switch (a_domain) {
    case vdInteger:
      return integer_identity_compare(a, b);
    case vdObject:
      return object_identity_compare(a, b, detector);
    default:
      return internal_false_value();
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

static void integer_print_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdInteger, value);
  string_buffer_printf(buf, "%lli", get_integer_value(value));
}

static void signal_print_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdSignal, value);
  signal_cause_t cause = get_signal_cause(value);
  const char *cause_name = get_signal_cause_name(cause);
  string_buffer_printf(buf, "%%<signal: %s(", cause_name);
  uint32_t details = get_signal_details(value);
  switch (cause) {
    case scInvalidSyntax: {
      string_buffer_printf(buf, "%s", get_invalid_syntax_cause_name(details));
      break;
    }
    case scUnsupportedBehavior: {
      unsupported_behavior_details_codec_t codec;
      codec.encoded = details;
      unsupported_behavior_cause_t cause = codec.decoded.cause;
      string_buffer_printf(buf, "%s", get_unsupported_behavior_cause_name(cause));
      value_domain_t domain = codec.decoded.domain;
      string_buffer_printf(buf, " of %s", get_value_domain_name(domain));
      object_family_t family = codec.decoded.family;
      if (family != __ofUnknown__)
        string_buffer_printf(buf, "/%s", get_object_family_name(family));
      break;
    }
    case scLookupError: {
      string_buffer_printf(buf, "%s", get_lookup_error_cause_name(details));
      break;
    }
    default: {
      string_buffer_printf(buf, "dt@%i", details);
      break;
    }
  }
  string_buffer_printf(buf, ")>");
}

static void object_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  family_behavior_t *behavior = get_object_family_behavior(value);
  (behavior->print_on)(value, buf, flags, depth);
}

void value_print_on_cycle_detect(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  switch (get_value_domain(value)) {
    case vdInteger:
      integer_print_on(value, buf);
      break;
    case vdObject:
      object_print_on(value, buf, flags, depth);
      break;
    case vdSignal:
      signal_print_on(value, buf);
      break;
    default:
      UNREACHABLE("value print on");
      break;
  }
}

void value_print_on(value_t value, string_buffer_t *buf) {
  value_print_on_cycle_detect(value, buf, pfNone, 2);
}

void value_print_on_unquoted(value_t value, string_buffer_t *buf) {
  value_print_on_cycle_detect(value, buf, pfUnquote, 2);
}

void value_print_inner_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  if (depth == 0) {
    string_buffer_printf(buf, "-");
  } else {
    value_print_on_cycle_detect(value, buf, flags, depth);
  }
}


// --- N e w   i n s t a n c e ---

static value_t new_instance_of_factory(runtime_t *runtime, value_t type) {
  value_t constr_wrapper = get_factory_constructor(type);
  void *constr_ptr = get_void_p_value(constr_wrapper);
  factory_constructor_t *constr = (factory_constructor_t*) (intptr_t) constr_ptr;
  return constr(runtime);
}

static value_t new_instance_of_protocol(runtime_t *runtime, value_t protocol) {
  TRY_DEF(species, new_heap_instance_species(runtime, protocol));
  TRY_DEF(result, new_heap_instance(runtime, species));
  return result;
}

static value_t new_object_with_object_type(runtime_t *runtime, value_t type) {
  object_family_t family = get_object_family(type);
  switch (family) {
    case ofNull:
      // For now we use null to indicate an instance. Later this should be
      // replaced by something else, something species-like possibly.
      return new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
    case ofProtocol:
      return new_instance_of_protocol(runtime, type);
    case ofFactory:
      return new_instance_of_factory(runtime, type);
    default: {
      value_to_string_t data;
      WARN("Invalid type %s", value_to_string(&data, type));
      dispose_value_to_string(&data);
      return new_unsupported_behavior_signal(vdObject, family, ubNewObjectWithType);
    }
  }
}

value_t new_object_with_type(runtime_t *runtime, value_t type) {
  value_domain_t domain = get_value_domain(type);
  switch (domain) {
    case vdObject:
      return new_object_with_object_type(runtime, type);
    default:
      return new_unsupported_behavior_signal(domain, __ofUnknown__,
          ubNewObjectWithType);
  }
}


// --- P a y l o a d ---

value_t set_object_contents(runtime_t *runtime, value_t object, value_t payload) {
  family_behavior_t *behavior = get_object_family_behavior(object);
  TRY((behavior->set_contents)(object, runtime, payload));
  TRY(object_validate(object));
  return success();
}

// A function compatible with set_contents that always returns unsupported.
static value_t set_contents_unsupported(value_t value, runtime_t *runtime,
    value_t contents) {
  return new_unsupported_behavior_signal(vdObject, get_object_family(value),
      ubSetContents);
}


// --- P r o t o c o l ---

static value_t get_object_protocol(value_t self, runtime_t *runtime) {
  family_behavior_t *behavior = get_object_family_behavior(self);
  return (behavior->get_protocol)(self, runtime);
}

value_t get_protocol(value_t self, runtime_t *runtime) {
  value_domain_t domain = get_value_domain(self);
  switch (domain) {
    case vdInteger:
      return ROOT(runtime, integer_protocol);
    case vdObject:
      return get_object_protocol(self, runtime);
    default:
      return new_unsupported_behavior_signal(domain, __ofUnknown__,
          ubGetProtocol);
  }
}

static value_t get_internal_object_protocol(value_t self, runtime_t *runtime) {
  return new_signal(scInternalFamily);
}


// --- F r a m e w o r k ---

// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_OBJECT_FAMILY_BEHAVIOR(Family, family, CM, ID, CT, SR, NL, FU, EM, MD, OW) \
family_behavior_t k##Family##Behavior = {                                      \
  of##Family,                                                                  \
  &family##_validate,                                                          \
  ID(                                                                          \
    &family##_transient_identity_hash,                                         \
    default_object_transient_identity_hash),                                   \
  ID(                                                                          \
    &family##_identity_compare,                                                \
    &default_object_identity_compare),                                         \
  CM(                                                                          \
    &family##_ordering_compare,                                                \
    NULL),                                                                     \
  &family##_print_on,                                                          \
  &get_##family##_layout,                                                      \
  CT(                                                                          \
    &set_##family##_contents,                                                  \
    &set_contents_unsupported),                                                \
  SR(                                                                          \
    &get_##family##_protocol,                                                  \
    &get_internal_object_protocol),                                            \
  FU(                                                                          \
    &fixup_##family##_post_migrate,                                            \
    NULL),                                                                     \
  MD(                                                                          \
    get_modal_object_mode,                                                     \
    get_##family##_mode),                                                      \
  MD(                                                                          \
    set_modal_object_mode_unchecked,                                           \
    set_##family##_mode_unchecked),                                            \
  OW(                                                                          \
    ensure_##family##_owned_values_frozen,                                     \
    NULL)                                                                      \
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
