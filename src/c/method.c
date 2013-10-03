// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "method.h"
#include "try-inl.h"
#include "value-inl.h"


// --- S i g n a t u r e ---

ACCESSORS_IMPL(Signature, signature, acInFamilyOpt, ofArray, Tags, tags);
INTEGER_ACCESSORS_IMPL(Signature, signature, ParameterCount, parameter_count);
INTEGER_ACCESSORS_IMPL(Signature, signature, MandatoryCount, mandatory_count);
INTEGER_ACCESSORS_IMPL(Signature, signature, AllowExtra, allow_extra);

value_t signature_validate(value_t self) {
  VALIDATE_FAMILY(ofSignature, self);
  VALIDATE_FAMILY_OPT(ofArray, get_signature_tags(self));
  return success();
}

// Given an array of parameters, returns a new array that contains cloned
// parameters with the same contents but which have been ordered appropriately.
static value_t clone_and_order_parameter_array(runtime_t *runtime,
    value_t raw_params) {
  size_t length = get_array_length(raw_params);
  TRY_DEF(result, new_heap_array(runtime, length));
  reusable_scratch_memory_t scratch;
  reusable_scratch_memory_init(&scratch);
  E_BEGIN_TRY_FINALLY();
    size_t scratchc = 2 * length;
    void *memory = reusable_scratch_memory_alloc(&scratch,
      scratchc * sizeof(value_t) + length * sizeof(size_t));
    value_t *scratch_values = memory;
    size_t *offsets = (void*) ((scratchc * sizeof(value_t)) + ((address_t) memory));
    calc_parameter_ordering(raw_params, scratch_values, scratchc, offsets, length);
    for (size_t i = 0; i < length; i++) {
      value_t raw_param = get_array_at(raw_params, i);
      E_TRY_DEF(param, new_heap_parameter(runtime, afFreeze,
          get_parameter_guard(raw_param),
          get_parameter_tags(raw_param),
          get_parameter_is_optional(raw_param),
          offsets[i]));
      set_array_at(result, i, param);
    }
    E_RETURN(result);
  E_FINALLY();
    reusable_scratch_memory_dispose(&scratch);
  E_END_TRY_FINALLY();
}

// Extract the attributes for a signature object from an array of parameter
// objects.
value_t set_signature_contents_from_parameters(runtime_t *runtime,
    value_t signature, value_t raw_params) {
  TRY_DEF(params, clone_and_order_parameter_array(runtime, raw_params));
  size_t param_count = get_array_length(params);
  size_t mandatory_count = 0;
  size_t tag_count = 0;
  // Count how many tags there are and how many of the params are mandatory.
  for (size_t i = 0; i < param_count; i++) {
    value_t param = get_array_at(params, i);
    if (!get_parameter_is_optional(param))
      mandatory_count++;
    tag_count += get_array_length(get_parameter_tags(param));
  }
  // Create an array with pairs of values, the first entry of which is the tag
  // and the second is the parameter.
  TRY_DEF(param_vector, new_heap_pair_array(runtime, tag_count));
  // Loop over all the tags, t being the tag index across the whole signature.
  size_t t = 0;
  for (size_t i = 0; i < param_count; i++) {
    value_t param = get_array_at(params, i);
    value_t tags = get_parameter_tags(param);
    size_t param_tag_count = get_array_length(tags);
    for (size_t j = 0; j < param_tag_count; j++, t++) {
      TRY_DEF(tag, get_array_at(tags, j));
      set_pair_array_first_at(param_vector, t, tag);
      set_pair_array_second_at(param_vector, t, param);
    }
  }
  co_sort_pair_array(param_vector);
  set_signature_parameter_count(signature, param_count);
  set_signature_mandatory_count(signature, mandatory_count);
  set_signature_tags(signature, param_vector);
  return success();
}

value_t set_signature_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(params, get_id_hash_map_at(contents, RSTR(runtime, parameters)));
  set_signature_contents_from_parameters(runtime, object, params);
  set_signature_allow_extra(object, false);
  return success();
}

value_t ensure_signature_owned_values_frozen(runtime_t *runtime, value_t self) {
  return ensure_frozen(runtime, get_signature_tags(self));
}

size_t get_signature_tag_count(value_t self) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_length(get_signature_tags(self));
}

value_t get_signature_tag_at(value_t self, size_t index) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_first_at(get_signature_tags(self), index);
}

value_t get_signature_parameter_at(value_t self, size_t index) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_second_at(get_signature_tags(self), index);
}

void signature_print_on(value_t self, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<signature: ");
  for (size_t i = 0; i < get_signature_parameter_count(self); i++) {
    if (i > 0)
      string_buffer_printf(buf, ", ");
    value_print_on(get_signature_tag_at(self, i), buf);
    string_buffer_printf(buf, ":");
    value_t param = get_signature_parameter_at(self, i);
    value_print_on(get_parameter_guard(param), buf);
  }
  string_buffer_printf(buf, ">");
}

void signature_print_atomic_on(value_t self, string_buffer_t *buf) {
  CHECK_FAMILY(ofSignature, self);
  string_buffer_printf(buf, "#<signature>");
}

bool match_result_is_match(match_result_t value) {
  return value >= mrMatch;
}

void match_info_init(match_info_t *info, score_t *scores, size_t *offsets,
    size_t capacity) {
  info->scores = scores;
  info->offsets = offsets;
  info->capacity = capacity;
}

match_result_t match_signature(runtime_t *runtime, value_t self, value_t record,
    frame_t *frame, value_t space, match_info_t *match_info) {
  size_t argument_count = get_invocation_record_argument_count(record);
  CHECK_REL("score array too short", argument_count, <=, match_info->capacity);
  // Vector of parameters seen. This is used to ensure that we only see each
  // parameter once.
  size_t param_count = get_signature_parameter_count(self);
  bit_vector_t params_seen;
  bit_vector_init(&params_seen, param_count, false);
  // Count how many mandatory parameters we see so we can check that we see all
  // of them.
  size_t mandatory_seen_count = 0;
  // The value to return if there is a match.
  match_result_t on_match = mrMatch;
  // Clear the score vector.
  for (size_t i = 0; i < argument_count; i++) {
    match_info->scores[i] = gsNoMatch;
    match_info->offsets[i] = kNoOffset;
  }
  // Scan through the arguments and look them up in the signature.
  value_t tags = get_signature_tags(self);
  for (size_t i = 0; i < argument_count; i++) {
    value_t tag = get_invocation_record_tag_at(record, i);
    // TODO: propagate any errors caused by this.
    value_t param = binary_search_pair_array(tags, tag);
    if (is_signal(scNotFound, param)) {
      // The tag wasn't found in this signature.
      if (get_signature_allow_extra(self)) {
        // It's fine, this signature allows extra arguments.
        on_match = mrExtraMatch;
        match_info->scores[i] = gsExtraMatch;
        continue;
      } else {
        // This signature doesn't allow extra arguments so we bail out.
        bit_vector_dispose(&params_seen);
        return mrUnexpectedArgument;
      }
    }
    CHECK_FALSE("binary search failed", get_value_domain(tags) == vdSignal);
    // The tag matched one in this signature.
    size_t index = get_parameter_index(param);
    if (bit_vector_get_at(&params_seen, index)) {
      // We've now seen two tags that match the same parameter. Bail out.
      bit_vector_dispose(&params_seen);
      return mrRedundantArgument;
    }
    value_t value = get_invocation_record_argument_at(record, frame, i);
    score_t score = guard_match(runtime, get_parameter_guard(param), value,
        space);
    if (!is_score_match(score)) {
      // The guard says the argument doesn't match. Bail out.
      bit_vector_dispose(&params_seen);
      return mrGuardRejected;
    }
    // We got a match! Record the result and move on to the next.
    bit_vector_set_at(&params_seen, index, true);
    match_info->scores[i] = score;
    match_info->offsets[index] = get_invocation_record_offset_at(record, i);
    if (!get_parameter_is_optional(param))
      mandatory_seen_count++;
  }
  bit_vector_dispose(&params_seen);
  if (mandatory_seen_count < get_signature_mandatory_count(self)) {
    // All arguments matched but there were mandatory arguments missing so it's
    // no good.
    return mrMissingArgument;
  } else {
    // Everything matched including all mandatories. We're golden.
    return on_match;
  }
}

join_status_t join_score_vectors(score_t *target, score_t *source, size_t length) {
  // The bit fiddling here works because of how the enum values are chosen.
  join_status_t result = jsEqual;
  for (size_t i = 0; i < length; i++) {
    int cmp = compare_scores(target[i], source[i]);
    if (cmp < 0) {
      // The target was strictly better than the source.
      result |= jsWorse;
    } else if (cmp > 0) {
      // The source was strictly better than the target; override.
      result |= jsBetter;
      target[i] = source[i];
    }
  }
  return result;
}


// --- P a r a m e t e r ---

ACCESSORS_IMPL(Parameter, parameter, acInFamilyOpt, ofGuard, Guard, guard);
ACCESSORS_IMPL(Parameter, parameter, acInFamilyOpt, ofArray, Tags, tags);
INTEGER_ACCESSORS_IMPL(Parameter, parameter, IsOptional, is_optional);
INTEGER_ACCESSORS_IMPL(Parameter, parameter, Index, index);

value_t set_parameter_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(tags, get_id_hash_map_at(contents, RSTR(runtime, tags)));
  TRY_DEF(guard, get_id_hash_map_at(contents, RSTR(runtime, guard)));
  set_parameter_tags(object, tags);
  set_parameter_guard(object, guard);
  set_parameter_index(object, 0);
  set_parameter_is_optional(object, false);
  return success();
}

value_t parameter_validate(value_t value) {
  VALIDATE_FAMILY(ofParameter, value);
  VALIDATE_FAMILY_OPT(ofGuard, get_parameter_guard(value));
  VALIDATE_FAMILY_OPT(ofArray, get_parameter_tags(value));
  return success();
}

void parameter_print_on(value_t self, string_buffer_t *buf) {
  parameter_print_atomic_on(self, buf);
}

void parameter_print_atomic_on(value_t self, string_buffer_t *buf) {
  CHECK_FAMILY(ofParameter, self);
  string_buffer_printf(buf, "#<parameter: gd@");
  // We know the guard is a guard, not a parameter, so this can't cause a cycle.
  guard_print_atomic_on(get_parameter_guard(self), buf);
  string_buffer_printf(buf, ", op@%i, ix@%i>",
      get_parameter_is_optional(self), get_parameter_index(self));
}


// --- G u a r d ---

ENUM_ACCESSORS_IMPL(Guard, guard, guard_type_t, Type, type);
ACCESSORS_IMPL(Guard, guard, acNoCheck, 0, Value, value);

value_t guard_validate(value_t value) {
  VALIDATE_FAMILY(ofGuard, value);
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
    case gtEq: {
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

void guard_print_on(value_t self, string_buffer_t *buf) {
  guard_print_atomic_on(self, buf);
}

void guard_print_atomic_on(value_t self, string_buffer_t *buf) {
  CHECK_FAMILY(ofGuard, self);
  switch (get_guard_type(self)) {
    case gtEq:
      string_buffer_printf(buf, "eq(");
      value_print_atomic_on(get_guard_value(self), buf);
      string_buffer_printf(buf, ")");
      break;
    case gtIs:
      string_buffer_printf(buf, "is(");
      value_print_atomic_on(get_guard_value(self), buf);
      string_buffer_printf(buf, ")");
      break;
    case gtAny:
      string_buffer_printf(buf, "any()");
      break;
  }
}

value_t set_guard_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(type_str, get_id_hash_map_at(contents, RSTR(runtime, type)));
  TRY_DEF(value, get_id_hash_map_at(contents, RSTR(runtime, value)));
  guard_type_t type;
  // Maybe passing an integer enum will be good enough? Or does that conflict
  // with being self-describing?
  EXPECT_FAMILY(scInvalidInput, ofString, type_str);
  char type_char = get_string_chars(type_str)[0];
  switch (type_char) {
    case '=': type = gtEq; break;
    case 'i': type = gtIs; break;
    case '*': type = gtAny; break;
    default: return new_signal(scInvalidInput);
  }
  set_guard_type(object, type);
  set_guard_value(object, value);
  return success();
}


// --- M e t h o d ---

TRIVIAL_PRINT_ON_IMPL(Method, method);

ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofSignature, Signature, signature);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofCodeBlock, Code, code);
ACCESSORS_IMPL(Method, method, acIsSyntaxOpt, 0, Syntax, syntax);

value_t method_validate(value_t self) {
  VALIDATE_FAMILY(ofMethod, self);
  VALIDATE_FAMILY_OPT(ofSignature, get_method_signature(self));
  VALIDATE_FAMILY_OPT(ofCodeBlock, get_method_code(self));
  return success();
}

value_t set_method_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(signature, get_id_hash_map_at(contents, RSTR(runtime, signature)));
  TRY_DEF(syntax, get_id_hash_map_at(contents, RSTR(runtime, syntax)));
  set_method_signature(object, signature);
  set_method_code(object, ROOT(runtime, nothing));
  set_method_syntax(object, syntax);
  return success();
}


// --- M e t h o d   s p a c e ---

TRIVIAL_PRINT_ON_IMPL(Methodspace, methodspace);

ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofIdHashMap, Inheritance,
    inheritance);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofArrayBuffer, Methods,
    methods);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofArray, Imports,
    imports);

value_t methodspace_validate(value_t value) {
  VALIDATE_FAMILY(ofMethodspace, value);
  VALIDATE_FAMILY(ofIdHashMap, get_methodspace_inheritance(value));
  return success();
}

value_t ensure_methodspace_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_methodspace_inheritance(self)));
  TRY(ensure_frozen(runtime, get_methodspace_methods(self)));
  TRY(ensure_frozen(runtime, get_methodspace_imports(self)));
  return success();
}

value_t add_methodspace_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype) {
  CHECK_FAMILY(ofMethodspace, self);
  CHECK_MUTABLE(self);
  CHECK_FAMILY(ofProtocol, subtype);
  CHECK_FAMILY(ofProtocol, supertype);
  value_t inheritance = get_methodspace_inheritance(self);
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

value_t add_methodspace_method(runtime_t *runtime, value_t self,
    value_t method) {
  CHECK_FAMILY(ofMethodspace, self);
  CHECK_MUTABLE(self);
  CHECK_FAMILY(ofMethod, method);
  return add_to_array_buffer(runtime, get_methodspace_methods(self), method);
}

value_t get_protocol_parents(runtime_t *runtime, value_t space, value_t protocol) {
  value_t inheritance = get_methodspace_inheritance(space);
  value_t parents = get_id_hash_map_at(inheritance, protocol);
  if (is_signal(scNotFound, parents)) {
    return ROOT(runtime, empty_array_buffer);
  } else {
    return parents;
  }
}

// The state maintained while doing method space lookup.
typedef struct {
  // The resulting method to return, or an appropriate signal.
  value_t result;
  // Running argument-wise max over all the methods that have matched.
  score_t *max_score;
  // We use two scratch offsets vectors such that we can keep the best in one
  // and the other as scratch, swapping them around when a new best one is
  // found.
  size_t *result_offsets;
  size_t *scratch_offsets;
  // Is the current max score vector synthetic, that is, is it taken over
  // several ambiguous methods that are each individually smaller than their
  // max?
  bool max_is_synthetic;
} methodspace_lookup_state_t;

// Swaps around the result and scratch offsets.
static void methodspace_lookup_state_swap_offsets(methodspace_lookup_state_t *state) {
  size_t *temp = state->result_offsets;
  state->result_offsets = state->scratch_offsets;
  state->scratch_offsets = temp;
}

// The max amount of arguments for which we'll allocate the lookup state on the
// stack.
#define kSmallLookupLimit 32

// Continue a search for a method method locally in the given method space, that
// is, this does not follow imports and only updates the lookup state, it doesn't
// return the best match.
static void lookup_methodspace_local_method(methodspace_lookup_state_t *state,
    runtime_t *runtime, value_t space, value_t record, frame_t *frame) {
  match_info_t match_info;
  match_info_init(&match_info, state->max_score, state->result_offsets,
      kSmallLookupLimit);
  value_t methods = get_methodspace_methods(space);
  size_t method_count = get_array_buffer_length(methods);
  size_t current = 0;
  // First scan until we find the first match, using the max score vector
  // to hold the score directly. Until we have at least one match there's no
  // point in doing comparisons.
  for (; current < method_count; current++) {
    value_t method = get_array_buffer_at(methods, current);
    value_t signature = get_method_signature(method);
    match_result_t match = match_signature(runtime, signature, record, frame,
        space, &match_info);
    if (match_result_is_match(match)) {
      state->result = method;
      current++;
      break;
    }
  }
  // Continue scanning but compare every new match to the existing best score.
  score_t scratch_score[kSmallLookupLimit];
  match_info_init(&match_info, scratch_score, state->scratch_offsets,
      kSmallLookupLimit);
  size_t arg_count = get_invocation_record_argument_count(record);
  for (; current < method_count; current++) {
    value_t method = get_array_buffer_at(methods, current);
    value_t signature = get_method_signature(method);
    match_result_t match = match_signature(runtime, signature, record, frame,
        space, &match_info);
    if (!match_result_is_match(match))
      continue;
    join_status_t status = join_score_vectors(state->max_score, scratch_score,
        arg_count);
    if (status == jsBetter || (state->max_is_synthetic && status == jsEqual)) {
      // This score is either better than the previous best, or it is equal to
      // the max which is itself synthetic and hence better than any of the
      // methods we've seen so far.
      state->result = method;
      // Now the max definitely isn't synthetic.
      state->max_is_synthetic = false;
      // The offsets for the result is now stored in scratch_offsets and we have
      // no more use for the previous result_offsets so we swap them around.
      methodspace_lookup_state_swap_offsets(state);
      // And then we have to update the match info with the new scratch offsets.
      match_info_init(&match_info, scratch_score, state->scratch_offsets,
          kSmallLookupLimit);
    } else if (status != jsWorse) {
      // The next score was not strictly worse than the best we've seen so we
      // don't have a unique best.
      state->result = new_signal(scNotFound);
      // If the result is ambiguous that means the max is now synthetic.
      state->max_is_synthetic = (status == jsAmbiguous);
    }
  }
}

// Do a transitive method lookup in the given method space, that is, look up
// locally and in any imported spaces.
static void lookup_methodspace_transitive_method(methodspace_lookup_state_t *state,
    runtime_t *runtime, value_t space, value_t record, frame_t *frame) {
  lookup_methodspace_local_method(state, runtime, space, record, frame);
  value_t imports = get_methodspace_imports(space);
  for (size_t i = 0; i < get_array_length(imports); i++) {
    value_t import = get_array_at(imports, i);
    lookup_methodspace_transitive_method(state, runtime, import, record, frame);
  }
}

// Given an array of offsets, builds and returns an argument map that performs
// that offset mapping.
static value_t build_argument_map(runtime_t *runtime, size_t offsetc, size_t *offsets) {
  value_t current_node = MROOT(runtime, argument_map_trie_root);
  for (size_t i = 0; i < offsetc; i++) {
    size_t offset = offsets[i];
    value_t value = (offset == kNoOffset) ? ROOT(runtime, null) : new_integer(offset);
    TRY_SET(current_node, get_argument_map_trie_child(runtime, current_node, value));
  }
  return get_argument_map_trie_value(current_node);
}

value_t lookup_methodspace_method(runtime_t *runtime, value_t space,
    value_t record, frame_t *frame, value_t *arg_map_out) {
  // Input validation.
  size_t arg_count = get_invocation_record_argument_count(record);
  CHECK_REL("too many arguments", arg_count, <=, kSmallLookupLimit);
  // Initialize the lookup state using stack-allocated space.
  score_t max_score[kSmallLookupLimit];
  size_t offsets_one[kSmallLookupLimit];
  size_t offsets_two[kSmallLookupLimit];
  methodspace_lookup_state_t state;
  state.result = new_signal(scNotFound);
  state.max_is_synthetic = false;
  state.max_score = max_score;
  state.result_offsets = offsets_one;
  state.scratch_offsets = offsets_two;
  // Perform the lookup.
  lookup_methodspace_transitive_method(&state, runtime, space, record, frame);
  // Post-process the result.
  if (!in_domain(vdSignal, state.result)) {
    // We have a result so we need to build an argument map that represents the
    // result's offsets vector.
    TRY_SET(*arg_map_out, build_argument_map(runtime, arg_count, state.result_offsets));
  }
  return state.result;
}

value_t set_methodspace_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(raw_methods, get_id_hash_map_at(contents, RSTR(runtime, methods)));
  TRY_DEF(methods, new_heap_array_buffer_with_contents(runtime, raw_methods));
  TRY_DEF(inheritance, get_id_hash_map_at(contents, RSTR(runtime, inheritance)));
  TRY_DEF(imports, get_id_hash_map_at(contents, RSTR(runtime, imports)));
  set_methodspace_methods(object, methods);
  set_methodspace_inheritance(object, inheritance);
  set_methodspace_imports(object, imports);
  return success();
}


// --- I n v o c a t i o n   r e c o r d ---

ACCESSORS_IMPL(InvocationRecord, invocation_record, acInFamily, ofArray,
    ArgumentVector, argument_vector);

value_t invocation_record_validate(value_t self) {
  VALIDATE_FAMILY(ofInvocationRecord, self);
  VALIDATE_FAMILY(ofArray, get_invocation_record_argument_vector(self));
  return success();
}

value_t get_invocation_record_tag_at(value_t self, size_t index) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_pair_array_first_at(argument_vector, index);
}

size_t get_invocation_record_offset_at(value_t self, size_t index) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_integer_value(get_pair_array_second_at(argument_vector, index));
}

size_t get_invocation_record_argument_count(value_t self) {
  CHECK_FAMILY(ofInvocationRecord, self);
  value_t argument_vector = get_invocation_record_argument_vector(self);
  return get_array_length(argument_vector) >> 1;
}

value_t build_invocation_record_vector(runtime_t *runtime, value_t tags) {
  size_t tag_count = get_array_length(tags);
  TRY_DEF(result, new_heap_pair_array(runtime, tag_count));
  for (size_t i = 0; i < tag_count; i++) {
    set_pair_array_first_at(result, i, get_array_at(tags, i));
    // The offset is counted backwards because the argument evaluated last will
    // be at the top of the stack, that is, offset 0, and the first will be at
    // the bottom so has the highest offset.
    size_t offset = tag_count - i - 1;
    set_pair_array_second_at(result, i, new_integer(offset));
  }
  co_sort_pair_array(result);
  return result;
}

value_t get_invocation_record_argument_at(value_t self, frame_t *frame, size_t index) {
  size_t offset = get_invocation_record_offset_at(self, index);
  return frame_peek_value(frame, offset);
}

void print_invocation(value_t record, frame_t *frame) {
  string_buffer_t buf;
  string_buffer_init(&buf);
  size_t arg_count = get_invocation_record_argument_count(record);
  string_buffer_printf(&buf, "{");
  for (size_t i = 0;  i < arg_count; i++) {
    value_t tag = get_invocation_record_tag_at(record, i);
    value_t arg = get_invocation_record_argument_at(record, frame, i);
    if (i > 0)
      string_buffer_printf(&buf, ", ");
    value_print_on(tag, &buf);
    string_buffer_printf(&buf, ": ");
    value_print_on(arg, &buf);
  }
  string_buffer_printf(&buf, "}");
  string_t str;
  string_buffer_flush(&buf, &str);
  printf("%s\n", str.chars);
  fflush(stdout);
  string_buffer_dispose(&buf);
}

void invocation_record_print_on(value_t self, string_buffer_t *buf) {
  string_buffer_printf(buf, "{");
  size_t arg_count = get_invocation_record_argument_count(self);
  for (size_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(buf, ", ");
    value_t tag = get_invocation_record_tag_at(self, i);
    size_t offset = get_invocation_record_offset_at(self, i);
    value_print_atomic_on(tag, buf);
    string_buffer_printf(buf, "@%i", offset);
  }
  string_buffer_printf(buf, "}");
}

void invocation_record_print_atomic_on(value_t self, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<invocation_record>");
}

value_t ensure_invocation_record_owned_values_frozen(runtime_t *runtime,
    value_t self) {
  TRY(ensure_frozen(runtime, get_invocation_record_argument_vector(self)));
  return success();
}

// --- O p e r a t i o n ---

INTEGER_ACCESSORS_IMPL(Operation, operation, Type, type);
ACCESSORS_IMPL(Operation, operation, acNoCheck, 0, Value, value);

value_t operation_validate(value_t self) {
  VALIDATE_FAMILY(ofOperation, self);
  return success();
}

void operation_print_on(value_t self, string_buffer_t *buf) {
  operation_print_atomic_on(self, buf);
}

value_t operation_transient_identity_hash(value_t self, size_t depth) {
  if (depth > kCircularObjectDepthThreshold)
    return new_signal(scMaybeCircular);
  value_t value = get_operation_value(self);
  operation_type_t type = get_operation_type(self);
  TRY_DEF(value_hash, value_transient_identity_hash_cycle_protect(value,
      depth + 1));
  return new_integer(~(get_integer_value(value_hash) ^ type));
}

value_t operation_identity_compare(value_t a, value_t b, size_t depth) {
  if (get_operation_type(a) != get_operation_type(b))
    return internal_false_value();
  if (depth > kCircularObjectDepthThreshold)
    return new_signal(scMaybeCircular);
  return value_identity_compare_cycle_protect(get_operation_value(a),
      get_operation_value(b), depth + 1);
}

void operation_print_atomic_on(value_t self, string_buffer_t *buf) {
  value_t value = get_operation_value(self);
  switch (get_operation_type(self)) {
    case otAssign:
      value_print_atomic_on_unquoted(value, buf);
      string_buffer_printf(buf, ":=");
      break;
    case otCall:
      string_buffer_printf(buf, "()");
      break;
    case otIndex:
      string_buffer_printf(buf, "[]");
      break;
    case otInfix:
      string_buffer_printf(buf, ".");
      value_print_atomic_on_unquoted(value, buf);
      string_buffer_printf(buf, "()");
      break;
    case otPrefix:
      value_print_atomic_on_unquoted(value, buf);
      string_buffer_printf(buf, "()");
      break;
    case otProperty:
      string_buffer_printf(buf, ".");
      value_print_atomic_on_unquoted(value, buf);
      break;
    case otSuffix:
      string_buffer_printf(buf, "()");
      value_print_atomic_on_unquoted(value, buf);
      break;
    default:
      UNREACHABLE("unexpected operation type");
      break;
  }
}
