// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "log.h"
#include "method.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "value-inl.h"


void signature_map_lookup_input_init(signature_map_lookup_input_t *input,
    value_t ambience, value_t record, frame_t *frame, void *data) {
  input->runtime = get_ambience_runtime(ambience);
  input->ambience = ambience;
  input->record = record;
  input->frame = frame;
  input->data = data;
}


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

void signature_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<signature: ");
  for (size_t i = 0; i < get_signature_parameter_count(self); i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_print_inner_on(get_signature_tag_at(self, i), context, -1);
    string_buffer_printf(context->buf, ":");
    value_t param = get_signature_parameter_at(self, i);
    value_print_inner_on(get_parameter_guard(param), context, -1);
  }
  string_buffer_printf(context->buf, ">");
}

bool match_result_is_match(match_result_t value) {
  return value >= mrMatch;
}

void match_info_init(match_info_t *info, value_t *scores, size_t *offsets,
    size_t capacity) {
  info->scores = scores;
  info->offsets = offsets;
  info->capacity = capacity;
}

// This guard matched perfectly.
static value_t new_identical_match_score() {
  return new_score(scEq, 0);
}

// It's not an identical match but the closest possible instanceof-match.
static value_t new_perfect_is_match_score() {
  return new_score(scIs, 0);
}

// Score that signifies that a guard didn't match at all.
static value_t new_no_match_score() {
  return new_score(scNone, 0);
}

// There was a match but only because extra arguments are allowed so anything
// more specific would match better.
static value_t new_extra_match_score() {
  return new_score(scExtra, 0);
}

// The guard matched the given value but only because it matches any value so
// anything more specific would match better.
static value_t new_any_match_score() {
  return  new_score(scAny, 0);
}

value_t match_signature(value_t self, signature_map_lookup_input_t *input,
    value_t space, match_info_t *match_info, match_result_t *result_out) {
  TOPIC_INFO(Lookup, "Matching against %4v", self);
  size_t argument_count = get_invocation_record_argument_count(input->record);
  CHECK_REL("score array too short", argument_count, <=, match_info->capacity);
  // Fast case if fewer than that minimum number of arguments is given.
  size_t mandatory_count = get_signature_mandatory_count(self);
  if (argument_count < mandatory_count) {
    *result_out = mrMissingArgument;
    return success();
  }
  // Fast case if too many arguments are given.
  size_t param_count = get_signature_parameter_count(self);
  bool allow_extra = get_signature_allow_extra(self);
  if (!allow_extra && (argument_count > param_count)) {
    *result_out = mrUnexpectedArgument;
    return success();
  }
  // Vector of parameters seen. This is used to ensure that we only see each
  // parameter once.
  bit_vector_t params_seen;
  bit_vector_init(&params_seen, param_count, false);
  // Count how many mandatory parameters we see so we can check that we see all
  // of them.
  size_t mandatory_seen_count = 0;
  // The value to return if there is a match.
  match_result_t on_match = mrMatch;
  // Clear the score vector.
  for (size_t i = 0; i < argument_count; i++) {
    match_info->scores[i] = new_no_match_score();
    match_info->offsets[i] = kNoOffset;
  }
  // Scan through the arguments and look them up in the signature.
  value_t tags = get_signature_tags(self);
  for (size_t i = 0; i < argument_count; i++) {
    value_t tag = get_invocation_record_tag_at(input->record, i);
    // TODO: propagate any errors caused by this.
    value_t param = binary_search_pair_array(tags, tag);
    if (is_condition(ccNotFound, param)) {
      // The tag wasn't found in this signature.
      if (allow_extra) {
        // It's fine, this signature allows extra arguments.
        on_match = mrExtraMatch;
        match_info->scores[i] = new_extra_match_score();
        continue;
      } else {
        // This signature doesn't allow extra arguments so we bail out.
        bit_vector_dispose(&params_seen);
        *result_out = mrUnexpectedArgument;
        return success();
      }
    }
    CHECK_FALSE("binary search failed", get_value_domain(tags) == vdCondition);
    // The tag matched one in this signature.
    size_t index = get_parameter_index(param);
    if (bit_vector_get_at(&params_seen, index)) {
      // We've now seen two tags that match the same parameter. Bail out.
      bit_vector_dispose(&params_seen);
      *result_out = mrRedundantArgument;
      return success();
    }
    value_t value = get_invocation_record_argument_at(input->record, input->frame, i);
    value_t score;
    TRY(guard_match(get_parameter_guard(param), value, input, space, &score));
    if (!is_score_match(score)) {
      // The guard says the argument doesn't match. Bail out.
      bit_vector_dispose(&params_seen);
      *result_out = mrGuardRejected;
      return success();
    }
    // We got a match! Record the result and move on to the next.
    bit_vector_set_at(&params_seen, index, true);
    match_info->scores[i] = score;
    match_info->offsets[index] = get_invocation_record_offset_at(input->record, i);
    if (!get_parameter_is_optional(param))
      mandatory_seen_count++;
  }
  bit_vector_dispose(&params_seen);
  if (mandatory_seen_count < get_signature_mandatory_count(self)) {
    // All arguments matched but there were mandatory arguments missing so it's
    // no good.
    *result_out = mrMissingArgument;
    return success();
  } else {
    // Everything matched including all mandatories. We're golden.
    *result_out = on_match;
    return success();
  }
}

join_status_t join_score_vectors(value_t *target, value_t *source, size_t length) {
  // The bit fiddling here works because of how the enum values are chosen.
  join_status_t result = jsEqual;
  for (size_t i = 0; i < length; i++) {
    int cmp = compare_tagged_scores(source[i], target[i]);
    if (cmp < 0) {
      // The source was strictly worse than the target.
      result = SET_ENUM_FLAG(join_status_t, result, jsWorse);
    } else if (cmp > 0) {
      // The source was strictly better than the target; override.
      result = SET_ENUM_FLAG(join_status_t, result, jsBetter);
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

value_t parameter_validate(value_t value) {
  VALIDATE_FAMILY(ofParameter, value);
  VALIDATE_FAMILY_OPT(ofGuard, get_parameter_guard(value));
  VALIDATE_FAMILY_OPT(ofArray, get_parameter_tags(value));
  return success();
}

void parameter_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofParameter, self);
  string_buffer_printf(context->buf, "#<parameter: gd@");
  // We know the guard is a guard, not a parameter, so this can't cause a cycle.
  value_print_inner_on(get_parameter_guard(self), context, -1);
  string_buffer_printf(context->buf, ", op@%i, ix@%i>",
      get_parameter_is_optional(self), get_parameter_index(self));
}


// --- G u a r d ---

ENUM_ACCESSORS_IMPL(Guard, guard, guard_type_t, Type, type);
ACCESSORS_IMPL(Guard, guard, acNoCheck, 0, Value, value);

value_t guard_validate(value_t value) {
  VALIDATE_FAMILY(ofGuard, value);
  return success();
}

// Given two scores returns the best of them.
static value_t best_score(value_t a, value_t b) {
  return (compare_tagged_scores(a, b) > 0) ? a : b;
}

static value_t find_best_match(runtime_t *runtime, value_t current,
    value_t target, value_t current_score, value_t space, value_t *score_out) {
  if (value_identity_compare(current, target)) {
    *score_out = current_score;
    return success();
  } else {
    TRY_DEF(parents, get_type_parents(runtime, space, current));
    size_t length = get_array_buffer_length(parents);
    value_t score = new_no_match_score();
    for (size_t i = 0; i < length; i++) {
      value_t parent = get_array_buffer_at(parents, i);
      value_t next_score = whatever();
      TRY(find_best_match(runtime, parent, target, get_score_successor(current_score),
          space, &next_score));
      score = best_score(score, next_score);
    }
    *score_out = score;
    return success();
  }
}

value_t guard_match(value_t guard, value_t value,
    signature_map_lookup_input_t *lookup_input, value_t space, value_t *score_out) {
  CHECK_FAMILY(ofGuard, guard);
  switch (get_guard_type(guard)) {
    case gtEq: {
      value_t guard_value = get_guard_value(guard);
      bool match = value_identity_compare(guard_value, value);
      *score_out = match ? new_identical_match_score() : new_no_match_score();
      return success();
    }
    case gtIs: {
      TRY_DEF(primary, get_primary_type(value, lookup_input->runtime));
      value_t target = get_guard_value(guard);
      return find_best_match(lookup_input->runtime, primary, target,
          new_perfect_is_match_score(), space, score_out);
    }
    case gtAny:
      *score_out = new_any_match_score();
      return success();
    default:
      UNREACHABLE("Unknown guard type");
      return new_condition(ccWat);
  }
}

void guard_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofGuard, self);
  switch (get_guard_type(self)) {
    case gtEq:
      string_buffer_printf(context->buf, "eq(");
      value_print_inner_on(get_guard_value(self), context, -1);
      string_buffer_printf(context->buf, ")");
      break;
    case gtIs:
      string_buffer_printf(context->buf, "is(");
      value_print_inner_on(get_guard_value(self), context, -1);
      string_buffer_printf(context->buf, ")");
      break;
    case gtAny:
      string_buffer_printf(context->buf, "any()");
      break;
  }
}


// --- M e t h o d ---

ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofSignature, Signature, signature);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofCodeBlock, Code, code);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofMethodAst, Syntax, syntax);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofModuleFragment, ModuleFragment,
    module_fragment);

value_t method_validate(value_t self) {
  VALIDATE_FAMILY(ofMethod, self);
  VALIDATE_FAMILY_OPT(ofSignature, get_method_signature(self));
  VALIDATE_FAMILY_OPT(ofCodeBlock, get_method_code(self));
  VALIDATE_FAMILY_OPT(ofMethodAst, get_method_syntax(self));
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_method_module_fragment(self));
  return success();
}

void method_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<method ");
  value_t signature = get_method_signature(self);
  value_print_inner_on(signature, context, -1);
  string_buffer_printf(context->buf, " ");
  value_t syntax = get_method_syntax(self);
  value_print_inner_on(syntax, context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t compile_method_ast_to_method(runtime_t *runtime, value_t method_ast,
    value_t fragment) {
  reusable_scratch_memory_t scratch;
  reusable_scratch_memory_init(&scratch);
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(signature, build_method_signature(runtime, fragment, &scratch,
        get_method_ast_signature(method_ast)));
    E_TRY_DEF(method, new_heap_method(runtime, afMutable, signature, method_ast,
        nothing(), fragment));
    E_RETURN(method);
  E_FINALLY();
    reusable_scratch_memory_dispose(&scratch);
  E_END_TRY_FINALLY();
}

// --- S i g n a t u r e   m a p ---

TRIVIAL_PRINT_ON_IMPL(SignatureMap, signature_map);

ACCESSORS_IMPL(SignatureMap, signature_map, acInFamily, ofArrayBuffer, Entries,
    entries);

value_t signature_map_validate(value_t value) {
  VALIDATE_FAMILY(ofSignatureMap, value);
  VALIDATE_FAMILY(ofArrayBuffer, get_signature_map_entries(value));
  return success();
}

value_t add_to_signature_map(runtime_t *runtime, value_t map, value_t signature,
    value_t value) {
  CHECK_FAMILY(ofSignatureMap, map);
  CHECK_FAMILY(ofSignature, signature);
  value_t entries = get_signature_map_entries(map);
  TRY(add_to_pair_array_buffer(runtime, entries, signature, value));
  return success();
}

value_t ensure_signature_map_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_signature_map_entries(self)));
  return success();
}

// The state maintained while doing signature map lookup.
struct signature_map_lookup_state_t {
  // The resulting value to return, or an appropriate condition.
  value_t result;
  // Running argument-wise max over all the entries that have matched.
  value_t *max_score;
  // We use two scratch offsets vectors such that we can keep the best in one
  // and the other as scratch, swapping them around when a new best one is
  // found.
  size_t *result_offsets;
  size_t *scratch_offsets;
  // Is the current max score vector synthetic, that is, is it taken over
  // several ambiguous entries that are each individually smaller than their
  // max?
  bool max_is_synthetic;
  // The input data used as the basis of the lookup.
  signature_map_lookup_input_t input;
};

// Swaps around the result and scratch offsets.
static void signature_map_lookup_state_swap_offsets(signature_map_lookup_state_t *state) {
  size_t *temp = state->result_offsets;
  state->result_offsets = state->scratch_offsets;
  state->scratch_offsets = temp;
}

// The max amount of arguments for which we'll allocate the lookup state on the
// stack.
#define kSmallLookupLimit 16

value_t continue_signature_map_lookup(signature_map_lookup_state_t *state,
    value_t sigmap, value_t space) {
  CHECK_FAMILY(ofSignatureMap, sigmap);
  CHECK_FAMILY(ofMethodspace, space);
  TOPIC_INFO(Lookup, "Looking up in signature map %v", sigmap);
  value_t entries = get_signature_map_entries(sigmap);
  size_t entry_count = get_pair_array_buffer_length(entries);
  value_t scratch_score[kSmallLookupLimit];
  match_info_t match_info;
  match_info_init(&match_info, scratch_score, state->scratch_offsets,
      kSmallLookupLimit);
  size_t arg_count = get_invocation_record_argument_count(state->input.record);
  for (size_t current = 0; current < entry_count; current++) {
    value_t signature = get_pair_array_buffer_first_at(entries, current);
    value_t value = get_pair_array_buffer_second_at(entries, current);
    match_result_t match;
    TRY(match_signature(signature, &state->input, space, &match_info, &match));
    if (!match_result_is_match(match))
      continue;
    join_status_t status = join_score_vectors(state->max_score, scratch_score,
        arg_count);
    if (status == jsBetter || (state->max_is_synthetic && status == jsEqual)) {
      // This score is either better than the previous best, or it is equal to
      // the max which is itself synthetic and hence better than any of the
      // entries we've seen so far.
      state->result = value;
      // Now the max definitely isn't synthetic.
      state->max_is_synthetic = false;
      // The offsets for the result is now stored in scratch_offsets and we have
      // no more use for the previous result_offsets so we swap them around.
      signature_map_lookup_state_swap_offsets(state);
      // And then we have to update the match info with the new scratch offsets.
      match_info_init(&match_info, scratch_score, state->scratch_offsets,
          kSmallLookupLimit);
    } else if (status != jsWorse) {
      // If we hit the exact same entry more than once, which can happen if
      // the same signature map is traversed more than once, that's okay we just
      // skip.
      if (is_same_value(value, state->result))
        continue;
      // The next score was not strictly worse than the best we've seen so we
      // don't have a unique best.
      state->result = new_lookup_error_condition(lcAmbiguity);
      // If the result is ambiguous that means the max is now synthetic.
      state->max_is_synthetic = (status == jsAmbiguous);
    }
  }
  return success();
}

value_t do_signature_map_lookup(value_t ambience, value_t record, frame_t *frame,
    signature_map_lookup_callback_t callback, void *data) {
  // For now we only handle lookups of a certain size. Hopefully by the time
  // this is too small this implementation will be gone anyway.
  size_t arg_count = get_invocation_record_argument_count(record);
  CHECK_REL("too many arguments", arg_count, <=, kSmallLookupLimit);
  // Initialize the lookup state using stack-allocated space.
  value_t max_score[kSmallLookupLimit];
  // The following code will assume the max score has a meaningful value so
  // reset it explicitly.
  for (size_t i = 0; i < arg_count; i++)
    max_score[i] = new_no_match_score();
  size_t offsets_one[kSmallLookupLimit];
  size_t offsets_two[kSmallLookupLimit];
  signature_map_lookup_state_t state;
  signature_map_lookup_input_init(&state.input, ambience, record, frame, data);
  state.result = new_lookup_error_condition(lcNoMatch);
  state.max_is_synthetic = false;
  state.max_score = max_score;
  state.result_offsets = offsets_one;
  state.scratch_offsets = offsets_two;
  TRY(callback(&state));
  return state.result;
}

// Given an array of offsets, builds and returns an argument map that performs
// that offset mapping.
static value_t build_argument_map(runtime_t *runtime, size_t offsetc, size_t *offsets) {
  value_t current_node = MROOT(runtime, argument_map_trie_root);
  for (size_t i = 0; i < offsetc; i++) {
    size_t offset = offsets[i];
    value_t value = (offset == kNoOffset) ? null() : new_integer(offset);
    TRY_SET(current_node, get_argument_map_trie_child(runtime, current_node, value));
  }
  return get_argument_map_trie_value(current_node);
}

value_t get_signature_map_lookup_argument_map(signature_map_lookup_state_t *state) {
  if (in_domain(vdCondition, state->result)) {
    return whatever();
  } else {
    size_t arg_count = get_invocation_record_argument_count(state->input.record);
    return build_argument_map(state->input.runtime, arg_count, state->result_offsets);
  }
}


// --- M e t h o d   s p a c e ---

ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofIdHashMap, Inheritance,
    inheritance);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofSignatureMap, Methods,
    methods);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofArrayBuffer, Imports,
    imports);

value_t methodspace_validate(value_t value) {
  VALIDATE_FAMILY(ofMethodspace, value);
  VALIDATE_FAMILY(ofIdHashMap, get_methodspace_inheritance(value));
  VALIDATE_FAMILY(ofSignatureMap, get_methodspace_methods(value));
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
  CHECK_FAMILY(ofType, subtype);
  CHECK_FAMILY(ofType, supertype);
  value_t inheritance = get_methodspace_inheritance(self);
  value_t parents = get_id_hash_map_at(inheritance, subtype);
  if (is_condition(ccNotFound, parents)) {
    // Make the parents buffer small since most types don't have many direct
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

value_t add_methodspace_import(runtime_t *runtime, value_t self, value_t imported) {
  CHECK_FAMILY(ofMethodspace, self);
  CHECK_FAMILY(ofMethodspace, imported);
  CHECK_MUTABLE(self);
  value_t imports = get_methodspace_imports(self);
  return add_to_array_buffer(runtime, imports, imported);
}

value_t add_methodspace_method(runtime_t *runtime, value_t self,
    value_t method) {
  CHECK_FAMILY(ofMethodspace, self);
  CHECK_MUTABLE(self);
  CHECK_FAMILY(ofMethod, method);
  value_t signature = get_method_signature(method);
  return add_to_signature_map(runtime, get_methodspace_methods(self), signature,
      method);
}

value_t get_type_parents(runtime_t *runtime, value_t space, value_t type) {
  value_t inheritance = get_methodspace_inheritance(space);
  value_t parents = get_id_hash_map_at(inheritance, type);
  if (is_condition(ccNotFound, parents)) {
    return ROOT(runtime, empty_array_buffer);
  } else {
    return parents;
  }
}

// Does a full exhaustive lookup through the tags of the invocation for the
// subject of this call. Returns a not found condition if there is no subject.
static value_t get_invocation_subject_no_shortcut(
    signature_map_lookup_state_t *state) {
  value_t record = state->input.record;
  size_t argc = get_invocation_record_argument_count(record);
  for (size_t i = 0; i < argc; i++) {
    value_t tag = get_invocation_record_tag_at(record, i);
    if (is_same_value(tag, ROOT(state->input.runtime, subject_key)))
      return get_invocation_record_argument_at(record, state->input.frame, i);
  }
  return new_not_found_condition();
}

// Returns the subject of the invocation, using the fact that the subject sorts
// lowest so it must be at parameter index 0 if it is there at all. Note that
// _parameter_ index 0 is not the same as _argument_ index 0, it doesn't have
// to be the 0'th argument (that is, the first in evaluation order) for this
// to work. Rather, the argument index must be given by the 0'th entry of the
// invocation record. Potentially confusingly, the argument index will actually
// almost always be 0 as well but that's not what we're using here (since we're
// hardcoding the index we need _always_ always, not _almost_ always).
static value_t get_invocation_subject_with_shortcut(
    signature_map_lookup_state_t *state) {
  value_t record = state->input.record;
  value_t tag_zero = get_invocation_record_tag_at(record, 0);
  if (is_same_value(tag_zero, ROOT(state->input.runtime, subject_key)))
    return get_invocation_record_argument_at(record, state->input.frame, 0);
  else
    return new_not_found_condition();
}

// Ensures that the given methodspace as well as all transitive dependencies
// are present in the given cache array.
static value_t ensure_methodspace_transitive_dependencies(runtime_t *runtime,
    value_t methodspace, value_t cache) {
  CHECK_FAMILY(ofMethodspace, methodspace);
  CHECK_FAMILY(ofArrayBuffer, cache);
  TRY(ensure_array_buffer_contains(runtime, cache, methodspace));
  value_t imports = get_methodspace_imports(methodspace);
  for (size_t i = 0; i < get_array_buffer_length(imports); i++) {
    value_t import = get_array_buffer_at(imports, i);
    TRY(ensure_methodspace_transitive_dependencies(runtime, import, cache));
  }
  return success();
}

value_t get_or_create_module_fragment_methodspaces_cache(runtime_t *runtime,
    value_t fragment) {
  CHECK_FAMILY(ofModuleFragment, fragment);
  value_t cache = get_module_fragment_methodspaces_cache(fragment);
  if (!is_nothing(cache)) {
    CHECK_FAMILY(ofArrayBuffer, cache);
    return cache;
  }
  // The next part requires the fragment to be mutable.
  CHECK_MUTABLE(fragment);
  // There is no cache available; build it now. As always, remember to do any
  // allocation before modifying objects to make sure we don't end up in an
  // inconsistent state if allocation fails.
  TRY_SET(cache, new_heap_array_buffer(runtime, 16));
  // We catch cycles (crudely) by setting the cache to an invalid value which
  // will be caught by the check above. It's not that cycles couldn't in
  // principle make sense but it'll take some thinking through to ensure that it
  // really does make sense.
  set_module_fragment_methodspaces_cache(fragment, new_integer(0));
  // Scan through this fragment and its predecessors and add their transitive
  // dependencies.
  value_t module = get_module_fragment_module(fragment);
  value_t current = fragment;
  while (!is_condition(ccNotFound, current)) {
    value_t methodspace = get_module_fragment_methodspace(current);
    TRY(ensure_methodspace_transitive_dependencies(runtime, methodspace, cache));
    value_t stage = get_module_fragment_stage(current);
    current = get_module_fragment_before(module, stage);
  }
  TRY(ensure_frozen(runtime, cache));
  set_module_fragment_methodspaces_cache(fragment, cache);
  return cache;
}

// Performs a method lookup through the given fragment, that is, in the fragment
// itself and any of the siblings before it.
static value_t lookup_through_fragment(signature_map_lookup_state_t *state,
    value_t fragment) {
  CHECK_FAMILY(ofModuleFragment, fragment);
  value_t space = get_module_fragment_methodspace(fragment);
  TRY_DEF(methodspaces, get_or_create_module_fragment_methodspaces_cache(
      state->input.runtime, fragment));
  for (size_t i = 0; i < get_array_buffer_length(methodspaces); i++) {
    value_t methodspace = get_array_buffer_at(methodspaces, i);
    value_t sigmap = get_methodspace_methods(methodspace);
    TRY(continue_signature_map_lookup(state, sigmap, space));
  }
  return success();
}

// Perform a method lookup in the subject's module of origin.
static value_t lookup_subject_methods(signature_map_lookup_state_t *state) {
  // Look for a subject value, if there is none there is nothing to do.
  value_t subject = get_invocation_subject_with_shortcut(state);
  TOPIC_INFO(Lookup, "Subject value: %v", subject);
  if (is_condition(ccNotFound, subject)) {
    // Just in case, check that the shortcut version gave the correct answer.
    // The case where it returns a non-condition is trivially correct (FLW) so this
    // is the only case there can be any doubt about.
    IF_EXPENSIVE_CHECKS_ENABLED(CHECK_TRUE("Subject shortcut didn't work",
        is_condition(ccNotFound, get_invocation_subject_no_shortcut(state))));
    return success();
  }
  // Extract the origin of the subject.
  value_t type = get_primary_type(subject, state->input.runtime);
  TOPIC_INFO(Lookup, "Subject type: %v", type);
  CHECK_FAMILY(ofType, type);
  value_t origin = get_type_origin(type, state->input.ambience);
  TOPIC_INFO(Lookup, "Subject origin: %v", origin);
  if (is_nothing(origin))
    // Some types have no origin (at least at the moment) and that's okay, we
    // just don't perform the extra lookup.
    return success();
  TRY(lookup_through_fragment(state, origin));
  return success();
}

// A pair of a value and an argument map. If only C had generic types.
typedef struct {
  value_t value;
  value_t *arg_map_out;
} value_and_argument_map_t;

// Performs a full method lookup through the current module and the subject's
// module of origin.
static value_t do_full_method_lookup(signature_map_lookup_state_t *state) {
  value_and_argument_map_t *data = (value_and_argument_map_t*) state->input.data;
  CHECK_FAMILY(ofModuleFragment, data->value);
  TOPIC_INFO(Lookup, "Performing fragment lookup %v", state->input.record);
  TRY(lookup_through_fragment(state, data->value));
  TOPIC_INFO(Lookup, "Performing subject lookup");
  TRY(lookup_subject_methods(state));
  TOPIC_INFO(Lookup, "Lookup result: %v", state->result);
  TRY_SET(*data->arg_map_out, get_signature_map_lookup_argument_map(state));
  return success();
}

value_t lookup_method_full(value_t ambience, value_t fragment,
    value_t record, frame_t *frame, value_t *arg_map_out) {
  value_and_argument_map_t data;
  data.value = fragment;
  data.arg_map_out = arg_map_out;
  return do_signature_map_lookup(ambience, record, frame, do_full_method_lookup,
      &data);
}

// Do a transitive method lookup in the given method space, that is, look up
// locally and in any imported spaces.
static value_t lookup_methodspace_transitive_method(signature_map_lookup_state_t *state,
    value_t space) {
  value_t local_methods = get_methodspace_methods(space);
  TRY(continue_signature_map_lookup(state, local_methods, space));
  value_t imports = get_methodspace_imports(space);
  for (size_t i = 0; i < get_array_buffer_length(imports); i++) {
    value_t import = get_array_buffer_at(imports, i);
    TRY(lookup_methodspace_transitive_method(state, import));
  }
  return success();
}

// Performs a method lookup within a single methodspace.
static value_t do_methodspace_method_lookup(signature_map_lookup_state_t *state) {
  value_and_argument_map_t *data = (value_and_argument_map_t*) state->input.data;
  CHECK_FAMILY(ofMethodspace, data->value);
  TRY(lookup_methodspace_transitive_method(state, data->value));
  TRY_SET(*data->arg_map_out, get_signature_map_lookup_argument_map(state));
  return success();
}

value_t lookup_methodspace_method(value_t ambience, value_t methodspace,
    value_t record, frame_t *frame, value_t *arg_map_out) {
  value_and_argument_map_t data;
  data.value = methodspace;
  data.arg_map_out = arg_map_out;
  return do_signature_map_lookup(ambience, record, frame,
      do_methodspace_method_lookup, &data);
}

value_t plankton_new_methodspace(runtime_t *runtime) {
  return new_heap_methodspace(runtime);
}

value_t plankton_set_methodspace_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, methods, inheritance, imports);
  set_methodspace_methods(object, methods);
  set_methodspace_inheritance(object, inheritance);
  set_methodspace_imports(object, imports);
  return success();
}

void methodspace_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<methodspace ");
  value_t methods = get_methodspace_methods(self);
  value_print_inner_on(methods, context, -1);
  string_buffer_printf(context->buf, " ");
  value_t imports = get_methodspace_imports(self);
  value_print_inner_on(imports, context, -1);
  string_buffer_printf(context->buf, ">");
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

void print_invocation_on(value_t record, frame_t *frame, string_buffer_t *buf) {
  size_t arg_count = get_invocation_record_argument_count(record);
  string_buffer_printf(buf, "{");
  for (size_t i = 0;  i < arg_count; i++) {
    value_t tag = get_invocation_record_tag_at(record, i);
    value_t arg = get_invocation_record_argument_at(record, frame, i);
    if (i > 0)
      string_buffer_printf(buf, ", ");
    string_buffer_printf(buf, "%v: %v", tag, arg);
  }
  string_buffer_printf(buf, "}");
}

void invocation_record_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "{");
  size_t arg_count = get_invocation_record_argument_count(self);
  for (size_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_t tag = get_invocation_record_tag_at(self, i);
    size_t offset = get_invocation_record_offset_at(self, i);
    value_print_inner_on(tag, context, -1);
    string_buffer_printf(context->buf, "@%i", offset);
  }
  string_buffer_printf(context->buf, "}");
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

value_t operation_transient_identity_hash(value_t self, hash_stream_t *stream,
    cycle_detector_t *outer) {
  value_t value = get_operation_value(self);
  operation_type_t type = (operation_type_t) get_operation_type(self);
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, self));
  hash_stream_write_int64(stream, type);
  TRY(value_transient_identity_hash_cycle_protect(value, stream, &inner));
  return success();
}

value_t operation_identity_compare(value_t a, value_t b,
    cycle_detector_t *outer) {
  if (get_operation_type(a) != get_operation_type(b))
    return no();
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  return value_identity_compare_cycle_protect(get_operation_value(a),
      get_operation_value(b), &inner);
}

void operation_print_on(value_t self, print_on_context_t *context) {
  value_t value = get_operation_value(self);
  print_on_context_t unquote_context = *context;
  unquote_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
  switch (get_operation_type(self)) {
    case otAssign:
      // Since the operator for the assignment is kind of sort of part of the
      // operator let's not decrease depth. If you make an assignment whose
      // operator is the assignment itself then 1) this will fail and 2) I hate
      // you.
      value_print_inner_on(value, &unquote_context, 0);
      string_buffer_printf(context->buf, ":=");
      break;
    case otCall:
      string_buffer_printf(context->buf, "()");
      break;
    case otIndex:
      string_buffer_printf(context->buf, "[]");
      break;
    case otInfix:
      string_buffer_printf(context->buf, ".");
      value_print_inner_on(value, &unquote_context, -1);
      string_buffer_printf(context->buf, "()");
      break;
    case otPrefix:
      value_print_inner_on(value, &unquote_context, -1);
      string_buffer_printf(context->buf, "()");
      break;
    case otProperty:
      string_buffer_printf(context->buf, ".");
      value_print_inner_on(value, &unquote_context, -1);
      break;
    case otSuffix:
      string_buffer_printf(context->buf, "()");
      value_print_inner_on(value, &unquote_context, -1);
      break;
    default:
      UNREACHABLE("unexpected operation type");
      break;
  }
}

void operation_print_open_on(value_t self, print_on_context_t *context) {
  value_t value = get_operation_value(self);
  print_on_context_t unquote_context = *context;
  unquote_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
  switch (get_operation_type(self)) {
  case otAssign:
    // Since the operator for the assignment is kind of sort of part of the
    // operator let's not decrease depth. If you make an assignment whose
    // operator is the assignment itself then 1) this will fail and 2) I hate
    // you.
    value_print_inner_on(value, &unquote_context, 0);
    string_buffer_printf(context->buf, ":=(");
    break;
  case otCall:
    string_buffer_printf(context->buf, "(");
    break;
  case otIndex:
    string_buffer_printf(context->buf, "[");
    break;
  case otInfix:
    string_buffer_printf(context->buf, ".");
    value_print_inner_on(value, &unquote_context, -1);
    string_buffer_printf(context->buf, "(");
    break;
  case otPrefix:
    value_print_inner_on(value, &unquote_context, -1);
    string_buffer_printf(context->buf, "(");
    break;
  case otProperty:
    string_buffer_printf(context->buf, ".");
    value_print_inner_on(value, &unquote_context, -1);
    break;
  case otSuffix:
    string_buffer_printf(context->buf, "(");
    break;
  default:
    UNREACHABLE("unexpected operation type");
    break;
  }
}

void operation_print_close_on(value_t self, print_on_context_t *context) {
  value_t value = get_operation_value(self);
  print_on_context_t unquote_context = *context;
  unquote_context.flags = SET_ENUM_FLAG(print_flags_t, context->flags, pfUnquote);
  switch (get_operation_type(self)) {
  case otAssign:
  case otCall:
  case otInfix:
  case otPrefix:
    string_buffer_printf(context->buf, ")");
    break;
  case otIndex:
    string_buffer_printf(context->buf, "]");
    break;
  case otProperty:
    break;
  case otSuffix:
    string_buffer_printf(context->buf, ")");
    value_print_inner_on(value, &unquote_context, -1);
    break;
  default:
    UNREACHABLE("unexpected operation type");
    break;
  }
}

value_t plankton_new_operation(runtime_t *runtime) {
  return new_heap_operation(runtime, afMutable, otCall, nothing());
}

value_t plankton_set_operation_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, type, value);
  set_operation_type(object, get_integer_value(type));
  set_operation_value(object, value);
  return success();
}


// --- B u i l t i n   m a r k e r ---

GET_FAMILY_PRIMARY_TYPE_IMPL(builtin_marker);
NO_BUILTIN_METHODS(builtin_marker);
FIXED_GET_MODE_IMPL(builtin_marker, vmMutable);

ACCESSORS_IMPL(BuiltinMarker, builtin_marker, acNoCheck, 0, Name, name);

value_t builtin_marker_validate(value_t self) {
  VALIDATE_FAMILY(ofBuiltinMarker, self);
  return success();
}

void builtin_marker_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofBuiltinMarker, self);
  string_buffer_printf(context->buf, "#<builtin_marker ");
  value_print_inner_on(get_builtin_marker_name(self), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- B u i l t i n   i m p l e m e n t a t i o n ---

FIXED_GET_MODE_IMPL(builtin_implementation, vmMutable);

ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInFamily,
    ofString, Name, name);
ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInFamily,
    ofCodeBlock, Code, code);
INTEGER_ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation,
    ArgumentCount, argument_count);

value_t builtin_implementation_validate(value_t self) {
  VALIDATE_FAMILY(ofBuiltinImplementation, self);
  VALIDATE_FAMILY(ofString, get_builtin_implementation_name(self));
  VALIDATE_FAMILY(ofCodeBlock, get_builtin_implementation_code(self));
  return success();
}

void builtin_implementation_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofBuiltinImplementation, self);
  string_buffer_printf(context->buf, "#<builtin_implementation ");
  value_print_inner_on(get_builtin_implementation_name(self), context, -1);
  string_buffer_printf(context->buf, ">");
}
