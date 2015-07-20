//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "ctrino.h"
#include "derived-inl.h"
#include "freeze.h"
#include "io.h"
#include "runtime.h"
#include "sync.h"
#include "syntax.h"
#include "try-inl.h"
#include "utils/log.h"
#include "value-inl.h"


// --- V a l i d a t e ---

// Is the given family in a modal division?
static bool in_modal_division(heap_object_family_t family) {
  switch (family) {
#define __GEN_CASE__(Family, family, MD, SR, MINOR, N)                         \
    MD(                                                                        \
      case of##Family: return true;,                                           \
      )
    ENUM_HEAP_OBJECT_FAMILIES(__GEN_CASE__)
#undef __GEN_CASE__
    default:
      return false;
  }
}

// Validate that a given deep frozen object only points to other deep frozen
// objects.
static void deep_frozen_heap_object_validate(value_t value) {
  value_field_iter_t iter;
  value_field_iter_init(&iter, value);
  value_t *field = NULL;
  while (value_field_iter_next(&iter, &field)) {
    if (!peek_deep_frozen(*field)) {
      ERROR("Holder: %v, field: %v", value, *field);
      UNREACHABLE("deep frozen reference of not deep frozen");
    }
  }
}

value_t heap_object_validate(value_t value) {
  family_behavior_t *behavior = get_heap_object_family_behavior(value);
  CHECK_FALSE("Modal value with non-modal species",
      in_modal_division(behavior->family) && get_heap_object_division(value) != sdModal);
  if (peek_deep_frozen(value) && behavior->deep_frozen_field_validation)
    deep_frozen_heap_object_validate(value);
  return (behavior->validate)(value);
}

value_t derived_object_validate(value_t value) {
  value_t anchor = get_derived_object_anchor(value);
  CHECK_PHYLUM(tpDerivedObjectAnchor, anchor);
  genus_descriptor_t *desc = get_genus_descriptor(get_derived_object_anchor_genus(anchor));
  return (desc->validate)(value);
}

value_t value_validate(value_t value) {
  switch (get_value_domain(value)) {
    case vdHeapObject:
      return heap_object_validate(value);
    case vdDerivedObject:
      return derived_object_validate(value);
    default:
      return success();
  }
}


// --- L a y o u t ---

void heap_object_layout_init(heap_object_layout_t *layout) {
  layout->size = 0;
  layout->value_offset = 0;
}

void heap_object_layout_set(heap_object_layout_t *layout, size_t size, size_t value_offset) {
  layout->size = size;
  layout->value_offset = value_offset;
}

void get_heap_object_layout(value_t self, heap_object_layout_t *layout_out) {
  // This has to work during gc so some of the normal behavior checks are
  // disabled.
  CHECK_DOMAIN(vdHeapObject, self);
  // We only get the layout of objects that have already been moved so this
  // gives a proper species.
  value_t species = get_heap_object_species(self);
  // The species itself may have been moved but in any its memory will still be
  // intact enough that we can get the behavior out.
  family_behavior_t *behavior = get_species_family_behavior(species);
  (behavior->get_heap_object_layout)(self, layout_out);
}

// Declares the heap size functions for a fixed-size object that don't have any
// non-value fields.
#define __TRIVIAL_LAYOUT_FUNCTION__(Family, family)                            \
static void get_##family##_layout(value_t value, heap_object_layout_t *layout_out) {\
  heap_object_layout_set(layout_out, k##Family##Size, kHeapObjectHeaderSize);  \
}

// Generate all the trivial layout functions since we know what they'll look
// like.
#define __DEFINE_TRIVIAL_LAYOUT_FUNCTION__(Family, family, MD, SR, MINOR, N)   \
mfNl MINOR (                                                                   \
  ,                                                                            \
  __TRIVIAL_LAYOUT_FUNCTION__(Family, family))
  ENUM_HEAP_OBJECT_FAMILIES(__DEFINE_TRIVIAL_LAYOUT_FUNCTION__)
#undef __DEFINE_TRIVIAL_LAYOUT_FUNCTION__
#undef __TRIVIAL_LAYOUT_FUNCTION__


// --- M o d e ---

value_mode_t get_value_mode(value_t self) {
  if (is_heap_object(self)) {
    family_behavior_t *behavior = get_heap_object_family_behavior(self);
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
    return new_invalid_mode_change_condition(current_mode);
  }
}

value_t set_value_mode_unchecked(runtime_t *runtime, value_t self, value_mode_t mode) {
  if (is_heap_object(self)) {
    family_behavior_t *behavior = get_heap_object_family_behavior(self);
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

static value_t custom_tagged_transient_identity_hash(value_t self,
    hash_stream_t *stream) {
  CHECK_DOMAIN(vdCustomTagged, self);
  hash_stream_write_tags(stream, vdCustomTagged, __ofUnknown__);
  hash_stream_write_int64(stream, get_custom_tagged_phylum(self));
  hash_stream_write_int64(stream, get_custom_tagged_payload(self));
  return success();
}

static value_t default_heap_heap_object_transient_identity_hash(value_t value,
    hash_stream_t *stream, cycle_detector_t *detector) {
  // heap_object_transient_identity_hash has already written the tags.
  hash_stream_write_int64(stream, value.encoded);
  return success();
}

static value_t heap_object_transient_identity_hash(value_t self,
    hash_stream_t *stream, cycle_detector_t *detector) {
  // The toplevel delegator functions are responsible for writing the tags,
  // that way the individual hashing functions don't all have to do that.
  family_behavior_t *behavior = get_heap_object_family_behavior(self);
  hash_stream_write_tags(stream, vdHeapObject, behavior->family);
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
  switch (get_value_domain(value)) {
    case vdInteger:
      return integer_transient_identity_hash(value, stream);
    case vdHeapObject:
      return heap_object_transient_identity_hash(value, stream, detector);
    case vdCustomTagged:
      return custom_tagged_transient_identity_hash(value, stream);
    default:
      return new_unsupported_behavior_condition(get_value_type_info(value),
          ubTransientIdentityHash);
  }
}


// --- I d e n t i t y ---

static value_t default_heap_heap_object_identity_compare(value_t a, value_t b,
    cycle_detector_t *detector) {
  return new_boolean(is_same_value(a, b));
}

static value_t heap_object_identity_compare(value_t a, value_t b,
    cycle_detector_t *detector) {
  CHECK_DOMAIN(vdHeapObject, a);
  CHECK_DOMAIN(vdHeapObject, b);
  // Fast case when a and b are the same object.
  if (is_same_value(a, b))
    return yes();
  heap_object_family_t a_family = get_heap_object_family(a);
  heap_object_family_t b_family = get_heap_object_family(b);
  if (a_family != b_family)
    return no();
  family_behavior_t *behavior = get_heap_object_family_behavior(a);
  return (behavior->identity_compare)(a, b, detector);
}

bool value_identity_compare(value_t a, value_t b) {
  if (is_same_value(a, b))
    return true;
  cycle_detector_t detector;
  cycle_detector_init_bottom(&detector);
  value_t ptected = value_identity_compare_cycle_protect(a, b, &detector);
  return in_condition_cause(ccCircular, ptected) ? false : get_boolean_value(ptected);
}

value_t value_identity_compare_cycle_protect(value_t a, value_t b,
    cycle_detector_t *detector) {
  // First check that they even belong to the same domain. Values can be equal
  // across domains.
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return no();
  // Then dispatch to the domain equals functions.
  switch (a_domain) {
    case vdInteger:
    case vdCustomTagged:
      return new_boolean(is_same_value(a, b));
    case vdHeapObject:
      return heap_object_identity_compare(a, b, detector);
    default:
      return no();
  }
}


// --- C o m p a r i n g ---

static value_t integer_ordering_compare(value_t a, value_t b) {
  return compare_signed_integers(get_integer_value(a), get_integer_value(b));
}

static value_t object_ordering_compare(value_t a, value_t b) {
  CHECK_DOMAIN(vdHeapObject, a);
  CHECK_DOMAIN(vdHeapObject, b);
  heap_object_family_t a_family = get_heap_object_family(a);
  heap_object_family_t b_family = get_heap_object_family(b);
  if (a_family != b_family)
    // This may cause us to return a valid result even when a and b are not
    // comparable.
    return compare_signed_integers(a_family, b_family);
  family_behavior_t *behavior = get_heap_object_family_behavior(a);
  value_t (*ordering_compare)(value_t a, value_t b) = behavior->ordering_compare;
  if (ordering_compare == NULL) {
    return unordered();
  } else {
    return ordering_compare(a, b);
  }
}

static value_t custom_tagged_ordering_compare(value_t a, value_t b) {
  CHECK_DOMAIN(vdCustomTagged, a);
  CHECK_DOMAIN(vdCustomTagged, b);
  custom_tagged_phylum_t a_phylum = get_custom_tagged_phylum(a);
  custom_tagged_phylum_t b_phylum = get_custom_tagged_phylum(b);
  if (a_phylum != b_phylum)
    return compare_signed_integers(a_phylum, b_phylum);
  phylum_behavior_t *behavior = get_custom_tagged_phylum_behavior(a_phylum);
  value_t (*ordering_compare)(value_t a, value_t b) = behavior->ordering_compare;
  if (ordering_compare == NULL) {
    return unordered();
  } else {
    return ordering_compare(a, b);
  }
}

value_t value_ordering_compare(value_t a, value_t b) {
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain) {
    int a_ordinal = get_value_domain_ordinal(a_domain);
    int b_ordinal = get_value_domain_ordinal(b_domain);
    return compare_signed_integers(a_ordinal, b_ordinal);
  } else {
    switch (a_domain) {
      case vdInteger:
        return integer_ordering_compare(a, b);
      case vdHeapObject:
        return object_ordering_compare(a, b);
      case vdCustomTagged:
        return custom_tagged_ordering_compare(a, b);
      default:
        return unordered();
    }
  }
}


// --- P r i n t i n g ---

void print_on_context_init(print_on_context_t *context, string_buffer_t *buf,
  uint32_t flags, size_t depth) {
  context->buf = buf;
  context->flags = flags;
  context->depth = depth;
}

static void integer_print_on(value_t value, print_on_context_t *context) {
  CHECK_DOMAIN(vdInteger, value);
  const char *fmt = (context->flags & pfHex) ? "%llx" : "%lli";
  string_buffer_printf(context->buf, fmt, get_integer_value(value));
}

static void condition_print_on(value_t value, string_buffer_t *buf) {
  CHECK_DOMAIN(vdCondition, value);
  condition_cause_t cause = get_condition_cause(value);
  const char *cause_name = get_condition_cause_name(cause);
  string_buffer_printf(buf, "%%<condition: %s(", cause_name);
  uint32_t details = get_condition_details(value);
  switch (cause) {
    case ccInvalidSyntax: {
      invalid_syntax_cause_t cause = (invalid_syntax_cause_t) details;
      string_buffer_printf(buf, "%s", get_invalid_syntax_cause_name(cause));
      break;
    }
    case ccUnsupportedBehavior: {
      unsupported_behavior_details_codec_t codec;
      codec.encoded = details;
      unsupported_behavior_cause_t cause = (unsupported_behavior_cause_t) codec.decoded.cause;
      string_buffer_printf(buf, "%s", get_unsupported_behavior_cause_name(cause));
      value_type_info_t type = value_type_info_decode(codec.decoded.type_info);
      string_buffer_printf(buf, " of %s", value_type_info_name(type));
      break;
    }
    case ccLookupError: {
      lookup_error_cause_t cause = (lookup_error_cause_t) details;
      string_buffer_printf(buf, "%s", get_lookup_error_cause_name(cause));
      break;
    }
    case ccSystemError: {
      system_error_cause_t cause = (system_error_cause_t) details;
      string_buffer_printf(buf, "%s", get_system_error_cause_name(cause));
      break;
    }
    case ccInvalidInput: {
      if (details != 0) {
        // If no hint is given (or the hint it the empty string) the details
        // field will be 0.
        invalid_input_details_codec_t codec;
        codec.encoded = details;
        char hint[7];
        string_hint_to_c_str(codec.decoded, hint);
        string_buffer_printf(buf, "%s", hint);
      }
      break;
    }
    case ccNotSerializable: {
      value_type_info_t type = value_type_info_decode((uint16_t) details);
      string_buffer_printf(buf, "%s", value_type_info_name(type));
      break;
    }
    case ccUncaughtSignal: {
      string_buffer_printf(buf, is_uncaught_signal_escape(value) ? "escape" : "non-escape");
      break;
    }
    default: {
      string_buffer_printf(buf, "dt@%i", details);
      break;
    }
  }
  string_buffer_printf(buf, ")>");
}

static void heap_object_print_on(value_t value, print_on_context_t *context) {
  CHECK_DOMAIN(vdHeapObject, value);
  family_behavior_t *behavior = get_heap_object_family_behavior(value);
  (behavior->print_on)(value, context);
}

static void custom_tagged_print_on(value_t value, print_on_context_t *context) {
  CHECK_DOMAIN(vdCustomTagged, value);
  phylum_behavior_t *behavior = get_custom_tagged_behavior(value);
  (behavior->print_on)(value, context);
}

static void derived_object_print_on(value_t value, print_on_context_t *context) {
  CHECK_DOMAIN(vdDerivedObject, value);
  derived_object_genus_t genus = get_derived_object_genus(value);
  genus_descriptor_t *descriptor = get_genus_descriptor(genus);
  (descriptor->print_on)(value, context);
}

void value_print_on_cycle_detect(value_t value, print_on_context_t *context) {
  switch (get_value_domain(value)) {
    case vdInteger:
      integer_print_on(value, context);
      break;
    case vdHeapObject:
      heap_object_print_on(value, context);
      break;
    case vdDerivedObject:
      derived_object_print_on(value, context);
      break;
    case vdCondition:
      condition_print_on(value, context->buf);
      break;
    case vdCustomTagged:
      custom_tagged_print_on(value, context);
      break;
    default:
      UNREACHABLE("value print on");
      break;
  }
}

void value_print_on(value_t value, print_on_context_t *context) {
  value_print_inner_on(value, context, 0);
}

void value_print_default_on(value_t value, string_buffer_t *buf) {
  print_on_context_t context;
  print_on_context_init(&context, buf, pfNone, kDefaultPrintDepth);
  value_print_on(value, &context);
}

void value_print_on_unquoted(value_t value, string_buffer_t *buf) {
  print_on_context_t context;
  print_on_context_init(&context, buf, pfUnquote, kDefaultPrintDepth);
  value_print_on_cycle_detect(value, &context);
}

void value_print_inner_on(value_t value, print_on_context_t *context,
    int32_t delta_depth) {
  size_t new_depth = context->depth + delta_depth;
  if (new_depth == 0) {
    string_buffer_printf(context->buf, kBottomValuePlaceholder);
  } else {
    print_on_context_t new_context = *context;
    new_context.depth = new_depth;
    value_print_on_cycle_detect(value, &new_context);
  }
}


// --- N e w   i n s t a n c e ---

static value_t new_instance_of_seed(runtime_t *runtime, value_t type) {
  return new_heap_seed(runtime, type, nothing());
}

static value_t new_instance_of_type(runtime_t *runtime, value_t type) {
  TRY_DEF(species, new_heap_instance_species(runtime, type, nothing(), vmFluid));
  TRY_DEF(result, new_heap_instance(runtime, species));
  return result;
}

static value_t new_instance_of_string(runtime_t *runtime, value_t type) {
  value_t factories = ROOT(runtime, plankton_factories);
  value_t factory = get_id_hash_map_at(factories, type);
  if (in_family(ofFactory, factory)) {
    value_t new_instance_wrapper = get_factory_new_instance(factory);
    void *new_instance_ptr = get_void_p_value(new_instance_wrapper);
    factory_new_instance_t *new_instance = (factory_new_instance_t*) (intptr_t) new_instance_ptr;
    return new_instance(runtime);
  } else {
    return new_heap_seed(runtime, type, nothing());
  }
}

static value_t new_heap_object_with_object_type(runtime_t *runtime, value_t type) {
  heap_object_family_t family = get_heap_object_family(type);
  switch (family) {
    case ofType:
      return new_instance_of_type(runtime, type);
    case ofUtf8:
      return new_instance_of_string(runtime, type);
    default: {
      return new_instance_of_seed(runtime, type);
    }
  }
}

static value_t new_heap_object_with_custom_tagged_type(runtime_t *runtime, value_t type) {
  custom_tagged_phylum_t phylum = get_custom_tagged_phylum(type);
  switch (phylum) {
    case tpNull:
      // For now we use null to indicate an instance. Later this should be
      // replaced by something else, something species-like possibly.
      return new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
    default:
      return new_instance_of_seed(runtime, type);
  }
}

value_t new_heap_object_with_type(runtime_t *runtime, value_t header) {
  switch (get_value_domain(header)) {
    case vdHeapObject:
      return new_heap_object_with_object_type(runtime, header);
    case vdCustomTagged:
      return new_heap_object_with_custom_tagged_type(runtime, header);
    default:
      return new_unsupported_behavior_condition(get_value_type_info(header),
          ubNewObjectWithType);
  }
}


// --- P a y l o a d ---

static value_t set_heap_object_contents_with_utf8_type(runtime_t *runtime,
    value_t object, value_t header, value_t payload) {
  value_t factories = ROOT(runtime, plankton_factories);
  value_t factory = get_id_hash_map_at(factories, header);
  if (in_family(ofFactory, factory)) {
    value_t set_contents_wrapper = get_factory_set_contents(factory);
    void *set_contents_ptr = get_void_p_value(set_contents_wrapper);
    factory_set_contents_t *set_contents = (factory_set_contents_t*) (intptr_t) set_contents_ptr;
    return set_contents(object, runtime, payload);
  } else {
    set_seed_payload(object, payload);
    return success();
  }
}

static value_t set_heap_object_contents_with_object_type(runtime_t *runtime, value_t object,
    value_t header, value_t payload) {
  heap_object_family_t family = get_heap_object_family(header);
  switch (family) {
    case ofUtf8:
      return set_heap_object_contents_with_utf8_type(runtime, object, header,
          payload);
    default: {
      set_seed_payload(object, payload);
      return success();
    }
  }
}

static value_t set_heap_object_contents_with_custom_tagged_type(runtime_t *runtime,
    value_t object, value_t header, value_t payload) {
  custom_tagged_phylum_t phylum = get_custom_tagged_phylum(header);
  switch (phylum) {
    case tpNull:
      // For now we use null to indicate an instance. Later this should be
      // replaced by something else, something species-like possibly.
      return plankton_set_instance_contents(object, runtime, payload);
    default:
      set_seed_payload(object, payload);
      return success();
  }
}

value_t set_heap_object_contents(runtime_t *runtime, value_t object,
    value_t header, value_t payload) {
  switch (get_value_domain(header)) {
    case vdHeapObject:
      return set_heap_object_contents_with_object_type(runtime, object, header, payload);
      break;
    case vdCustomTagged:
      return set_heap_object_contents_with_custom_tagged_type(runtime, object, header, payload);
      break;
    default:
      return new_unsupported_behavior_condition(get_value_type_info(header),
          ubNewObjectWithType);
  }
}


// --- P r o t o c o l ---

static value_t get_heap_object_primary_type(value_t self, runtime_t *runtime) {
  family_behavior_t *behavior = get_heap_object_family_behavior(self);
  return (behavior->get_primary_type)(self, runtime);
}

static value_t get_custom_tagged_primary_type(value_t self, runtime_t *runtime) {
  phylum_behavior_t *behavior = get_custom_tagged_behavior(self);
  return (behavior->get_primary_type)(self, runtime);
}

value_t get_primary_type(value_t self, runtime_t *runtime) {
  switch (get_value_domain(self)) {
    case vdInteger:
      return ROOT(runtime, integer_type);
    case vdHeapObject:
      return get_heap_object_primary_type(self, runtime);
    case vdCustomTagged:
      return get_custom_tagged_primary_type(self, runtime);
    default:
      return new_unsupported_behavior_condition(get_value_type_info(self),
          ubGetPrimaryType);
  }
}

static value_t get_internal_object_type(value_t self, runtime_t *runtime) {
  ERROR("Internal family: %v", self);
  return new_condition(ccInternalFamily);
}

value_t finalize_heap_object(value_t garbage) {
  CHECK_DOMAIN(vdHeapObject, garbage);
  value_t header = get_heap_object_header(garbage);
  CHECK_FALSE("finalizing live object", in_domain(vdMovedObject, header));
  family_behavior_t *behavior = get_species_family_behavior(header);
  garbage_value_t wrapper = {garbage};
  CHECK_FALSE("finalizing object without finalizer", behavior->finalize == NULL);
  return (behavior->finalize(wrapper));
}


// ## Scoping

void on_derived_object_exit(value_t self) {
  CHECK_DOMAIN(vdDerivedObject, self);
  genus_descriptor_t *descriptor = get_derived_object_descriptor(self);
  CHECK_TRUE("derived object not scoped", descriptor->on_scope_exit != NULL);
  (descriptor->on_scope_exit)(self);
}


// --- F r a m e w o r k ---

// Define all the family behaviors in one go. Because of this, as soon as you
// add a new object type you'll get errors for all the behaviors you need to
// implement.
#define DEFINE_OBJECT_FAMILY_BEHAVIOR(Family, family, MD, SR, MINOR, N)        \
family_behavior_t k##Family##Behavior = {                                      \
  of##Family,                                                                  \
  mfFz MINOR (false, true),                                                    \
  &family##_validate,                                                          \
  mfId MINOR (                                                                 \
    &family##_transient_identity_hash,                                         \
    default_heap_heap_object_transient_identity_hash),                         \
  mfId MINOR (                                                                 \
    &family##_identity_compare,                                                \
    &default_heap_heap_object_identity_compare),                               \
  mfCm MINOR (                                                                 \
    &family##_ordering_compare,                                                \
    NULL),                                                                     \
  &family##_print_on,                                                          \
  &get_##family##_layout,                                                      \
  SR(                                                                          \
    &get_##family##_primary_type,                                              \
    &get_internal_object_type),                                                \
  mfFu MINOR (                                                                 \
    &fixup_##family##_post_migrate,                                            \
    NULL),                                                                     \
  MD(                                                                          \
    get_modal_heap_object_mode,                                                \
    get_##family##_mode),                                                      \
  MD(                                                                          \
    set_modal_heap_object_mode_unchecked,                                      \
    set_##family##_mode_unchecked),                                            \
  mfOw MINOR (                                                                 \
    ensure_##family##_owned_values_frozen,                                     \
    NULL),                                                                     \
  mfFl MINOR (                                                                 \
    finalize_##family,                                                         \
    NULL),                                                                     \
  mfSo MINOR (                                                                 \
    serialize_##family,                                                        \
    NULL)                                                                      \
};
ENUM_HEAP_OBJECT_FAMILIES(DEFINE_OBJECT_FAMILY_BEHAVIOR)
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


// --- P h y l u m s ---

// Define all the division behaviors. Similarly to families, when you add a new
// division you have to add the methods or this will break.
#define DEFINE_TAGGED_PHYLUM_BEHAVIOR(Phylum, phylum, SR, MINOR, N)            \
phylum_behavior_t k##Phylum##PhylumBehavior = {                                \
  tp##Phylum,                                                                  \
  &phylum##_print_on,                                                          \
  mpCm MINOR (                                                                 \
    &phylum##_ordering_compare,                                                \
    NULL),                                                                     \
  SR(                                                                          \
    &get_##phylum##_primary_type,                                              \
    &get_internal_object_type),                                                \
};
ENUM_CUSTOM_TAGGED_PHYLUMS(DEFINE_TAGGED_PHYLUM_BEHAVIOR)
#undef DEFINE_TAGGED_PHYLUM_BEHAVIOR

phylum_behavior_t *kCustomTaggedPhylumBehaviors[kCustomTaggedPhylumCount] = {NULL};

phylum_behavior_t *get_custom_tagged_phylum_behavior(custom_tagged_phylum_t phylum) {
  if (kCustomTaggedPhylumBehaviors[0] == NULL) {
#define __SET_PHYLUM_BEHAVIOR__(Phylum, phylum, SR, MINOR, N)                  \
    kCustomTaggedPhylumBehaviors[tp##Phylum] = &k##Phylum##PhylumBehavior;
  ENUM_CUSTOM_TAGGED_PHYLUMS(__SET_PHYLUM_BEHAVIOR__)
#undef __SET_PHYLUM_BEHAVIOR__
  }
  return kCustomTaggedPhylumBehaviors[phylum];
}

phylum_behavior_t *get_custom_tagged_behavior(value_t value) {
  CHECK_DOMAIN(vdCustomTagged, value);
  custom_tagged_phylum_t phylum = get_custom_tagged_phylum(value);
  return get_custom_tagged_phylum_behavior(phylum);
}
