//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "derived-inl.h"
#include "freeze.h"
#include "log.h"
#include "method.h"
#include "ook-inl.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "value-inl.h"

void sigmap_input_init(sigmap_input_o *input, value_t ambience, value_t tags) {
  input->runtime = get_ambience_runtime(ambience);
  input->ambience = ambience;
  input->tags = tags;
  input->argc = is_nothing(tags) ? 0 : get_call_tags_entry_count(tags);
}

size_t sigmap_input_get_argument_count(sigmap_input_o *input) {
  return input->argc;
}

value_t sigmap_input_get_tag_at(sigmap_input_o *input, size_t index) {
  return get_call_tags_tag_at(input->tags, index);
}

size_t sigmap_input_get_offset_at(sigmap_input_o *input, size_t index) {
  return get_call_tags_offset_at(input->tags, index);
}

value_t total_sigmap_input_get_value_at(total_sigmap_input_o *self, size_t index) {
  return METHOD(self, get_value_at)(self, index);
}

// Returns the index'th argument passed in the frame of the given input in
// sorted tag order.
static value_t frame_sigmap_input_get_argument_at(frame_sigmap_input_o *self,
    size_t index) {
  return frame_get_pending_argument_at(self->frame, UPCAST(UPCAST(self))->tags,
      index);
}

// Implementation of sigmap_input.match_value_at which works for
// frame_sigmap_inputs.
static value_t frame_sigmap_input_match_value_at(sigmap_input_o *super_self,
    size_t index, value_t guard, value_t space, value_t *score_out) {
  frame_sigmap_input_o *self = DOWNCAST(frame_sigmap_input_o, super_self);
  value_t value = frame_sigmap_input_get_argument_at(self, index);
  return guard_match(guard, value, super_self, space, score_out);
}

// Returns the value of the index'th argument to a frame_sigmap_input, viewed
// as a total_sigmap_input, in sorted tag order.
static value_t frame_sigmap_input_get_value_at(total_sigmap_input_o *super_self,
    size_t index) {
  frame_sigmap_input_o *self = DOWNCAST(frame_sigmap_input_o, super_self);
  return frame_sigmap_input_get_argument_at(self, index);
}

// The singleton vtable for frame_sigmap_input_os.
VTABLE(frame_sigmap_input_o, total_sigmap_input_o) {
  { frame_sigmap_input_match_value_at },
  frame_sigmap_input_get_value_at
};

frame_sigmap_input_o frame_sigmap_input_new(value_t ambience, value_t tags,
    frame_t *frame) {
  frame_sigmap_input_o result;
  VTABLE_INIT(frame_sigmap_input_o, &result);
  result.frame = frame;
  sigmap_input_init(UPCAST(UPCAST(&result)), ambience, tags);
  return result;
}

// Implementation of sigmap_input.match_value_at which works for
// call_data_sigmap_inputs.
static value_t call_data_sigmap_input_match_value_at(sigmap_input_o *super_self,
    size_t index, value_t guard, value_t space, value_t *score_out) {
  call_data_sigmap_input_o *self = DOWNCAST(call_data_sigmap_input_o, super_self);
  value_t value = get_call_data_value_at(self->call_data, index);
  return guard_match(guard, value, super_self, space, score_out);
}

// Returns the value of the index'th argument to a call_data_sigmap_input, viewed
// as a total_sigmap_input, in sorted tag order.
static value_t call_data_sigmap_input_get_value_at(total_sigmap_input_o *super_self,
    size_t index) {
  call_data_sigmap_input_o *self = DOWNCAST(call_data_sigmap_input_o, super_self);
  return get_call_data_value_at(self->call_data, index);
}

// The singleton vtable for call_data_sigmap_input_os.
VTABLE(call_data_sigmap_input_o, total_sigmap_input_o) {
  { call_data_sigmap_input_match_value_at },
  call_data_sigmap_input_get_value_at
};

call_data_sigmap_input_o call_data_sigmap_input_new(value_t ambience, value_t call_data) {
  call_data_sigmap_input_o result;
  VTABLE_INIT(call_data_sigmap_input_o, &result);
  result.call_data = call_data;
  value_t tags = get_call_data_tags(call_data);
  sigmap_input_init(UPCAST(UPCAST(&result)), ambience, tags);
  return result;
}

// A callback called by do_signature_map_lookup to perform the traversal that
// produces the signature maps to lookup within.
typedef value_t (*sigmap_thunk_call_m)(sigmap_thunk_o *thunk);

struct sigmap_thunk_o_vtable_t {
  sigmap_thunk_call_m call;
};

// A callback-style object that holds the state associated with the lookup
// process. Implementations can add the extra state they need for their
// particular type of lookup.
struct sigmap_thunk_o {
  INTERFACE_HEADER(sigmap_thunk_o);
  // The lookup state. This is set for the caller by the lookup process.
  sigmap_state_t *state;
};


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

value_t match_signature(value_t self, sigmap_input_o *input,
    value_t space, match_info_t *match_info, match_result_t *result_out) {
  // This implementation matches match_signature_tags very closely. Ideally the
  // same implementation could be used for both purposes but the flow is
  // different enough that having two near-identical copies is actually easier
  // to manage. Make sure to keep them in sync.
  CHECK_FAMILY(ofSignature, self);
  CHECK_FAMILY_OPT(ofMethodspace, space);
  TOPIC_INFO(Lookup, "Matching against %5v", self);
  size_t argc = sigmap_input_get_argument_count(input);
  CHECK_REL("score array too short", argc, <=, match_info->capacity);
  // Fast case if fewer than that minimum number of arguments is given.
  size_t mandatory_count = get_signature_mandatory_count(self);
  if (argc < mandatory_count) {
    *result_out = mrMissingArgument;
    return success();
  }
  // Fast case if too many arguments are given.
  size_t param_count = get_signature_parameter_count(self);
  bool allow_extra = get_signature_allow_extra(self);
  if (!allow_extra && (argc > param_count)) {
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
  for (size_t i = 0; i < argc; i++) {
    match_info->scores[i] = new_no_match_score();
    match_info->offsets[i] = kNoOffset;
  }
  // Scan through the arguments and look them up in the signature.
  value_t tags = get_signature_tags(self);
  for (size_t i = 0; i < argc; i++) {
    value_t tag = sigmap_input_get_tag_at(input, i);
    // TODO: propagate any errors caused by this.
    value_t param = binary_search_pair_array(tags, tag);
    if (in_condition_cause(ccNotFound, param)) {
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
    value_t score;
    value_t guard = get_parameter_guard(param);
    TRY(METHOD(input, match_value_at)(input, index, guard, space, &score));
    if (!is_score_match(score)) {
      // The guard says the argument doesn't match. Bail out.
      bit_vector_dispose(&params_seen);
      *result_out = mrGuardRejected;
      return success();
    }
    // We got a match! Record the result and move on to the next.
    bit_vector_set_at(&params_seen, index, true);
    match_info->scores[i] = score;
    match_info->offsets[index] = sigmap_input_get_offset_at(input, i);
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

value_t match_signature_tags(value_t self, value_t call_tags,
    match_result_t *result_out) {
  // This implementation matches match_signature very closely. Ideally the same
  // implementation could be used for both purposes but the flow is different
  // enough that having two near-identical copies is actually easier to manage.
  // Make sure to keep them in sync.
  CHECK_FAMILY(ofSignature, self);
  CHECK_FAMILY(ofCallTags, call_tags);
  TOPIC_INFO(Lookup, "Matching tags against %4v", self);
  size_t argument_count = get_call_tags_entry_count(call_tags);
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
  // Scan through the arguments and look them up in the signature.
  value_t sig_tags = get_signature_tags(self);
  for (size_t i = 0; i < argument_count; i++) {
    value_t tag = get_call_tags_tag_at(call_tags, i);
    // TODO: propagate any errors caused by this.
    value_t param = binary_search_pair_array(sig_tags, tag);
    if (in_condition_cause(ccNotFound, param)) {
      // The tag wasn't found in this signature.
      if (allow_extra) {
        // It's fine, this signature allows extra arguments.
        on_match = mrExtraMatch;
        continue;
      } else {
        // This signature doesn't allow extra arguments so we bail out.
        bit_vector_dispose(&params_seen);
        *result_out = mrUnexpectedArgument;
        return success();
      }
    }
    CHECK_FALSE("binary search failed", get_value_domain(sig_tags) == vdCondition);
    // The tag matched one in this signature.
    size_t index = get_parameter_index(param);
    if (bit_vector_get_at(&params_seen, index)) {
      // We've now seen two tags that match the same parameter. Bail out.
      bit_vector_dispose(&params_seen);
      *result_out = mrRedundantArgument;
      return success();
    }
    // We got a match! Move on to the next.
    bit_vector_set_at(&params_seen, index, true);
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
    if (is_score_better(target[i], source[i])) {
      // The source was strictly worse than the target.
      result = SET_ENUM_FLAG(join_status_t, result, jsWorse);
    } else if (is_score_better(source[i], target[i])) {
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

value_t guard_match(value_t guard, value_t value, sigmap_input_o *lookup_input,
    value_t space, value_t *score_out) {
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


// ## Method

ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofSignature, Signature, signature);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofCodeBlock, Code, code);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofMethodAst, Syntax, syntax);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofModuleFragment, ModuleFragment,
    module_fragment);
ACCESSORS_IMPL(Method, method, acInPhylum, tpFlagSet, Flags, flags);

value_t method_validate(value_t self) {
  VALIDATE_FAMILY(ofMethod, self);
  VALIDATE_FAMILY_OPT(ofSignature, get_method_signature(self));
  VALIDATE_FAMILY_OPT(ofCodeBlock, get_method_code(self));
  VALIDATE_FAMILY_OPT(ofMethodAst, get_method_syntax(self));
  VALIDATE_FAMILY_OPT(ofModuleFragment, get_method_module_fragment(self));
  VALIDATE_PHYLUM(tpFlagSet, get_method_flags(self));
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
        nothing(), fragment, new_flag_set(kFlagSetAllOff)));
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

// The state maintained while doing signature map lookup. Originally called
// signature_map_lookup_state but it, and particularly the derived names, got
// so long that it's now called the less descriptive sigmap_state_t.
struct sigmap_state_t {
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
  // The result collector used to collect results in whatever way is appropriate.
  sigmap_output_o *output;
  // The input data used as the basis of the lookup.
  sigmap_input_o *input;
};

// Swaps around the result and scratch offsets.
static void sigmap_state_swap_offsets(sigmap_state_t *state) {
  size_t *temp = state->result_offsets;
  state->result_offsets = state->scratch_offsets;
  state->scratch_offsets = temp;
}

// The max amount of arguments for which we'll allocate the lookup state on the
// stack.
#define kSmallLookupLimit 8

value_t continue_sigmap_lookup(sigmap_state_t *state, value_t sigmap, value_t space) {
  CHECK_FAMILY(ofSignatureMap, sigmap);
  CHECK_FAMILY(ofMethodspace, space);
  TOPIC_INFO(Lookup, "Looking up in signature map %v", sigmap);
  value_t entries = get_signature_map_entries(sigmap);
  size_t entry_count = get_pair_array_buffer_length(entries);
  value_t scratch_score[kSmallLookupLimit];
  match_info_t match_info;
  match_info_init(&match_info, scratch_score, state->scratch_offsets,
      kSmallLookupLimit);
  size_t argc = sigmap_input_get_argument_count(state->input);
  for (size_t current = 0; current < entry_count; current++) {
    value_t signature = get_pair_array_buffer_first_at(entries, current);
    value_t value = get_pair_array_buffer_second_at(entries, current);
    match_result_t match = __mrNone__;
    TRY(match_signature(signature, state->input, space, &match_info, &match));
    if (!match_result_is_match(match))
      continue;
    join_status_t status = join_score_vectors(state->max_score, scratch_score,
        argc);
    if (status == jsBetter || (state->max_is_synthetic && status == jsEqual)) {
      // This score is either better than the previous best, or it is equal to
      // the max which is itself synthetic and hence better than any of the
      // entries we've seen so far.
      TRY(METHOD(state->output, add_better)(state->output, value));
      // Now the max definitely isn't synthetic.
      state->max_is_synthetic = false;
      // The offsets for the result is now stored in scratch_offsets and we have
      // no more use for the previous result_offsets so we swap them around.
      sigmap_state_swap_offsets(state);
      // And then we have to update the match info with the new scratch offsets.
      match_info_init(&match_info, scratch_score, state->scratch_offsets,
          kSmallLookupLimit);
    } else if (status != jsWorse) {
      // The next score was not strictly worse than the best we've seen so we
      // don't have a unique best.
      TRY(METHOD(state->output, add_ambiguous)(state->output, value));
      // If the result is ambiguous that means the max is now synthetic.
      state->max_is_synthetic = (status == jsAmbiguous);
    }
  }
  return success();
}

// Reset the scores of a lookup state struct. The pointer fields are assumed to
// already have been set, this only resets them.
static void sigmap_state_reset(sigmap_state_t *state) {
  METHOD(state->output, reset)(state->output);
  state->max_is_synthetic = false;
  for (size_t i = 0; i < state->input->argc; i++)
    state->max_score[i] = new_no_match_score();
}

value_t do_sigmap_lookup(sigmap_thunk_o *thunk, sigmap_input_o *input,
    sigmap_output_o *output) {
  // For now we only handle lookups of a certain size. Hopefully by the time
  // this is too small this implementation will be gone anyway.
  size_t argc = sigmap_input_get_argument_count(input);
  CHECK_REL("too many arguments", argc, <=, kSmallLookupLimit);
  // Initialize the lookup state using stack-allocated space.
  value_t max_score[kSmallLookupLimit];
  size_t offsets_one[kSmallLookupLimit];
  size_t offsets_two[kSmallLookupLimit];
  sigmap_state_t state;
  state.input = input;
  state.output = output;
  state.max_score = max_score;
  state.result_offsets = offsets_one;
  state.scratch_offsets = offsets_two;
  sigmap_state_reset(&state);
  thunk->state = &state;
  TRY(METHOD(thunk, call)(thunk));
  return METHOD(state.output, get_result)(state.output);
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

value_t get_sigmap_lookup_argument_map(sigmap_state_t *state) {
  value_t result = METHOD(state->output, get_result)(state->output);
  if (in_domain(vdCondition, result)) {
    return whatever();
  } else {
    size_t argc = sigmap_input_get_argument_count(state->input);
    return build_argument_map(state->input->runtime, argc, state->result_offsets);
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
  if (in_condition_cause(ccNotFound, parents)) {
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
  if (in_condition_cause(ccNotFound, parents)) {
    return ROOT(runtime, empty_array_buffer);
  } else {
    return parents;
  }
}

// Does a full exhaustive lookup through the tags of the invocation for the
// subject of this call. Returns a not found condition if there is no subject.
static value_t get_invocation_subject_no_shortcut(total_sigmap_input_o *input) {
  size_t argc = sigmap_input_get_argument_count(UPCAST(input));
  for (size_t i = 0; i < argc; i++) {
    value_t tag = sigmap_input_get_tag_at(UPCAST(input), i);
    if (is_same_value(tag, ROOT(UPCAST(input)->runtime, subject_key)))
      return total_sigmap_input_get_value_at(input, i);
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
static value_t get_invocation_subject_with_shortcut(total_sigmap_input_o *input) {
  value_t tag_zero = sigmap_input_get_tag_at(UPCAST(input), 0);
  if (is_same_value(tag_zero, ROOT(UPCAST(input)->runtime, subject_key)))
    return total_sigmap_input_get_value_at(input, 0);
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
  value_t current = fragment;
  while (!is_nothing(current)) {
    value_t methodspace = get_module_fragment_methodspace(current);
    TRY(ensure_methodspace_transitive_dependencies(runtime, methodspace, cache));
    current = get_module_fragment_predecessor(current);
  }
  TRY(ensure_frozen(runtime, cache));
  set_module_fragment_methodspaces_cache(fragment, cache);
  return cache;
}

// Performs a method lookup through the given fragment, that is, in the fragment
// itself and any of the siblings before it.
static value_t lookup_through_fragment(sigmap_state_t *state, value_t fragment) {
  CHECK_FAMILY(ofModuleFragment, fragment);
  value_t space = get_module_fragment_methodspace(fragment);
  TRY_DEF(methodspaces, get_or_create_module_fragment_methodspaces_cache(
      state->input->runtime, fragment));
  for (size_t i = 0; i < get_array_buffer_length(methodspaces); i++) {
    value_t methodspace = get_array_buffer_at(methodspaces, i);
    value_t sigmap = get_methodspace_methods(methodspace);
    TRY(continue_sigmap_lookup(state, sigmap, space));
  }
  return success();
}

// Does the same as lookup_through_fragment but faster by using the helper data
// provided by the compiler.
static value_t lookup_through_fragment_with_helper(sigmap_state_t *state,
    value_t fragment, value_t helper) {
  CHECK_FAMILY(ofModuleFragment, fragment);
  CHECK_FAMILY(ofSignatureMap, helper);
  value_t space = get_module_fragment_methodspace(fragment);
  TRY(continue_sigmap_lookup(state, helper, space));
  return success();
}

IMPLEMENTATION(full_thunk_o, sigmap_thunk_o);

// Thunk that carries data for a full method lookup.
struct full_thunk_o {
  IMPLEMENTATION_HEADER(full_thunk_o, sigmap_thunk_o);
  // The input viewed as a total_sigmap_input_o.
  total_sigmap_input_o *input;
  // The fragment we're performing the lookup within.
  value_t fragment;
  // The helper data for this call.
  value_t helper;
  // The resulting arg map.
  value_t *arg_map_out;
};

// Perform a method lookup in the subject's module of origin.
static value_t lookup_subject_methods(full_thunk_o *thunk, value_t *subject_out) {
  // Look for a subject value, if there is none there is nothing to do.
  sigmap_state_t *state = UPCAST(thunk)->state;
  total_sigmap_input_o *input = thunk->input;
  value_t subject = *subject_out = get_invocation_subject_with_shortcut(input);
  TOPIC_INFO(Lookup, "Subject value: %9v", subject);
  if (in_condition_cause(ccNotFound, subject)) {
    // Just in case, check that the shortcut version gave the correct answer.
    // The case where it returns a non-condition is trivially correct (FLW) so this
    // is the only case there can be any doubt about.
    IF_EXPENSIVE_CHECKS_ENABLED(CHECK_TRUE("Subject shortcut didn't work",
        in_condition_cause(ccNotFound, get_invocation_subject_no_shortcut(input))));
    return success();
  }
  // Extract the origin of the subject.
  value_t type = get_primary_type(subject, UPCAST(input)->runtime);
  TOPIC_INFO(Lookup, "Subject type: %9v", type);
  CHECK_FAMILY(ofType, type);
  value_t origin = get_type_origin(type, UPCAST(input)->ambience);
  TOPIC_INFO(Lookup, "Subject origin: %9v", origin);
  if (is_nothing(origin))
    // Some types have no origin (at least at the moment) and that's okay, we
    // just don't perform the extra lookup.
    return success();
  TRY(lookup_through_fragment(state, origin));
  return success();
}

IMPLEMENTATION(unique_best_match_output_o, sigmap_output_o);

// Sigmap output that keeps the best result seen so far and returns an ambiguity
// signal if there's no unique best match.
struct unique_best_match_output_o {
  IMPLEMENTATION_HEADER(unique_best_match_output_o, sigmap_output_o);
  value_t result;
};

static value_t unique_best_match_output_add_ambiguous(sigmap_output_o *super_self,
    value_t value) {
  unique_best_match_output_o *self = DOWNCAST(unique_best_match_output_o, super_self);
  if (!is_same_value(value, self->result))
    // If we hit the exact same entry more than once, which can happen if
    // the same signature map is traversed more than once, that's okay we just
    // skip. Otherwise we've found a genuine ambiguity.
    self->result = new_lookup_error_condition(lcAmbiguity);
  return success();
}

static value_t unique_best_match_output_add_better(sigmap_output_o *super_self,
    value_t value) {
  unique_best_match_output_o *self = DOWNCAST(unique_best_match_output_o, super_self);
  self->result = value;
  return success();
}

static value_t unique_best_match_output_get_result(sigmap_output_o *super_self) {
  unique_best_match_output_o *self = DOWNCAST(unique_best_match_output_o, super_self);
  return self->result;
}

static void unique_best_match_output_reset(sigmap_output_o *super_self) {
  unique_best_match_output_o *self = DOWNCAST(unique_best_match_output_o, super_self);
  self->result = new_lookup_error_condition(lcNoMatch);
}

VTABLE(unique_best_match_output_o, sigmap_output_o) {
  unique_best_match_output_add_ambiguous,
  unique_best_match_output_add_better,
  unique_best_match_output_get_result,
  unique_best_match_output_reset
};

static unique_best_match_output_o unique_best_match_output_new() {
  unique_best_match_output_o result;
  VTABLE_INIT(unique_best_match_output_o, UPCAST(&result));
  unique_best_match_output_reset(UPCAST(&result));
  return result;
}

// Do a transitive method lookup in the given method space, that is, look up
// locally and in any imported spaces.
static value_t lookup_methodspace_transitive_method(sigmap_state_t *state,
    value_t space) {
  value_t local_methods = get_methodspace_methods(space);
  TRY(continue_sigmap_lookup(state, local_methods, space));
  value_t imports = get_methodspace_imports(space);
  for (size_t i = 0; i < get_array_buffer_length(imports); i++) {
    value_t import = get_array_buffer_at(imports, i);
    TRY(lookup_methodspace_transitive_method(state, import));
  }
  return success();
}

IMPLEMENTATION(methodspace_thunk_o, sigmap_thunk_o);

// State used when looking up within a single methodspace.
struct methodspace_thunk_o {
  IMPLEMENTATION_HEADER(methodspace_thunk_o, sigmap_thunk_o);
  // The methodspace to look up within.
  value_t methodspace;
  // The location to store the resulting arg map.
  value_t *arg_map_out;
};

static methodspace_thunk_o methodspace_thunk_new(value_t methodspace,
    value_t *arg_map_out) {
  methodspace_thunk_o result;
  VTABLE_INIT(methodspace_thunk_o, UPCAST(&result));
  result.methodspace = methodspace;
  result.arg_map_out = arg_map_out;
  return result;
}

// Performs a method lookup within a single methodspace.
static value_t methodspace_thunk_call(sigmap_thunk_o *super_self) {
  methodspace_thunk_o *self = DOWNCAST(methodspace_thunk_o, super_self);
  CHECK_FAMILY(ofMethodspace, self->methodspace);
  sigmap_state_t *state = UPCAST(self)->state;
  TRY(lookup_methodspace_transitive_method(state, self->methodspace));
  TRY_SET(*self->arg_map_out, get_sigmap_lookup_argument_map(state));
  return success();
}

VTABLE(methodspace_thunk_o, sigmap_thunk_o) { methodspace_thunk_call };

value_t lookup_methodspace_method(sigmap_input_o *input, value_t methodspace,
    value_t *arg_map_out) {
  methodspace_thunk_o thunk = methodspace_thunk_new(methodspace, arg_map_out);
  unique_best_match_output_o output = unique_best_match_output_new();
  return do_sigmap_lookup(UPCAST(&thunk), input, UPCAST(&output));
}

IMPLEMENTATION(signal_handler_output_o, sigmap_output_o);

// Lookup match collector that keeps the most specific signal handler as well as
// the handler it originates from.
struct signal_handler_output_o {
  IMPLEMENTATION_HEADER(signal_handler_output_o, sigmap_output_o);
  // The current best result.
  value_t result;
  // The handler of the current best result.
  value_t result_handler;
  // The current handler being looked through.
  value_t current_handler;
};

static value_t signal_handler_output_add_ambiguous(sigmap_output_o *super_self,
    value_t value) {
  // We're only interested in the first best match, subsequent as-good matches
  // are ignored as less relevant due to them being further down the stack.
  return success();
}

static value_t signal_handler_output_add_better(sigmap_output_o *super_self,
    value_t value) {
  signal_handler_output_o *self = DOWNCAST(signal_handler_output_o, super_self);
  self->result = value;
  self->result_handler = self->current_handler;
  return success();
}

static value_t signal_handler_output_get_result(sigmap_output_o *super_self) {
  signal_handler_output_o *self = DOWNCAST(signal_handler_output_o, super_self);
  return self->result;
}

static void signal_handler_output_reset(sigmap_output_o *super_self) {
  signal_handler_output_o *self = DOWNCAST(signal_handler_output_o, super_self);
  self->result = new_lookup_error_condition(lcNoMatch);
  self->current_handler = self->result_handler = nothing();
}

VTABLE(signal_handler_output_o, sigmap_output_o) {
  signal_handler_output_add_ambiguous,
  signal_handler_output_add_better,
  signal_handler_output_get_result,
  signal_handler_output_reset
};

// Creates a new empty signal handler output.
static signal_handler_output_o signal_handler_output_new() {
  signal_handler_output_o result;
  VTABLE_INIT(signal_handler_output_o, UPCAST(&result));
  signal_handler_output_reset(UPCAST(&result));
  return result;
}

IMPLEMENTATION(signal_handler_thunk_o, sigmap_thunk_o);

// State associated with looking up a signal handler.
struct signal_handler_thunk_o {
  IMPLEMENTATION_HEADER(signal_handler_thunk_o, sigmap_thunk_o);
  // The handler that holds the resulting method.
  value_t *handler_out;
  // The resulting arg map.
  value_t *arg_map_out;
  // The frame that holds the arguments to the handler.
  frame_t *frame;
};

// Performs a lookup through the signal handlers on the stack.
static value_t signal_handler_thunk_call(sigmap_thunk_o *super_self) {
  signal_handler_thunk_o *self = DOWNCAST(signal_handler_thunk_o, super_self);
  barrier_iter_t barrier_iter;
  sigmap_state_t *state = UPCAST(self)->state;
  signal_handler_output_o *output = DOWNCAST(signal_handler_output_o, state->output);
  value_t barrier = barrier_iter_init(&barrier_iter, self->frame);
  while (!is_nothing(barrier)) {
    if (in_genus(dgSignalHandlerSection, barrier)) {
      output->current_handler = barrier;
      value_t methods = get_barrier_state_payload(barrier);
      CHECK_FAMILY(ofMethodspace, methods);
      value_t sigmap = get_methodspace_methods(methods);
      TRY(continue_sigmap_lookup(state, sigmap, methods));
    }
    barrier = barrier_iter_advance(&barrier_iter);
  }
  TRY_SET(*self->arg_map_out, get_sigmap_lookup_argument_map(state));
  *self->handler_out = output->result_handler;
  return success();
}

VTABLE(signal_handler_thunk_o, sigmap_thunk_o) { signal_handler_thunk_call };

value_t lookup_signal_handler_method(sigmap_input_o *input,
    frame_t *frame, value_t *handler_out, value_t *arg_map_out) {
  signal_handler_thunk_o thunk;
  VTABLE_INIT(signal_handler_thunk_o, UPCAST(&thunk));
  thunk.handler_out = handler_out;
  thunk.arg_map_out = arg_map_out;
  thunk.frame = frame;
  signal_handler_output_o output = signal_handler_output_new();
  return do_sigmap_lookup(UPCAST(&thunk), input, UPCAST(&output));
}

// Performs the extra lookup for lambda methods that happens when the lambda
// delegate method is found in the normal lookup.
static value_t complete_special_lambda_lookup(full_thunk_o *thunk,
    value_t subject) {
  CHECK_FAMILY(ofLambda, subject);
  methodspace_thunk_o subthunk = methodspace_thunk_new(
      get_lambda_methods(subject), thunk->arg_map_out);
  sigmap_state_t *state = UPCAST(thunk)->state;
  sigmap_state_reset(state);
  UPCAST(&subthunk)->state = state;
  return METHOD(UPCAST(&subthunk), call)(UPCAST(&subthunk));
}

// Performs the extra lookup for block methods that happens when the block
// delegate method is found in the normal lookup.
static value_t complete_special_block_lookup(full_thunk_o *thunk,
    value_t subject) {
  CHECK_FAMILY(ofBlock, subject);
  value_t section = get_block_section(subject);
  methodspace_thunk_o subthunk = methodspace_thunk_new(
      get_block_section_methodspace(section), thunk->arg_map_out);
  sigmap_state_t *state = UPCAST(thunk)->state;
  sigmap_state_reset(state);
  UPCAST(&subthunk)->state = state;
  return METHOD(UPCAST(&subthunk), call)(UPCAST(&subthunk));
}

// Performs a full method lookup through the current module and the subject's
// module of origin.
static value_t full_thunk_call(sigmap_thunk_o *super_self) {
  full_thunk_o *self = DOWNCAST(full_thunk_o, super_self);
  CHECK_FAMILY(ofModuleFragment, self->fragment);
  sigmap_state_t *state = UPCAST(self)->state;
  TOPIC_INFO(Lookup, "Performing fragment lookup %v", state->input->tags);
  if (is_nothing(self->helper)) {
    TRY(lookup_through_fragment(state, self->fragment));
  } else {
    TRY(lookup_through_fragment_with_helper(state, self->fragment, self->helper));
  }
  TOPIC_INFO(Lookup, "Performing subject lookup");
  value_t subject = whatever();
  TRY(lookup_subject_methods(self, &subject));
  value_t result = METHOD(state->output, get_result)(state->output);
  TOPIC_INFO(Lookup, "Lookup result: %v", result);
  if (in_family(ofMethod, result)) {
    value_t result_flags = get_method_flags(result);
    if (!is_flag_set_empty(result_flags)) {
      // The result has at least one special flag set so we have to give this
      // lookup special treatment.
      if (get_flag_set_at(result_flags, mfLambdaDelegate)) {
        return complete_special_lambda_lookup(self, subject);
      } else if (get_flag_set_at(result_flags, mfBlockDelegate)) {
        return complete_special_block_lookup(self, subject);
      }
    }
  }
  TRY_SET(*self->arg_map_out, get_sigmap_lookup_argument_map(state));
  return success();
}

VTABLE(full_thunk_o, sigmap_thunk_o) { full_thunk_call };

value_t lookup_method_full_with_helper(total_sigmap_input_o *input,
    value_t fragment, value_t helper, value_t *arg_map_out) {
  unique_best_match_output_o output = unique_best_match_output_new();
  full_thunk_o thunk;
  VTABLE_INIT(full_thunk_o, UPCAST(&thunk));
  thunk.input = input;
  thunk.fragment = fragment;
  thunk.helper = helper;
  thunk.arg_map_out = arg_map_out;
  return do_sigmap_lookup(UPCAST(&thunk), UPCAST(input), UPCAST(&output));
}

value_t plankton_new_methodspace(runtime_t *runtime) {
  return new_heap_methodspace(runtime);
}

value_t plankton_set_methodspace_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, methods, inheritance, imports);
  set_methodspace_methods(object, methods_value);
  set_methodspace_inheritance(object, inheritance_value);
  set_methodspace_imports(object, imports_value);
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


/// ## Call tags

ACCESSORS_IMPL(CallTags, call_tags, acInFamily, ofArray, Entries, entries);

value_t call_tags_validate(value_t self) {
  VALIDATE_FAMILY(ofCallTags, self);
  VALIDATE_FAMILY(ofArray, get_call_tags_entries(self));
  return success();
}

value_t get_call_tags_tag_at(value_t self, size_t index) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_pair_array_first_at(entries, index);
}

size_t get_call_tags_offset_at(value_t self, size_t index) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_integer_value(get_pair_array_second_at(entries, index));
}

size_t get_call_tags_entry_count(value_t self) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_pair_array_length(entries);
}

value_t build_call_tags_entries(runtime_t *runtime, value_t tags) {
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
  TRY(co_sort_pair_array(result));
  return result;
}

void print_invocation_on(value_t tags, frame_t *frame, string_buffer_t *buf) {
  size_t arg_count = get_call_tags_entry_count(tags);
  string_buffer_printf(buf, "{");
  for (size_t i = 0;  i < arg_count; i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    value_t arg = frame_get_pending_argument_at(frame, tags, i);
    if (i > 0)
      string_buffer_printf(buf, ", ");
    string_buffer_printf(buf, "%v: %v", tag, arg);
  }
  string_buffer_printf(buf, "}");
}

void call_tags_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "{");
  size_t arg_count = get_call_tags_entry_count(self);
  for (size_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_t tag = get_call_tags_tag_at(self, i);
    size_t offset = get_call_tags_offset_at(self, i);
    value_print_inner_on(tag, context, -1);
    string_buffer_printf(context->buf, "@%i", offset);
  }
  string_buffer_printf(context->buf, "}");
}

value_t ensure_call_tags_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_call_tags_entries(self)));
  return success();
}

value_t call_tags_transient_identity_hash(value_t value, hash_stream_t *stream,
    cycle_detector_t *outer) {
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, value));
  value_t entries = get_call_tags_entries(value);
  return value_transient_identity_hash_cycle_protect(entries, stream, &inner);
}

value_t call_tags_identity_compare(value_t a, value_t b, cycle_detector_t *outer) {
  cycle_detector_t inner;
  TRY(cycle_detector_enter(outer, &inner, a));
  value_t a_entries = get_call_tags_entries(a);
  value_t b_entries = get_call_tags_entries(b);
  return value_identity_compare_cycle_protect(a_entries, b_entries, &inner);
}


/// ## Call data

GET_FAMILY_PRIMARY_TYPE_IMPL(call_data);
TRIVIAL_PRINT_ON_IMPL(CallData, call_data);

ACCESSORS_IMPL(CallData, call_data, acInFamily, ofCallTags, Tags, tags);
ACCESSORS_IMPL(CallData, call_data, acInFamily, ofArray, Values, values);

value_t call_data_validate(value_t self) {
  VALIDATE_FAMILY(ofCallData, self);
  VALIDATE_FAMILY(ofCallTags, get_call_data_tags(self));
  VALIDATE_FAMILY(ofArray, get_call_data_values(self));
  return success();
}

value_t get_call_data_value_at(value_t self, size_t param_index) {
  value_t tags = get_call_data_tags(self);
  size_t offset = get_call_tags_offset_at(tags, param_index);
  value_t values = get_call_data_values(self);
  return get_array_at(values, offset);
}

static value_t call_data_length(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofCallData, self);
  value_t values = get_call_data_values(self);
  return new_integer(get_array_length(values));
}

static value_t call_data_get(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofCallData, self);
  value_t needle = get_builtin_argument(args, 0);
  value_t tags = get_call_data_tags(self);
  for (size_t i = 0; i < get_call_tags_entry_count(tags); i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    if (value_identity_compare(needle, tag))
      return get_call_data_value_at(self, i);
  }
  ESCAPE_BUILTIN(args, no_such_tag, needle);
}

value_t add_call_data_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  ADD_BUILTIN_IMPL("call_data.length", 0, call_data_length);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("call_data[]", 1, 1, call_data_get);
  return success();
}


// --- O p e r a t i o n ---

GET_FAMILY_PRIMARY_TYPE_IMPL(operation);
NO_BUILTIN_METHODS(operation);

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
  set_operation_type(object, get_integer_value(type_value));
  set_operation_value(object, value_value);
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


/// ## Builtin implementation

FIXED_GET_MODE_IMPL(builtin_implementation, vmMutable);

ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInFamily,
    ofString, Name, name);
ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInFamily,
    ofCodeBlock, Code, code);
INTEGER_ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation,
    ArgumentCount, argument_count);
ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInPhylum,
    tpFlagSet, MethodFlags, method_flags);

value_t builtin_implementation_validate(value_t self) {
  VALIDATE_FAMILY(ofBuiltinImplementation, self);
  VALIDATE_FAMILY(ofString, get_builtin_implementation_name(self));
  VALIDATE_FAMILY(ofCodeBlock, get_builtin_implementation_code(self));
  VALIDATE_PHYLUM(tpFlagSet, get_builtin_implementation_method_flags(self));
  return success();
}

void builtin_implementation_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofBuiltinImplementation, self);
  string_buffer_printf(context->buf, "#<builtin_implementation ");
  value_print_inner_on(get_builtin_implementation_name(self), context, -1);
  string_buffer_printf(context->buf, ">");
}
