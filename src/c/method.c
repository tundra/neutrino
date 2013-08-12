// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "method.h"
#include "value-inl.h"


// --- G u a r d ---

OBJECT_IDENTITY_IMPL(guard);
CANT_SET_CONTENTS(guard);
FIXED_SIZE_PURE_VALUE_IMPL(Guard, guard);
TRIVIAL_PRINT_ON_IMPL(Guard, guard);
NO_FAMILY_PROTOCOL_IMPL(guard);

ENUM_ACCESSORS_IMPL(Guard, guard, guard_type_t, Type, type);
UNCHECKED_ACCESSORS_IMPL(Guard, guard, Value, value);

value_t guard_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofGuard, value);
  return success();
}

bool is_score_match(score_t score) {
  return score != gsNoMatch;
}

score_t guard_match(value_t guard, value_t value) {
  CHECK_FAMILY(ofGuard, guard);
  switch (get_guard_type(guard)) {
    case gtId: {
      value_t guard_value = get_guard_value(guard);
      bool match = value_are_identical(guard_value, value);
      return match ? gsIdenticalMatch : gsNoMatch;
    }
    case gtAny:
      return gsAnyMatch;
    default:
      return gsNoMatch;
  }
}


// --- M e t h o d   s p a c e ---

OBJECT_IDENTITY_IMPL(method_space);
CANT_SET_CONTENTS(method_space);
FIXED_SIZE_PURE_VALUE_IMPL(MethodSpace, method_space);
TRIVIAL_PRINT_ON_IMPL(MethodSpace, method_space);
NO_FAMILY_PROTOCOL_IMPL(method_space);

CHECKED_ACCESSORS_IMPL(MethodSpace, method_space, IdHashMap, InheritanceMap,
    inheritance_map);

value_t method_space_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofMethodSpace, value);
  VALIDATE_VALUE_FAMILY(ofIdHashMap, get_method_space_inheritance_map(value));
  return success();
}

value_t add_method_space_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype) {
  CHECK_FAMILY(ofMethodSpace, self);
  CHECK_FAMILY(ofProtocol, subtype);
  CHECK_FAMILY(ofProtocol, supertype);
  value_t inheritance = get_method_space_inheritance_map(self);
  value_t parents = get_id_hash_map_at(inheritance, subtype);
  if (is_signal(scNotFound, parents)) {
    // Make the parents buffer small since most protocols don't have many direct
    // parents. If this fails nothing has happened.
    TRY_SET(parents, new_heap_array_buffer(runtime, 4));
    // If this fails we've wasted some space allocating the parents array but
    // otherwise nothing has happened.
    TRY(set_id_hash_map_at(runtime, inheritance, subtype, parents));
  }
  // If this fails we may have set the parents array of the subtype to an empty
  // array which is awkward but okay.
  return add_to_array_buffer(runtime, parents, supertype);
}

value_t get_protocol_parents(runtime_t *runtime, value_t space, value_t protocol) {
  value_t inheritance = get_method_space_inheritance_map(space);
  value_t parents = get_id_hash_map_at(inheritance, protocol);
  if (is_signal(scNotFound, parents)) {
    return runtime->roots.empty_array_buffer;
  } else {
    return parents;
  }
}
