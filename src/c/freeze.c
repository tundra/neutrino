//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "freeze.h"
#include "heap.h"
#include "utils/log.h"
#include "value-inl.h"

bool is_mutable(value_t value) {
  return get_value_mode(value) <= vmMutable;
}

bool is_frozen(value_t value) {
  return get_value_mode(value) >= vmFrozen;
}

bool peek_deep_frozen(value_t value) {
  return get_value_mode(value) == vmDeepFrozen;
}

value_t ensure_shallow_frozen(runtime_t *runtime, value_t value) {
  return set_value_mode(runtime, value, vmFrozen);
}

value_t ensure_frozen(runtime_t *runtime, value_t value) {
  if (get_value_mode(value) == vmDeepFrozen)
    return success();
  if (is_heap_object(value)) {
    TRY(ensure_shallow_frozen(runtime, value));
    family_behavior_t *behavior = get_heap_object_family_behavior(value);
    value_t (*freeze_owned)(runtime_t*, value_t) = behavior->ensure_owned_values_frozen;
    if (freeze_owned == NULL) {
      return success();
    } else {
      return freeze_owned(runtime, value);
    }
  } else {
    UNREACHABLE("non-object not deep frozen");
    return new_condition(ccNotDeepFrozen);
  }
}

// Assume tentatively that the given value is deep frozen and then see if that
// makes the whole graph deep frozen. If not we'll restore the object, otherwise
// we can leave it deep frozen.
value_t transitively_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  CHECK_DOMAIN(vdHeapObject, value);
  CHECK_EQ("tentatively freezing non-frozen", vmFrozen, get_value_mode(value));
  // Deep freeze the object.
  set_value_mode_unchecked(runtime, value, vmDeepFrozen);
  // Scan through the object's fields.
  value_field_iter_t iter;
  value_field_iter_init(&iter, value);
  value_t *field = NULL;
  while (value_field_iter_next(&iter, &field)) {
    // Try to deep freeze the field's value.
    value_t ensured = validate_deep_frozen(runtime, *field, offender_out);
    if (is_condition(ensured)) {
      // Deep freezing failed. Restore the object to its previous state and bail.
      set_value_mode_unchecked(runtime, value, vmFrozen);
      TOPIC_INFO(Freeze, "Failed to validate deep frozen: %v", value);
      return ensured;
    }
  }
  // Deep freezing succeeded for all references. Hence we can leave this object
  // deep frozen and return success.
  return success();
}

value_t validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  value_mode_t mode = get_value_mode(value);
  if (mode == vmDeepFrozen) {
    return success();
  } else if (mode == vmFrozen) {
    // The object is frozen. We'll try deep freezing it.
    return transitively_validate_deep_frozen(runtime, value, offender_out);
  } else {
    if (offender_out != NULL)
      *offender_out = value;
    return new_not_deep_frozen_condition();
  }
}

bool try_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out) {
  value_t ensured = validate_deep_frozen(runtime, value, offender_out);
  if (is_condition(ensured)) {
    CHECK_TRUE("deep freeze failed", in_condition_cause(ccNotDeepFrozen, ensured));
    // A NotFrozen condition indicates that there is something mutable somewhere
    // in the object graph.
    return false;
  } else {
    // Non-condition so freezing must have succeeded.
    return true;
  }
}


/// ## Freeze cheat

FIXED_GET_MODE_IMPL(freeze_cheat, vmDeepFrozen);
TRIVIAL_PRINT_ON_IMPL(FreezeCheat, freeze_cheat);

ACCESSORS_IMPL(FreezeCheat, freeze_cheat, acNoCheck, 0, Value, value);

value_t freeze_cheat_validate(value_t self) {
  VALIDATE_FAMILY(ofFreezeCheat, self);
  return success();
}
