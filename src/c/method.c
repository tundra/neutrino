//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

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

sigmap_input_layout_t sigmap_input_layout_new(value_t ambience, value_t tags,
    value_t next_guards) {
  sigmap_input_layout_t result;
  result.ambience = ambience;
  result.tags = tags;
  result.next_guards = next_guards;
  return result;
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

int64_t get_signature_tag_count(value_t self) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_length(get_signature_tags(self));
}

value_t get_signature_tag_at(value_t self, int64_t index) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_first_at(get_signature_tags(self), index);
}

value_t get_signature_parameter_at(value_t self, int64_t index) {
  CHECK_FAMILY(ofSignature, self);
  return get_pair_array_second_at(get_signature_tags(self), index);
}

void signature_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<signature: ");
  for (int64_t i = 0; i < get_signature_parameter_count(self); i++) {
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

join_status_t join_score_vectors(value_t *target, value_t *source, size_t length) {
  // The bit fiddling here works because of how the enum values are chosen.
  uint32_t result = jsEqual;
  for (size_t i = 0; i < length; i++) {
    if (is_score_better(target[i], source[i])) {
      // The source was strictly worse than the target.
      result |= jsWorse;
    } else if (is_score_better(source[i], target[i])) {
      // The source was strictly better than the target; override.
      result |= jsBetter;
      target[i] = source[i];
    }
  }
  return (join_status_t) result;
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
    int64_t length = get_array_buffer_length(parents);
    value_t score = new_no_match_score();
    for (int64_t i = 0; i < length; i++) {
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

value_t guard_match(value_t guard, value_t value, runtime_t *runtime,
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
      TRY_DEF(primary, get_primary_type(value, runtime));
      value_t target = get_guard_value(guard);
      return find_best_match(runtime, primary, target,
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
ACCESSORS_IMPL(Method, method, acInFamily, ofFreezeCheat, CodePtr, code_ptr);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofMethodAst, Syntax, syntax);
ACCESSORS_IMPL(Method, method, acInFamilyOpt, ofModuleFragment, ModuleFragment,
    module_fragment);
ACCESSORS_IMPL(Method, method, acInPhylum, tpFlagSet, Flags, flags);

value_t method_validate(value_t self) {
  VALIDATE_FAMILY(ofMethod, self);
  VALIDATE_FAMILY_OPT(ofSignature, get_method_signature(self));
  value_t code_ptr = get_method_code_ptr(self);
  VALIDATE_FAMILY(ofFreezeCheat, code_ptr);
  VALIDATE_FAMILY_OPT(ofCodeBlock, get_freeze_cheat_value(code_ptr));
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
  TRY_FINALLY {
    E_TRY_DEF(signature, build_method_signature(runtime, fragment, &scratch,
        get_method_ast_signature(method_ast)));
    E_TRY_DEF(method, new_heap_method(runtime, afMutable, signature, method_ast,
        nothing(), fragment, new_flag_set(kFlagSetAllOff)));
    E_RETURN(method);
  } FINALLY {
    reusable_scratch_memory_dispose(&scratch);
  } YRT
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


// --- M e t h o d   s p a c e ---

ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofIdHashMap, Inheritance,
    inheritance);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofSignatureMap, Methods,
    methods);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamilyOpt, ofMethodspace, Parent,
    parent);
ACCESSORS_IMPL(Methodspace, methodspace, acInFamily, ofFreezeCheat, CachePtr,
    cache_ptr);

value_t methodspace_validate(value_t self) {
  VALIDATE_FAMILY(ofMethodspace, self);
  VALIDATE_FAMILY(ofIdHashMap, get_methodspace_inheritance(self));
  VALIDATE_FAMILY(ofSignatureMap, get_methodspace_methods(self));
  VALIDATE_FAMILY_OPT(ofMethodspace, get_methodspace_parent(self));
  VALIDATE_FAMILY(ofFreezeCheat, get_methodspace_cache_ptr(self));
  return success();
}

value_t ensure_methodspace_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_id_hash_map_frozen(runtime, get_methodspace_inheritance(self),
      mfFreezeValues));
  TRY(ensure_frozen(runtime, get_methodspace_methods(self)));
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
  invalidate_methodspace_caches(self);
  return add_to_array_buffer(runtime, parents, supertype);
}

value_t add_methodspace_method(runtime_t *runtime, value_t self,
    value_t method) {
  CHECK_FAMILY(ofMethodspace, self);
  CHECK_MUTABLE(self);
  CHECK_FAMILY(ofMethod, method);
  invalidate_methodspace_caches(self);
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

// Returns true if the given signature could possibly match an invocation where
// the given tag maps to the given value.
static bool can_match_eq(value_t signature, value_t tag, value_t value) {
  int64_t paramc = get_signature_parameter_count(signature);
  // First look for a matching parameter in the signature.
  value_t match = nothing();
  for (int64_t i = 0; i < paramc; i++) {
    value_t param = get_signature_parameter_at(signature, i);
    value_t tags = get_parameter_tags(param);
    if (in_array(tags, tag)) {
      match = param;
      break;
    }
  }
  if (is_nothing(match)) {
    // There was no matching parameter so this can only match if the signature
    // permits it as an extra argument.
    return get_signature_allow_extra(signature);
  } else {
    value_t guard = get_parameter_guard(match);
    if (get_guard_type(guard) == gtEq) {
      value_t eq_value = get_guard_value(guard);
      return value_identity_compare(value, eq_value);
    } else {
      return true;
    }
  }
}

static value_t create_methodspace_selector_slice(runtime_t *runtime, value_t self,
    value_t selector) {
  TRY_DEF(result, new_heap_signature_map(runtime));
  value_t current = self;
  while (!is_nothing(current)) {
    value_t methods = get_methodspace_methods(current);
    value_t entries = get_signature_map_entries(methods);
    for (int64_t i = 0; i < get_pair_array_buffer_length(entries); i++) {
      value_t signature = get_pair_array_buffer_first_at(entries, i);
      if (can_match_eq(signature, ROOT(runtime, selector_key), selector)) {
        value_t method = get_pair_array_buffer_second_at(entries, i);
        TRY(add_to_signature_map(runtime, result, signature, method));
      }
    }
    current = get_methodspace_parent(current);
  }
  return result;
}

value_t get_or_create_methodspace_selector_slice(runtime_t *runtime, value_t self,
    value_t selector) {
  value_t cache_ptr = get_methodspace_cache_ptr(self);
  value_t cache = get_freeze_cheat_value(cache_ptr);
  // Create the cache if it doesn't exist.
  if (is_nothing(cache)) {
    TRY_SET(cache, new_heap_id_hash_map(runtime, 128));
    set_freeze_cheat_value(cache_ptr, cache);
  }
  // Create the selector-specific cache if it doesn't exits.
  value_t slice = get_id_hash_map_at(cache, selector);
  if (in_condition_cause(ccNotFound, slice)) {
    TRY_SET(slice, create_methodspace_selector_slice(runtime, self, selector));
    TRY(set_id_hash_map_at(runtime, cache, selector, slice));
  }
  return slice;
}

void invalidate_methodspace_caches(value_t self) {
  value_t cache_ptr = get_methodspace_cache_ptr(self);
  set_freeze_cheat_value(cache_ptr, nothing());
}

void methodspace_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<methodspace ");
  value_t methods = get_methodspace_methods(self);
  value_print_inner_on(methods, context, -1);
  string_buffer_printf(context->buf, ">");
}


/// ## Call tags

ACCESSORS_IMPL(CallTags, call_tags, acInFamily, ofArray, Entries, entries);
ACCESSORS_IMPL(CallTags, call_tags, acInDomainOpt, vdInteger, SubjectOffset, subject_offset);
ACCESSORS_IMPL(CallTags, call_tags, acInDomainOpt, vdInteger, SelectorOffset, selector_offset);

value_t call_tags_validate(value_t self) {
  VALIDATE_FAMILY(ofCallTags, self);
  VALIDATE_FAMILY(ofArray, get_call_tags_entries(self));
  VALIDATE_DOMAIN_OPT(vdInteger, get_call_tags_subject_offset(self));
  VALIDATE_DOMAIN_OPT(vdInteger, get_call_tags_selector_offset(self));
  return success();
}

value_t get_call_tags_tag_at(value_t self, int64_t index) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_pair_array_first_at(entries, index);
}

int64_t get_call_tags_offset_at(value_t self, int64_t index) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_integer_value(get_pair_array_second_at(entries, index));
}

int64_t get_call_tags_entry_count(value_t self) {
  CHECK_FAMILY(ofCallTags, self);
  value_t entries = get_call_tags_entries(self);
  return get_pair_array_length(entries);
}

void check_call_tags_entries_unique(value_t tags) {
  if (get_pair_array_length(tags) == 0)
    return;
  value_t last_tag = get_pair_array_first_at(tags, 0);
  for (int64_t i = 1; i < get_pair_array_length(tags); i++) {
    value_t next_tag = get_pair_array_first_at(tags, i);
    if (value_identity_compare(last_tag, next_tag))
      FATAL("Tag %v occurs twice in %v", last_tag, tags);
    last_tag = next_tag;
  }
}

value_t build_call_tags_entries(runtime_t *runtime, value_t tags) {
  int64_t tag_count = get_array_length(tags);
  TRY_DEF(result, new_heap_pair_array(runtime, tag_count));
  for (int64_t i = 0; i < tag_count; i++) {
    set_pair_array_first_at(result, i, get_array_at(tags, i));
    // The offset is counted backwards because the argument evaluated last will
    // be at the top of the stack, that is, offset 0, and the first will be at
    // the bottom so has the highest offset.
    int64_t offset = tag_count - i - 1;
    set_pair_array_second_at(result, i, new_integer(offset));
  }
  TRY(co_sort_pair_array(result));
  IF_EXPENSIVE_CHECKS_ENABLED(check_call_tags_entries_unique(result));
  return result;
}

void print_invocation_on(value_t tags, frame_t *frame, string_buffer_t *buf) {
  int64_t arg_count = get_call_tags_entry_count(tags);
  string_buffer_printf(buf, "{");
  for (int64_t i = 0;  i < arg_count; i++) {
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
  int64_t arg_count = get_call_tags_entry_count(self);
  for (int64_t i = 0; i < arg_count; i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_t tag = get_call_tags_tag_at(self, i);
    size_t offset = (size_t) get_call_tags_offset_at(self, i);
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

value_t get_call_data_value_at(value_t self, int64_t param_index) {
  value_t tags = get_call_data_tags(self);
  int64_t offset = get_call_tags_offset_at(tags, param_index);
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
  for (int64_t i = 0; i < get_call_tags_entry_count(tags); i++) {
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
  unquote_context.flags |= pfUnquote;
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
    case otSuffix:
      string_buffer_printf(context->buf, "()");
      value_print_inner_on(value, &unquote_context, -1);
      break;
    default:
      UNREACHABLE("unexpected operation type");
      break;
  }
}

void operation_print_open_on(value_t self, value_t transport,
    print_on_context_t *context) {
  value_t value = get_operation_value(self);
  print_on_context_t unquote_context = *context;
  unquote_context.flags |= pfUnquote;
  bool is_async = is_same_value(transport, transport_async());
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
    string_buffer_printf(context->buf, is_async ? "->" : ".");
    value_print_inner_on(value, &unquote_context, -1);
    string_buffer_printf(context->buf, "(");
    break;
  case otPrefix:
    value_print_inner_on(value, &unquote_context, -1);
    string_buffer_printf(context->buf, "(");
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
  unquote_context.flags |= pfUnquote;
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
  return ensure_frozen(runtime, object);
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
    ofUtf8, Name, name);
ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInFamily,
    ofCodeBlock, Code, code);
INTEGER_ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation,
    ArgumentCount, argument_count);
ACCESSORS_IMPL(BuiltinImplementation, builtin_implementation, acInPhylum,
    tpFlagSet, MethodFlags, method_flags);

value_t builtin_implementation_validate(value_t self) {
  VALIDATE_FAMILY(ofBuiltinImplementation, self);
  VALIDATE_FAMILY(ofUtf8, get_builtin_implementation_name(self));
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
