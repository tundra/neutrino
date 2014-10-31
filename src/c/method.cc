//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "stdc.h"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "behavior.h"
#include "codegen.h"
#include "derived-inl.h"
#include "freeze.h"
#include "method.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "utils/log.h"
#include "utils/ook-inl.h"
#include "value-inl.h"
END_C_INCLUDES

/// ## Generic functions
///
/// The method lookup functions are used in a few different ways, with different
/// inputs or outputs, but the basic algorithm is always the same. To avoid
/// repetition we use templates which are specialized below.

template <typename I>
value_t generic_match_signature(value_t self, I *input, value_t space,
    match_info_t *match_info, match_result_t *result_out) {
  CHECK_FAMILY(ofSignature, self);
  CHECK_FAMILY_OPT(ofMethodspace, space);
  TOPIC_INFO(Lookup, "Matching against %5v", self);
  size_t argc = input->get_argument_count();
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
    value_t tag = input->get_tag_at(i);
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
    TRY(input->match_value_at(index, guard, space, &score));
    if (!is_score_match(score)) {
      // The guard says the argument doesn't match. Bail out.
      bit_vector_dispose(&params_seen);
      *result_out = mrGuardRejected;
      return success();
    }
    // We got a match! Record the result and move on to the next.
    bit_vector_set_at(&params_seen, index, true);
    match_info->scores[i] = score;
    match_info->offsets[index] = input->get_offset_at(i);
    if (!get_parameter_is_optional(param))
      mandatory_seen_count++;
  }
  bit_vector_dispose(&params_seen);
  if (mandatory_seen_count < mandatory_count) {
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

// The state maintained while doing signature map lookup. Originally called
// signature_map_lookup_state but it, and particularly the derived names, got
// so long that it's now called the less descriptive sigmap_state_t.
template <class I, class O>
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
  O *output;
  // The input data used as the basis of the lookup.
  I *input;
};

// Swaps around the result and scratch offsets.
template <class I, class O>
static void sigmap_state_swap_offsets(sigmap_state_t<I, O> *state) {
  size_t *temp = state->result_offsets;
  state->result_offsets = state->scratch_offsets;
  state->scratch_offsets = temp;
}

// Reset the scores of a lookup state struct. The pointer fields are assumed to
// already have been set, this only resets them.
template <class I, class O>
static void sigmap_state_reset(sigmap_state_t<I, O> *state) {
  state->output->reset();
  state->max_is_synthetic = false;
  for (size_t i = 0; i < state->input->get_argument_count(); i++)
    state->max_score[i] = new_no_match_score();
}

// The max amount of arguments for which we'll allocate the lookup state on the
// stack.
#define kSmallLookupLimit 8

// Prepares a signature map lookup and then calls the callback which must
// traverse the signature maps to include in the lookup and invoke
// continue_signature_map_lookup for each of them. When the callback returns
// this function completes the lookup and returns the result or a condition as
// appropriate.
template <class T, class I, class O>
static value_t generic_lookup_method(T *thunk, I *input, O *output) {
  // For now we only handle lookups of a certain size. Hopefully by the time
  // this is too small this implementation will be gone anyway.
  size_t argc = input->get_argument_count();
  CHECK_REL("too many arguments", argc, <=, kSmallLookupLimit);
  // Initialize the lookup state using stack-allocated space.
  value_t max_score[kSmallLookupLimit];
  size_t offsets_one[kSmallLookupLimit];
  size_t offsets_two[kSmallLookupLimit];
  sigmap_state_t<I, O> state;
  state.input = input;
  state.output = output;
  state.max_score = max_score;
  state.result_offsets = offsets_one;
  state.scratch_offsets = offsets_two;
  sigmap_state_reset(&state);
  TRY(thunk->call(&state));
  return output->get_result();
}

// Includes the given signature map in the lookup associated with the given
// lookup state.
template <class I, class O>
value_t continue_sigmap_lookup(sigmap_state_t<I, O> *state, value_t sigmap, value_t space) {
  CHECK_FAMILY(ofSignatureMap, sigmap);
  CHECK_FAMILY(ofMethodspace, space);
  TOPIC_INFO(Lookup, "Looking up in signature map %v", sigmap);
  value_t entries = get_signature_map_entries(sigmap);
  size_t entry_count = get_pair_array_buffer_length(entries);
  value_t scratch_score[kSmallLookupLimit];
  match_info_t match_info;
  match_info_init(&match_info, scratch_score, state->scratch_offsets,
      kSmallLookupLimit);
  I *input = state->input;
  O *output = state->output;
  size_t argc = input->get_argument_count();
  for (size_t current = 0; current < entry_count; current++) {
    value_t signature = get_pair_array_buffer_first_at(entries, current);
    value_t value = get_pair_array_buffer_second_at(entries, current);
    match_result_t match = __mrNone__;
    TRY(generic_match_signature(signature, input, space, &match_info, &match));
    if (!match_result_is_match(match))
      continue;
    join_status_t status = join_score_vectors(state->max_score, scratch_score,
        argc);
    if (status == jsBetter || (state->max_is_synthetic && status == jsEqual)) {
      // This score is either better than the previous best, or it is equal to
      // the max which is itself synthetic and hence better than any of the
      // entries we've seen so far.
      TRY(output->add_better(value));
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
      TRY(output->add_ambiguous(value));
      // If the result is ambiguous that means the max is now synthetic.
      state->max_is_synthetic = (status == jsAmbiguous);
    }
  }
  return success();
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

// Returns the argument map that describes the location of the arguments of the
// signature map lookup match recorded in the given lookup state. If there is
// no match recorded an arbitrary non-condition value will be returned.
template <class I, class O>
value_t get_sigmap_lookup_argument_map(sigmap_state_t<I, O> *state) {
  value_t result = state->output->get_result();
  if (in_domain(vdCondition, result)) {
    return whatever();
  } else {
    size_t argc = state->input->get_argument_count();
    return build_argument_map(state->input->get_runtime(), argc, state->result_offsets);
  }
}


// Does a full exhaustive lookup through the tags of the invocation for the
// subject of this call. Returns a not found condition if there is no subject.
template <class I>
static value_t get_invocation_subject_no_shortcut(I *input) {
  size_t argc = input->get_argument_count();
  for (size_t i = 0; i < argc; i++) {
    value_t tag = input->get_tag_at(i);
    if (is_same_value(tag, ROOT(input->get_runtime(), subject_key)))
      return input->get_value_at(i);
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
template <class I>
static value_t get_invocation_subject_with_shortcut(I *input) {
  value_t tag_zero = input->get_tag_at(0);
  if (is_same_value(tag_zero, ROOT(input->get_runtime(), subject_key)))
    return input->get_value_at(0);
  else
    return new_not_found_condition();
}

// Performs a method lookup through the given fragment, that is, in the fragment
// itself and any of the siblings before it.
template <class I, class O>
static value_t lookup_through_input(sigmap_state_t<I, O> *state) {
  value_t space = get_ambience_methodspace(state->input->get_ambience());
  // CHECK_DEEP_FROZEN(space);
  while (!is_nothing(space)) {
    value_t sigmap = get_methodspace_methods(space);
    TRY(continue_sigmap_lookup(state, sigmap, space));
    space = get_methodspace_parent(space);
  }
  return success();
}

// Returns the invocation subject.
template <class I>
static value_t get_invocation_subject(I *input) {
  // Look for a subject value, if there is none there is nothing to do.
  value_t subject = get_invocation_subject_with_shortcut(input);
  TOPIC_INFO(Lookup, "Subject value: %9v", subject);
  if (in_condition_cause(ccNotFound, subject)) {
    // Just in case, check that the shortcut version gave the correct answer.
    // The case where it returns a non-condition is trivially correct (FLW) so this
    // is the only case there can be any doubt about.
    IF_EXPENSIVE_CHECKS_ENABLED(CHECK_TRUE("Subject shortcut didn't work",
        in_condition_cause(ccNotFound, get_invocation_subject_no_shortcut(input))));
  }
  return subject;
}

/// ## Inputs

// Convenience supertype for the inputs.
class AbstractSigmapInput {
public:
  AbstractSigmapInput(sigmap_input_layout_t *layout);
  size_t get_argument_count();
  value_t get_tag_at(size_t index);
  size_t get_offset_at(size_t index);
  runtime_t *get_runtime();
  value_t get_ambience();
  value_t get_tags();
private:
  value_t ambience_;
  value_t tags_;
  size_t argc_;
  runtime_t *runtime_;
};

AbstractSigmapInput::AbstractSigmapInput(sigmap_input_layout_t *layout) {
  ambience_ = layout->ambience;
  tags_ = layout->tags;
  argc_ = is_nothing(tags_) ? 0 : get_call_tags_entry_count(tags_);
  runtime_ = get_ambience_runtime(layout->ambience);
}

size_t AbstractSigmapInput::get_argument_count() {
  return argc_;
}

value_t AbstractSigmapInput::get_tag_at(size_t index) {
  return get_call_tags_tag_at(tags_, index);
}

size_t AbstractSigmapInput::get_offset_at(size_t index) {
  return get_call_tags_offset_at(tags_, index);
}

runtime_t *AbstractSigmapInput::get_runtime() {
  return runtime_;
}

value_t AbstractSigmapInput::get_ambience() {
  return ambience_;
}

value_t AbstractSigmapInput::get_tags() {
  return tags_;
}

// Lookup input that gets values from a frame.
class FrameSigmapInput : public AbstractSigmapInput {
public:
  FrameSigmapInput(sigmap_input_layout_t *layout, frame_t *frame);
  value_t get_value_at(size_t index);
  value_t match_value_at(size_t index, value_t guard, value_t space, value_t *score_out);
private:
  frame_t *frame_;
};

FrameSigmapInput::FrameSigmapInput(sigmap_input_layout_t *layout, frame_t *frame)
  : AbstractSigmapInput(layout)
  , frame_(frame) { }

value_t FrameSigmapInput::get_value_at(size_t index) {
  return frame_get_pending_argument_at(frame_, get_tags(), index);
}

value_t FrameSigmapInput::match_value_at(size_t index, value_t guard,
    value_t space, value_t *score_out) {
  value_t value = get_value_at(index);
  return guard_match(guard, value, get_runtime(), space, score_out);
}

// Frame input that takes next-guards into account.
class FrameSigmapInputWithNexts : public FrameSigmapInput {
public:
  FrameSigmapInputWithNexts(sigmap_input_layout_t *layout, frame_t *frame);
  value_t match_value_at(size_t index, value_t guard, value_t space, value_t *score_out);
private:
  value_t next_guards_;
};

FrameSigmapInputWithNexts::FrameSigmapInputWithNexts(sigmap_input_layout_t *layout,
    frame_t *frame)
  : FrameSigmapInput(layout, frame)
  , next_guards_(layout->next_guards) {
  CHECK_FALSE("next frame input without next guards", is_nothing(layout->next_guards));
}

value_t FrameSigmapInputWithNexts::match_value_at(size_t index, value_t guard,
    value_t space, value_t *score_out) {
  value_t value = get_value_at(index);
  value_t score = whatever();
  TRY(guard_match(guard, value, get_runtime(), space, &score));
  value_t next_guard = get_array_at(next_guards_, index);
  if (is_nothing(next_guard)) {
    *score_out = score;
    return success();
  }
  value_t next_score = whatever();
  TRY(guard_match(next_guard, value, get_runtime(), space, &next_score));
  if (is_score_better(next_score, score)) {
    *score_out = score;
    return success();
  } else {
    *score_out = new_no_match_score();
    return success();
  }
}

// Lookup input that gets values from a call data object.
class CallDataSigmapInput : public AbstractSigmapInput {
public:
  CallDataSigmapInput(sigmap_input_layout_t *layout, value_t call_data);
  value_t get_value_at(size_t index);
  value_t match_value_at(size_t index, value_t guard, value_t space, value_t *score_out);
private:
  value_t call_data_;
};

CallDataSigmapInput::CallDataSigmapInput(sigmap_input_layout_t *layout, value_t call_data)
  : AbstractSigmapInput(layout)
  , call_data_(call_data) { }

value_t CallDataSigmapInput::get_value_at(size_t index) {
  return get_call_data_value_at(call_data_, index);
}

value_t CallDataSigmapInput::match_value_at(size_t index, value_t guard,
    value_t space, value_t *score_out) {
  value_t value = get_call_data_value_at(call_data_, index);
  return guard_match(guard, value, get_runtime(), space, score_out);
}


/// ## Outputs

// An output that picks the unique best match.
class UniqueBestMatchOutput {
public:
  UniqueBestMatchOutput();
  void reset();
  value_t get_result() { return result_; }
  value_t add_better(value_t value);
  value_t add_ambiguous(value_t value);
private:
  value_t result_;
};

UniqueBestMatchOutput::UniqueBestMatchOutput() {
  reset();
}

void UniqueBestMatchOutput::reset() {
  result_ = new_lookup_error_condition(lcNoMatch);
}

value_t UniqueBestMatchOutput::add_better(value_t value) {
  result_ = value;
  return success();
}

value_t UniqueBestMatchOutput::add_ambiguous(value_t value) {
  if (!is_same_value(value, result_))
    // If we hit the exact same entry more than once, which can happen if
    // the same signature map is traversed more than once, that's okay we just
    // skip. Otherwise we've found a genuine ambiguity.
    result_ = new_lookup_error_condition(lcAmbiguity);
  return success();
}

// An output handler that picks the first best result that matches the input
// and records the handler that corresponds to that result.
class SignalHandlerOutput {
public:
  SignalHandlerOutput();
  void set_current_handler(value_t value);
  value_t get_result_handler();
  value_t get_result() { return result_; }
  value_t add_better(value_t value);
  value_t add_ambiguous(value_t value);
  void reset();
private:
  // The current best result.
  value_t result_;
  // The handler of the current best result.
  value_t result_handler_;
  // The current handler being looked through. We need this such that when a
  // new better match is found we can record which handler is belongs to.
  value_t current_handler_;
};

SignalHandlerOutput::SignalHandlerOutput() {
  reset();
}

void SignalHandlerOutput::set_current_handler(value_t value) {
  current_handler_ = value;
}

value_t SignalHandlerOutput::get_result_handler() {
  return result_handler_;
}

value_t SignalHandlerOutput::add_better(value_t value) {
  result_ = value;
  result_handler_ = current_handler_;
  return success();
}
value_t SignalHandlerOutput::add_ambiguous(value_t value) {
  // We're only interested in the first best match, subsequent as-good matches
  // are ignored as less relevant due to them being further down the stack.
  return success();
}

void SignalHandlerOutput::reset() {
  result_ = new_lookup_error_condition(lcNoMatch);
  current_handler_ = result_handler_ = nothing();
}

/// ## Thunks
///
/// The code and state that coordinates how lookup proceeds is packed into a
/// thunk that gets called with the initialized state by the framework.

// Thunk that looks up within a particular methodspace.
template <class I, class O>
class MethodspaceThunk {
public:
  MethodspaceThunk(value_t methodspace, value_t *arg_map_out);
  value_t call(sigmap_state_t<I, O> *state);
private:
  value_t methodspace_;
  value_t *arg_map_out_;
};

template <class I, class O>
MethodspaceThunk<I, O>::MethodspaceThunk(value_t methodspace, value_t *arg_map_out)
  : methodspace_(methodspace)
  , arg_map_out_(arg_map_out) {
  CHECK_FAMILY(ofMethodspace, methodspace_);
}

template <class I, class O>
value_t MethodspaceThunk<I, O>::call(sigmap_state_t<I, O> *state) {
  value_t space = methodspace_;
  TRY(continue_sigmap_lookup(state, get_methodspace_methods(space), space));
  TRY_SET(*arg_map_out_, get_sigmap_lookup_argument_map(state));
  return success();
}

// Thunk that performs signal handler lookup down through the stack. The output
// will always be a SignalHandlerOutput.
template <class I>
class SignalHandlerThunk {
public:
  SignalHandlerThunk(value_t *handler_out, value_t *arg_map_out, frame_t *frame);
  value_t call(sigmap_state_t<I, SignalHandlerOutput> *state);
private:
  // The handler that holds the resulting method.
  value_t *handler_out_;
  // The resulting arg map.
  value_t *arg_map_out_;
  // The frame that holds the arguments to the handler.
  frame_t *frame_;
};

template <class I>
SignalHandlerThunk<I>::SignalHandlerThunk(value_t *handler_out, value_t *arg_map_out, frame_t *frame)
  : handler_out_(handler_out)
  , arg_map_out_(arg_map_out)
  , frame_(frame) { }

template <class I>
value_t SignalHandlerThunk<I>::call(sigmap_state_t<I, SignalHandlerOutput> *state) {
  barrier_iter_t barrier_iter;
  SignalHandlerOutput *output = state->output;
  value_t barrier = barrier_iter_init(&barrier_iter, frame_);
  while (!is_nothing(barrier)) {
    if (in_genus(dgSignalHandlerSection, barrier)) {
      output->set_current_handler(barrier);
      value_t methods = get_barrier_state_payload(barrier);
      CHECK_FAMILY(ofMethodspace, methods);
      value_t sigmap = get_methodspace_methods(methods);
      TRY(continue_sigmap_lookup(state, sigmap, methods));
    }
    barrier = barrier_iter_advance(&barrier_iter);
  }
  TRY_SET(*arg_map_out_, get_sigmap_lookup_argument_map(state));
  *handler_out_ = output->get_result_handler();
  return success();
}

// Performs normal method invocation lookup.
template <class I, class O>
class InvocationThunk {
public:
  InvocationThunk(I *input, value_t *arg_map_out);
  value_t call(sigmap_state_t<I, O> *state);
private:
  I *input_;
  value_t *arg_map_out_;

  // Does the specialized lookup that resolves lambda methods.
  value_t complete_special_lambda_lookup(value_t subject, sigmap_state_t<I, O> *state,
      value_t *arg_map_out);

  // Does the specialized lookup that resolves block methods.
  value_t complete_special_block_lookup(value_t subject, sigmap_state_t<I, O> *state,
      value_t *arg_map_out);

};

template <class I, class O>
InvocationThunk<I, O>::InvocationThunk(I *input, value_t *arg_map_out)
  : input_(input)
  , arg_map_out_(arg_map_out) { }

template <class I, class O>
value_t InvocationThunk<I, O>::call(sigmap_state_t<I, O> *state) {
  TRY(lookup_through_input(state));
  value_t result = state->output->get_result();
  TOPIC_INFO(Lookup, "Lookup result: %v", result);
  if (in_family(ofMethod, result)) {
    value_t result_flags = get_method_flags(result);
    if (!is_flag_set_empty(result_flags)) {
      TRY_DEF(subject, get_invocation_subject(input_));
      // The result has at least one special flag set so we have to give this
      // lookup special treatment.
      if (get_flag_set_at(result_flags, mfLambdaDelegate)) {
        return complete_special_lambda_lookup(subject, state, arg_map_out_);
      } else if (get_flag_set_at(result_flags, mfBlockDelegate)) {
        return complete_special_block_lookup(subject, state, arg_map_out_);
      }
    }
  }
  TRY_SET(*arg_map_out_, get_sigmap_lookup_argument_map(state));
  return success();
}

// Performs the extra lookup for lambda methods that happens when the lambda
// delegate method is found in the normal lookup.
template <class I, class O>
value_t InvocationThunk<I, O>::complete_special_lambda_lookup(value_t subject,
    sigmap_state_t<I, O> *state, value_t *arg_map_out) {
  CHECK_FAMILY(ofLambda, subject);
  MethodspaceThunk<I, O> thunk(get_lambda_methods(subject), arg_map_out);
  sigmap_state_reset(state);
  return thunk.call(state);
}

// Performs the extra lookup for block methods that happens when the block
// delegate method is found in the normal lookup.
template <class I, class O>
value_t InvocationThunk<I, O>::complete_special_block_lookup(value_t subject,
    sigmap_state_t<I, O> *state, value_t *arg_map_out) {
  CHECK_FAMILY(ofBlock, subject);
  value_t section = get_block_section(subject);
  MethodspaceThunk<I, O> thunk(get_block_section_methodspace(section),
      arg_map_out);
  sigmap_state_reset(state);
  return thunk.call(state);
}


/// ## Specializations

value_t match_signature_from_frame(value_t self, sigmap_input_layout_t *layout,
    frame_t *frame, value_t space, match_info_t *match_info,
    match_result_t *result_out) {
  if (is_nothing(layout->next_guards)) {
    FrameSigmapInput in(layout, frame);
    return generic_match_signature(self, &in, space, match_info, result_out);
  } else {
    FrameSigmapInputWithNexts in(layout, frame);
    return generic_match_signature(self, &in, space, match_info, result_out);
  }
}

value_t match_signature_from_call_data(value_t self, sigmap_input_layout_t *layout,
    value_t call_data, value_t space, match_info_t *match_info,
    match_result_t *result_out) {
  CallDataSigmapInput in(layout, call_data);
  return generic_match_signature(self, &in, space, match_info, result_out);
}

value_t lookup_method_full_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t *arg_map_out) {
  UniqueBestMatchOutput out;
  if (is_nothing(layout->next_guards)) {
    FrameSigmapInput in(layout, frame);
    InvocationThunk<FrameSigmapInput, UniqueBestMatchOutput> thunk(&in, arg_map_out);
    return generic_lookup_method(&thunk, &in, &out);
  } else {
    FrameSigmapInputWithNexts in(layout, frame);
    InvocationThunk<FrameSigmapInputWithNexts, UniqueBestMatchOutput> thunk(&in, arg_map_out);
    return generic_lookup_method(&thunk, &in, &out);
  }
}

value_t lookup_method_full_from_call_data(sigmap_input_layout_t *layout,
    value_t call_data, value_t *arg_map_out) {
  UniqueBestMatchOutput out;
  CallDataSigmapInput in(layout, call_data);
  InvocationThunk<CallDataSigmapInput, UniqueBestMatchOutput> thunk(&in, arg_map_out);
  return generic_lookup_method(&thunk, &in, &out);
}

value_t lookup_signal_handler_method_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t *handler_out, value_t *arg_map_out) {
  FrameSigmapInput in(layout, frame);
  SignalHandlerThunk<FrameSigmapInput> thunk(handler_out, arg_map_out, frame);
  SignalHandlerOutput out;
  return generic_lookup_method(&thunk, &in, &out);
}

value_t lookup_methodspace_method_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t methodspace, value_t *arg_map_out) {
  UniqueBestMatchOutput out;
  if (is_nothing(layout->next_guards)) {
    FrameSigmapInput in(layout, frame);
    MethodspaceThunk<FrameSigmapInput, UniqueBestMatchOutput> thunk(methodspace, arg_map_out);
    return generic_lookup_method(&thunk, &in, &out);
  } else {
    FrameSigmapInputWithNexts in(layout, frame);
    MethodspaceThunk<FrameSigmapInputWithNexts, UniqueBestMatchOutput> thunk(methodspace, arg_map_out);
    return generic_lookup_method(&thunk, &in, &out);
  }
}
