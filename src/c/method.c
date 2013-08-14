// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "method.h"
#include "value-inl.h"


// --- S i g n a t u r e ---

TRIVIAL_PRINT_ON_IMPL(Signature, signature);

CHECKED_ACCESSORS_IMPL(Signature, signature, Array, Tags, tags);
CHECKED_ACCESSORS_IMPL(Signature, signature, Array, Descriptors, descriptors);
INTEGER_ACCESSORS_IMPL(Signature, signature, ParameterCount, parameter_count);
INTEGER_ACCESSORS_IMPL(Signature, signature, MandatoryCount, mandatory_count);
INTEGER_ACCESSORS_IMPL(Signature, signature, AllowExtra, allow_extra);

value_t signature_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofSignature, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_signature_tags(value));
  VALIDATE_VALUE_FAMILY(ofArray, get_signature_descriptors(value));
  return success();
}


// --- P a r a m e t e r ---

TRIVIAL_PRINT_ON_IMPL(Parameter, parameter);

CHECKED_ACCESSORS_IMPL(Parameter, parameter, Guard, Guard, guard);
INTEGER_ACCESSORS_IMPL(Parameter, parameter, IsOptional, is_optional);
INTEGER_ACCESSORS_IMPL(Parameter, parameter, Index, index);

value_t parameter_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofParameter, value);
  VALIDATE_VALUE_FAMILY(ofGuard, get_parameter_guard(value));
  return success();
}


// --- G u a r d ---

TRIVIAL_PRINT_ON_IMPL(Guard, guard);

ENUM_ACCESSORS_IMPL(Guard, guard, guard_type_t, Type, type);
UNCHECKED_ACCESSORS_IMPL(Guard, guard, Value, value);

value_t guard_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofGuard, value);
  return success();
}

bool is_score_match(score_t score) {
  return score != gsNoMatch;
}

int compare_scores(score_t a, score_t b) {
  return a < b ? -1 : (a == b ? 0 : 1);
}

// Given two scores returns the best of them.
static score_t best_score(score_t a, score_t b) {
  return (compare_scores(a, b) < 0) ? a : b;
}

static score_t find_best_match(runtime_t *runtime, value_t current,
    value_t target, score_t current_score, value_t space) {
  if (value_identity_compare(current, target)) {
    return current_score;
  } else {
    value_t parents = get_protocol_parents(runtime, space, current);
    size_t length = get_array_buffer_length(parents);
    score_t score = gsNoMatch;
    for (size_t i = 0; i < length; i++) {
      value_t parent = get_array_buffer_at(parents, i);
      score_t next_score = find_best_match(runtime, parent, target,
          current_score + 1, space);
      score = best_score(score, next_score);
    }
    return score;
  }
}

score_t guard_match(runtime_t *runtime, value_t guard, value_t value,
    value_t space) {
  CHECK_FAMILY(ofGuard, guard);
  switch (get_guard_type(guard)) {
    case gtId: {
      value_t guard_value = get_guard_value(guard);
      bool match = value_identity_compare(guard_value, value);
      return match ? gsIdenticalMatch : gsNoMatch;
    }
    case gtIs: {
      value_t primary = get_protocol(value, runtime);
      value_t target = get_guard_value(guard);
      return find_best_match(runtime, primary, target, gsPerfectIsMatch, space);
    }
    case gtAny:
      return gsAnyMatch;
    default:
      return gsNoMatch;
  }
}


// --- M e t h o d   s p a c e ---

TRIVIAL_PRINT_ON_IMPL(MethodSpace, method_space);

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


// --- I n v o c a t i o n   r e c o r d ---

TRIVIAL_PRINT_ON_IMPL(InvocationRecord, invocation_record);

CHECKED_ACCESSORS_IMPL(InvocationRecord, invocation_record, Array,
    ArgumentVector, argument_vector);

value_t invocation_record_validate(value_t self) {
  VALIDATE_VALUE_FAMILY(ofInvocationRecord, self);
  VALIDATE_VALUE_FAMILY(ofArray, get_invocation_record_argument_vector(self));
  return success();
}

void set_invocation_record_tag_at(value_t self, size_t index, value_t value) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  set_array_at(argument_vector, index << 1, value);
}

value_t get_invocation_record_tag_at(value_t self, size_t index) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_array_at(argument_vector, index << 1);
}

void set_invocation_record_offset_at(value_t self, size_t index, size_t value) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  set_array_at(argument_vector, (index << 1) + 1, new_integer(value));
}

size_t get_invocation_record_offset_at(value_t self, size_t index) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_integer_value(get_array_at(argument_vector, (index << 1) + 1));
}

size_t get_invocation_record_argument_count(value_t self) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_array_length(argument_vector) >> 1;
}
