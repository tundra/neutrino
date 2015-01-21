//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "freeze.h"
#include "runtime-inl.h"
#include "syntax.h"
#include "try-inl.h"
#include "utils-inl.h"
#include "utils/log.h"
#include "utils/ook.h"
#include "value-inl.h"


// --- M i s c ---

value_t compile_expression(runtime_t *runtime, value_t program, value_t fragment,
    scope_o *scope, value_t reify_params_opt) {
  assembler_t assm;
  // Don't try to execute cleanup if this fails since there'll not be an
  // assembler to dispose.
  TRY(assembler_init(&assm, runtime, fragment, scope));
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(code_block, compile_expression_with_assembler(runtime, program,
        &assm, reify_params_opt));
    E_RETURN(code_block);
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

value_t compile_expression_with_assembler(runtime_t *runtime, value_t program,
    assembler_t *assm, value_t reify_params_opt) {
  if (!is_nothing(reify_params_opt))
    TRY(assembler_emit_reify_arguments(assm, reify_params_opt));
  TRY(emit_value(program, assm));
  if (!is_nothing(reify_params_opt))
    TRY(assembler_emit_slap(assm, 1));
  TRY(assembler_emit_return(assm));
  TRY_DEF(code_block, assembler_flush(assm));
  return code_block;
}

value_t safe_compile_expression(runtime_t *runtime, safe_value_t ast,
    safe_value_t module, scope_o *scope) {
  RETRY_ONCE_IMPL(runtime, compile_expression(runtime, deref(ast), deref(module),
      scope, nothing()));
}

size_t get_parameter_order_index_for_array(value_t tags) {
  size_t result = kMaxOrderIndex;
  for (size_t i = 0; i < get_array_length(tags); i++) {
    value_t tag = get_array_at(tags, i);
    if (is_integer(tag)) {
      result = min_size(result, 2 + get_integer_value(tag));
    } else if (in_family(ofKey, tag)) {
      size_t id = get_key_id(tag);
      if (id < 2)
        result = min_size(result, id);
    }
  }
  return result;
}

// Given two pointers to value arrays, compares them according to the parameter
// ordering for arrays.
static int compare_parameter_ordering_entries(const void *vp_a, const void *vp_b) {
  value_t a = *((const value_t*) vp_a);
  value_t b = *((const value_t*) vp_b);
  size_t oi_a = get_parameter_order_index_for_array(a);
  size_t oi_b = get_parameter_order_index_for_array(b);
  if (oi_a < oi_b) {
    return -1;
  } else if (oi_a > oi_b) {
    return 1;
  } else {
    return 0;
  }
}

// Abstract implementation of the parameter ordering function that works on
// any kind of object that has a set of tags. The get_entry_tags function
// argument is responsible for extracting the tags.
size_t *calc_parameter_ast_ordering(reusable_scratch_memory_t *scratch,
    value_t params) {
  size_t tagc = get_array_length(params);

  // Allocate the temporary storage and result array.
  value_t *pairs = NULL;
  size_t *result = NULL;
  reusable_scratch_memory_double_alloc(scratch,
      tagc * 2 * sizeof(value_t), (void**) &pairs,
      tagc * sizeof(size_t), (void**) &result);

  // First store the tag arrays in the pairs array, each along with the index
  // it came from in the tag array.
  for (size_t i = 0; i < tagc; i++) {
    value_t param = get_array_at(params, i);
    pairs[i * 2] = get_parameter_ast_tags(param);
    pairs[(i * 2) + 1] = new_integer(i);
  }
  // Sort the entries by parameter ordering. This moves the subject and selector
  // parameters to the front, followed by the integers, followed by the rest
  // in some arbitrary order. Note that the *2 means that the integers are
  // just moved along, they're not included in the sorting.
  //
  // This assumes that qsort is consistent, that is, that it sorts two arrays
  // the same way if compare_parameter_ordering_entries returns the same
  // comparisons.
  qsort(pairs, tagc, sizeof(value_t) * 2, compare_parameter_ordering_entries);
  // Transfer the resulting ordering to the output array.
  for (size_t i = 0; i < tagc; i++) {
    // This is the original position of the entry that is now the i'th in the
    // sorted parameter order.
    value_t origin = pairs[(i * 2) + 1];
    // Store a reverse mapping from the origin to that position.
    result[get_integer_value(origin)] = i;
  }

  return result;
}

// Forward declare all the emit methods.
#define __EMIT_SYNTAX_FAMILY_EMIT__(Family, family, MD, SR, MINOR, N)          \
    mfEm MINOR (                                                               \
      value_t emit_##family(value_t, assembler_t *);,                          \
      )
    ENUM_HEAP_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_EMIT__)
#undef __EMIT_SYNTAX_FAMILY_EMIT__

// --- L i t e r a l ---

GET_FAMILY_PRIMARY_TYPE_IMPL(literal_ast);
NO_BUILTIN_METHODS(literal_ast);

ACCESSORS_IMPL(LiteralAst, literal_ast, acNoCheck, 0, Value, value);

value_t literal_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLiteralAst, self);
  return success();
}

void literal_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<");
  value_print_inner_on(get_literal_ast_value(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_new_literal_ast(runtime_t *runtime) {
  return new_heap_literal_ast(runtime, afMutable, null());
}

value_t plankton_set_literal_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, value);
  set_literal_ast_value(object, value_value);
  return ensure_frozen(runtime, object);
}

value_t emit_literal_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofLiteralAst, value);
  return assembler_emit_push(assm, get_literal_ast_value(value));
}


// --- A r r a y ---

GET_FAMILY_PRIMARY_TYPE_IMPL(array_ast);
NO_BUILTIN_METHODS(array_ast);

ACCESSORS_IMPL(ArrayAst, array_ast, acInFamilyOpt, ofArray, Elements, elements);

value_t emit_array_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofArrayAst, value);
  value_t elements = get_array_ast_elements(value);
  size_t length = get_array_length(elements);
  for (size_t i = 0; i < length; i++)
    TRY(emit_value(get_array_at(elements, i), assm));
  TRY(assembler_emit_new_array(assm, length));
  return success();
}

value_t array_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofArrayAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_array_ast_elements(value));
  return success();
}

void array_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<array ast: ");
  value_print_inner_on(get_array_ast_elements(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_array_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, elements);
  set_array_ast_elements(object, elements_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_array_ast(runtime_t *runtime) {
  return new_heap_array_ast(runtime, afMutable, nothing());
}


// --- I n v o c a t i o n ---

TRIVIAL_PRINT_ON_IMPL(InvocationAst, invocation_ast);
GET_FAMILY_PRIMARY_TYPE_IMPL(invocation_ast);
NO_BUILTIN_METHODS(invocation_ast);

ACCESSORS_IMPL(InvocationAst, invocation_ast, acInFamilyOpt, ofArray, Arguments,
    arguments);

static value_t create_call_tags(value_t arguments, assembler_t *assm) {
  size_t arg_count = get_array_length(arguments);
  // Build the invocation record and emit the values at the same time.
  TRY_DEF(entries, new_heap_pair_array(assm->runtime, arg_count));
  for (size_t i = 0; i < arg_count; i++) {
    value_t argument = get_array_at(arguments, i);
    // Add the tag to the invocation record.
    value_t tag = get_argument_ast_tag(argument);
    set_pair_array_first_at(entries, i, tag);
    set_pair_array_second_at(entries, i, new_integer(arg_count - i - 1));
    // Emit the value.
    value_t value = get_argument_ast_value(argument);
    TRY(emit_value(value, assm));
  }
  TRY(co_sort_pair_array(entries));
  return new_heap_call_tags(assm->runtime, afFreeze, entries);
}

// Given a guard ast, evaluates it to a guard within the given fragment. There
// is some slight cheating involved here but it'll work for now.
static value_t evaluate_guard(runtime_t *runtime, value_t guard_ast, value_t fragment) {
  guard_type_t guard_type = get_guard_ast_type(guard_ast);
  if (guard_type == gtAny) {
    return ROOT(runtime, any_guard);
  } else {
    value_t guard_value_ast = get_guard_ast_value(guard_ast);
    TRY_DEF(guard_value, quick_and_dirty_evaluate_syntax(runtime, fragment,
        guard_value_ast));
    return new_heap_guard(runtime, afFreeze, guard_type, guard_value);
  }
}

// Given an array of argument asts, returns the guard array to use if this is
// a next call, otherwise nothing.
static value_t create_call_next_guards(value_t arguments, assembler_t *assm) {
  bool has_next = false;
  size_t arg_count = get_array_length(arguments);
  for (size_t i = 0; i < arg_count && !has_next; i++) {
    value_t arg = get_array_at(arguments, i);
    has_next = !is_nothing(get_argument_ast_next_guard(arg));
  }
  // There aren't any next guards so we can just stop here.
  if (!has_next)
    return nothing();
  // Build an array that matches the arguments that gives, for each one, the
  // guard to match the argument against.
  runtime_t *runtime = assm->runtime;
  TRY_DEF(result, new_heap_array(runtime, arg_count));
  for (size_t i = 0; i < arg_count; i++) {
    value_t arg = get_array_at(arguments, i);
    value_t guard_ast = get_argument_ast_next_guard(arg);
    if (is_nothing(guard_ast)) {
      set_array_at(result, i, nothing());
    } else {
      TRY_DEF(guard, evaluate_guard(runtime, guard_ast, assm->fragment));
      set_array_at(result, i, guard);
    }
  }
  return result;
}

value_t emit_invocation_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofInvocationAst, value);
  value_t arguments = get_invocation_ast_arguments(value);
  TRY_DEF(record, create_call_tags(arguments, assm));
  TRY_DEF(next_guards, create_call_next_guards(arguments, assm));
  TRY(assembler_emit_invocation(assm, assm->fragment, record, next_guards));
  size_t argc = get_call_tags_entry_count(record);
  TRY(assembler_emit_slap(assm, argc));
  return success();
}

value_t invocation_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofInvocationAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_invocation_ast_arguments(value));
  return success();
}

value_t plankton_set_invocation_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, arguments);
  set_invocation_ast_arguments(object, arguments_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_invocation_ast(runtime_t *runtime) {
  return new_heap_invocation_ast(runtime, afMutable, nothing());
}


/// ## Call literal ast

TRIVIAL_PRINT_ON_IMPL(CallLiteralAst, call_literal_ast);
NO_BUILTIN_METHODS(call_literal_ast);

ACCESSORS_IMPL(CallLiteralAst, call_literal_ast, acInFamilyOpt, ofArray, Arguments,
    arguments);

value_t emit_call_literal_ast(value_t value, assembler_t *assm) {
  value_t args = get_call_literal_ast_arguments(value);
  size_t argc = get_array_length(args);
  for (size_t i = 0; i < argc; i++) {
    value_t arg = get_array_at(args, i);
    value_t tag = get_call_literal_argument_ast_tag(arg);
    value_t value = get_call_literal_argument_ast_value(arg);
    TRY(emit_value(tag, assm));
    TRY(emit_value(value, assm));
  }
  TRY(assembler_emit_create_call_data(assm, argc));
  return success();
}

value_t call_literal_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofCallLiteralAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_call_literal_ast_arguments(value));
  return success();
}

value_t plankton_set_call_literal_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, arguments);
  set_call_literal_ast_arguments(object, arguments_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_call_literal_ast(runtime_t *runtime) {
  return new_heap_call_literal_ast(runtime, afMutable, nothing());
}


/// ## Call literal argument ast

TRIVIAL_PRINT_ON_IMPL(CallLiteralArgumentAst, call_literal_argument_ast);
NO_BUILTIN_METHODS(call_literal_argument_ast);

ACCESSORS_IMPL(CallLiteralArgumentAst, call_literal_argument_ast, acIsSyntaxOpt,
    0, Tag, tag);

ACCESSORS_IMPL(CallLiteralArgumentAst, call_literal_argument_ast, acIsSyntaxOpt,
    0, Value, value);

value_t call_literal_argument_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofCallLiteralArgumentAst, value);
  return success();
}

value_t plankton_set_call_literal_argument_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, tag, value);
  set_call_literal_argument_ast_tag(object, tag_value);
  set_call_literal_argument_ast_value(object, value_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_call_literal_argument_ast(runtime_t *runtime) {
  return new_heap_call_literal_argument_ast(runtime, afMutable, nothing(), nothing());
}


/// ## Signal ast

TRIVIAL_PRINT_ON_IMPL(SignalAst, signal_ast);
GET_FAMILY_PRIMARY_TYPE_IMPL(signal_ast);
NO_BUILTIN_METHODS(signal_ast);

ACCESSORS_IMPL(SignalAst, signal_ast, acInFamilyOpt, ofArray, Arguments,
    arguments);
ACCESSORS_IMPL(SignalAst, signal_ast, acNoCheck, 0, Escape, escape);
ACCESSORS_IMPL(SignalAst, signal_ast, acNoCheck, 0, Default, default);

value_t emit_signal_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofSignalAst, value);
  bool escape = get_boolean_value(get_signal_ast_escape(value));
  // First emit the signal arguments.
  value_t arguments = get_signal_ast_arguments(value);
  value_t record = create_call_tags(arguments, assm);
  opcode_t opcode = escape ? ocSignalEscape : ocSignalContinue;
  // Try invoking the handler.
  TRY(assembler_emit_signal(assm, opcode, record));
  // At this point there are three possible cases.
  //
  //   - Either the handler will have been executed in which case there's an
  //     extra result on the stack.
  //   - No handler was found and this is a continuing signal, in which case the
  //     goto that's coming up will be skipped and the else-part executed.
  //   - No handler was found and this is an escaping signal, in which case
  //     execution will have been abandoned.
  //
  // Either way, if execution does continue we'll arrive at the goto's
  // destination with a new argument on the stack.
  size_t argc = get_call_tags_entry_count(record);
  if (escape) {
    assembler_adjust_stack_height(assm, +1);
    TRY(assembler_emit_leave_or_fire_barrier(assm, argc));
  } else {
    short_buffer_cursor_t dest;
    size_t code_start_offset = assembler_get_code_cursor(assm);
    TRY(assembler_emit_goto_forward(assm, &dest));
    // This is the default part where no handler was executed.
    value_t defawlt = get_signal_ast_default(value);
    if (is_nothing(defawlt)) {
      // TODO: this is the case where there is no explicit default handler and
      //   no handler was found. This works but longer term it should probably
      //   generate an escaping signal.
      assembler_emit_push(assm, null());
    } else {
      TRY(emit_value(defawlt, assm));
    }
    size_t code_end_offset = assembler_get_code_cursor(assm);
    short_buffer_cursor_set(&dest, code_end_offset - code_start_offset);
  }
  // We've now either executed the handler (the goto will have jumped here)
  // or there was no handler and the default will have been executed. In either
  // case there's a result on top of the arguments so slap them off the stack.
  assembler_emit_slap(assm, argc);
  return success();
}

value_t signal_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofSignalAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_signal_ast_arguments(value));
  return success();
}

value_t plankton_set_signal_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, arguments, escape, default);
  set_signal_ast_escape(object, escape_value);
  set_signal_ast_arguments(object, arguments_value);
  set_signal_ast_default(object, null_to_nothing(default_value));
  return ensure_frozen(runtime, object);
}

value_t plankton_new_signal_ast(runtime_t *runtime) {
  return new_heap_signal_ast(runtime, afMutable, nothing(), nothing(), nothing());
}


/// ## Signal handler ast

TRIVIAL_PRINT_ON_IMPL(SignalHandlerAst, signal_handler_ast);
GET_FAMILY_PRIMARY_TYPE_IMPL(signal_handler_ast);
NO_BUILTIN_METHODS(signal_handler_ast);

ACCESSORS_IMPL(SignalHandlerAst, signal_handler_ast, acIsSyntaxOpt, 0, Body, body);
ACCESSORS_IMPL(SignalHandlerAst, signal_handler_ast, acInFamilyOpt, ofArray,
    Handlers, handlers);

static value_t build_methodspace_from_method_asts(value_t method_asts,
    assembler_t *assm) {
  CHECK_FAMILY(ofArray, method_asts);
  runtime_t *runtime = assm->runtime;
  TRY_DEF(space, new_heap_methodspace(runtime, nothing()));

  for (size_t i = 0; i < get_array_length(method_asts); i++) {
    value_t method_ast = get_array_at(method_asts, i);
    // Compile the signature and, if we're in a nontrivial inner scope, the
    // body of the lambda.
    value_t body_code = nothing();
    if (assm->scope != scope_get_bottom())
      TRY_SET(body_code, compile_method_body(assm, method_ast));
    TRY_DEF(signature, build_method_signature(assm->runtime, assm->fragment,
        assembler_get_scratch_memory(assm), get_method_ast_signature(method_ast)));

    // Build a method space in which to store the method.
    TRY_DEF(method, new_heap_method(runtime, afFreeze, signature,
        nothing(), body_code, nothing(), new_flag_set(kFlagSetAllOff)));
    TRY(add_methodspace_method(runtime, space, method));
  }

  return space;
}

value_t emit_signal_handler_ast(value_t value, assembler_t *assm) {
  // Create the methodspace that holds the handlers.
  block_scope_o block_scope;
  TRY(assembler_push_block_scope(assm, &block_scope));
  value_t handler_asts = get_signal_handler_ast_handlers(value);
  TRY_DEF(space, build_methodspace_from_method_asts(handler_asts, assm));
  assembler_pop_block_scope(assm, &block_scope);

  short_buffer_cursor_t cursor;
  TRY(assembler_emit_install_signal_handler(assm, space, &cursor));
  size_t start_offset = assembler_get_code_cursor(assm);
  TRY(emit_value(get_signal_handler_ast_body(value), assm));
  size_t end_offset = assembler_get_code_cursor(assm);
  short_buffer_cursor_set(&cursor, end_offset - start_offset);
  TRY(assembler_emit_uninstall_signal_handler(assm));

  return success();
}

value_t signal_handler_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofSignalHandlerAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_signal_handler_ast_handlers(value));
  return success();
}

value_t plankton_set_signal_handler_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, body, handlers);
  set_signal_handler_ast_body(object, body_value);
  set_signal_handler_ast_handlers(object, handlers_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_signal_handler_ast(runtime_t *runtime) {
  return new_heap_signal_handler_ast(runtime, afMutable, nothing(), nothing());
}


// --- E n s u r e   a s t ---

TRIVIAL_PRINT_ON_IMPL(EnsureAst, ensure_ast);
GET_FAMILY_PRIMARY_TYPE_IMPL(ensure_ast);
NO_BUILTIN_METHODS(ensure_ast);

ACCESSORS_IMPL(EnsureAst, ensure_ast, acIsSyntaxOpt, 0, Body, body);
ACCESSORS_IMPL(EnsureAst, ensure_ast, acIsSyntaxOpt, 0, OnExit, on_exit);

static value_t emit_ensurer(value_t code, assembler_t *assm) {
  // Push a block scope that refracts any symbols accessed outside the block.
  block_scope_o block_scope;
  TRY(assembler_push_block_scope(assm, &block_scope));

  TRY_DEF(code_block, compile_expression(assm->runtime, code, assm->fragment,
        UPCAST(&block_scope), nothing()));

  // Pop the block scope off, we're done refracting.
  assembler_pop_block_scope(assm, &block_scope);

  // Finally emit the bytecode that will create the block.
  TRY(assembler_emit_create_ensurer(assm, code_block));
  return success();
}

value_t emit_ensure_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofEnsureAst, self);
  value_t body = get_ensure_ast_body(self);
  value_t on_exit = get_ensure_ast_on_exit(self);
  TRY(emit_ensurer(on_exit, assm));
  TRY(emit_value(body, assm));
  TRY(assembler_emit_call_ensurer(assm));
  TRY(assembler_emit_dispose_ensurer(assm));
  return success();
}

value_t ensure_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofEnsureAst, value);
  return success();
}

value_t plankton_set_ensure_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, body, on_exit);
  set_ensure_ast_body(object, body_value);
  set_ensure_ast_on_exit(object, on_exit_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_ensure_ast(runtime_t *runtime) {
  return new_heap_ensure_ast(runtime, afMutable, nothing(), nothing());
}


// --- A r g u m e n t ---

TRIVIAL_PRINT_ON_IMPL(ArgumentAst, argument_ast);
GET_FAMILY_PRIMARY_TYPE_IMPL(argument_ast);
NO_BUILTIN_METHODS(argument_ast);

ACCESSORS_IMPL(ArgumentAst, argument_ast, acNoCheck, 0, Tag, tag);
ACCESSORS_IMPL(ArgumentAst, argument_ast, acIsSyntaxOpt, 0, Value, value);
ACCESSORS_IMPL(ArgumentAst, argument_ast, acInFamilyOpt, ofGuardAst, NextGuard, next_guard);

value_t argument_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofArgumentAst, value);
  VALIDATE_FAMILY_OPT(ofGuardAst, get_argument_ast_next_guard(value));
  return success();
}

value_t plankton_set_argument_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, tag, value, next_guard);
  set_argument_ast_tag(object, tag_value);
  set_argument_ast_value(object, value_value);
  if (!is_null(next_guard_value))
    set_argument_ast_next_guard(object, next_guard_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_argument_ast(runtime_t *runtime) {
  return new_heap_argument_ast(runtime, afMutable, nothing(), nothing(), nothing());
}


// --- S e q u e n c e ---

GET_FAMILY_PRIMARY_TYPE_IMPL(sequence_ast);
NO_BUILTIN_METHODS(sequence_ast);

ACCESSORS_IMPL(SequenceAst, sequence_ast, acInFamily, ofArray, Values, values);

value_t emit_sequence_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofSequenceAst, value);
  value_t values = get_sequence_ast_values(value);
  size_t length = get_array_length(values);
  if (length == 0) {
    // A no-element sequence has value null.
    TRY(assembler_emit_push(assm, null()));
  } else if (length == 1) {
    // A one-element sequence is equivalent to the value of the one element.
    TRY(emit_value(get_array_at(values, 0), assm));
  } else {
    for (size_t i = 0; i < length; i++) {
      if (i > 0) {
        // For all subsequent expressions we need to pop the previous value
        // first.
        TRY(assembler_emit_pop(assm, 1));
      }
      TRY(emit_value(get_array_at(values, i), assm));
    }
  }
  return success();
}

value_t sequence_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofSequenceAst, value);
  VALIDATE_FAMILY_OPT(ofArray, get_sequence_ast_values(value));
  return success();
}

void sequence_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<sequence ast: ");
  value_print_inner_on(get_sequence_ast_values(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_sequence_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, values);
  set_sequence_ast_values(object, values_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_sequence_ast(runtime_t *runtime) {
  return new_heap_sequence_ast(runtime, afMutable, ROOT(runtime, empty_array));
}


// --- L o c a l   d e c l a r a t i o n   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(local_declaration_ast);
NO_BUILTIN_METHODS(local_declaration_ast);

ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acInFamilyOpt,
    ofSymbolAst, Symbol, symbol);
ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acNoCheck, 0,
    IsMutable, is_mutable);
ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acIsSyntaxOpt,
    0, Value, value);
ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acIsSyntaxOpt,
    0, Body, body);

value_t emit_local_declaration_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofLocalDeclarationAst, self);
  // Record the stack offset where the value is being pushed.
  size_t offset = assm->stack_height;
  // Emit the value, wrapping it in a reference if this is a mutable local. The
  // reference approach is really inefficient but gives the correct semantics
  // with little effort.
  value_t value = get_local_declaration_ast_value(self);
  TRY(emit_value(value, assm));
  value_t is_mutable = get_local_declaration_ast_is_mutable(self);
  if (get_boolean_value(is_mutable))
    TRY(assembler_emit_new_reference(assm));
  // Record in the scope chain that the symbol is bound and where the value is
  // located on the stack. It is the responsibility of anyone reading or writing
  // the variable to dereference the value as appropriate.
  value_t symbol = get_local_declaration_ast_symbol(self);
  CHECK_FAMILY(ofSymbolAst, symbol);
  if (assembler_is_symbol_bound(assm, symbol))
    // We're trying to redefine an already defined symbol. That's not valid.
    return new_invalid_syntax_condition(isSymbolAlreadyBound);
  single_symbol_scope_o scope;
  assembler_push_single_symbol_scope(assm, &scope, symbol, btLocal, offset);
  value_t body = get_local_declaration_ast_body(self);
  // Emit the body in scope of the local.
  TRY(emit_value(body, assm));
  assembler_pop_single_symbol_scope(assm, &scope);
  // Slap the value of the local off, leaving just the value of the body.
  TRY(assembler_emit_slap(assm, 1));
  return success();
}

value_t local_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLocalDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_local_declaration_ast_symbol(self));
  return success();
}

void local_declaration_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<local declaration ast: ");
  value_print_inner_on(get_local_declaration_ast_symbol(value), context, -1);
  string_buffer_printf(context->buf, " := ");
  value_print_inner_on(get_local_declaration_ast_value(value), context, -1);
  string_buffer_printf(context->buf, " in ");
  value_print_inner_on(get_local_declaration_ast_body(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_local_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol, is_mutable, value, body);
  set_local_declaration_ast_symbol(object, symbol_value);
  set_local_declaration_ast_is_mutable(object, is_mutable_value);
  set_local_declaration_ast_value(object, value_value);
  set_local_declaration_ast_body(object, body_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_local_declaration_ast(runtime_t *runtime) {
  return new_heap_local_declaration_ast(runtime, afMutable, nothing(), nothing(),
      nothing(), nothing());
}


// --- B l o c k   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(block_ast);
NO_BUILTIN_METHODS(block_ast);
TRIVIAL_PRINT_ON_IMPL(BlockAst, block_ast);

ACCESSORS_IMPL(BlockAst, block_ast, acInFamilyOpt, ofSymbolAst, Symbol, symbol);
ACCESSORS_IMPL(BlockAst, block_ast, acInFamilyOpt, ofArray, Methods, methods);
ACCESSORS_IMPL(BlockAst, block_ast, acIsSyntaxOpt, 0, Body, body);

// Pushes the binding of a symbol onto the stack. If the symbol is mutable this
// will push the reference, not the value. It is the caller's responsibility to
// dereference the value as appropriate. The is_ref out argument indicates
// whether that is relevant.
static value_t assembler_access_symbol(value_t symbol, assembler_t *assm,
    bool *is_ref_out) {
  CHECK_FAMILY(ofSymbolAst, symbol);
  binding_info_t binding;
  if (in_condition_cause(ccNotFound, assembler_lookup_symbol(assm, symbol, &binding)))
    // We're trying to access a symbol that hasn't been defined here. That's
    // not valid.
    return new_invalid_syntax_condition(isSymbolNotBound);
  if (binding.block_depth == 0) {
    // Direct reads from the current scope.
    switch (binding.type) {
      case btLocal:
        TRY(assembler_emit_load_local(assm, binding.data));
        break;
      case btArgument:
        TRY(assembler_emit_load_argument(assm, binding.data));
        break;
      case btLambdaCaptured:
        TRY(assembler_emit_load_lambda_capture(assm, binding.data));
        break;
      default:
        WARN("Unknown binding type %i", binding.type);
        UNREACHABLE("unknown binding type");
        break;
    }
  } else {
    // Indirect reads through one or more blocks into an enclosing scope.
    switch (binding.type) {
      case btArgument:
        TRY(assembler_emit_load_refracted_argument(assm, binding.data,
            binding.block_depth));
        break;
      case btLambdaCaptured:
        TRY(assembler_emit_load_refracted_capture(assm, binding.data,
            binding.block_depth));
        break;
      case btLocal:
        TRY(assembler_emit_load_refracted_local(assm, binding.data,
            binding.block_depth));
        break;
      default:
        WARN("Unknown refracted binding type %i", binding.type);
        UNREACHABLE("unknown block binding type");
        break;
    }
  }
  if (is_ref_out != NULL) {
    value_t origin = get_symbol_ast_origin(symbol);
    if (in_family(ofLocalDeclarationAst, origin)) {
      value_t is_mutable = get_local_declaration_ast_is_mutable(origin);
      *is_ref_out = get_boolean_value(is_mutable);
    }
  }
  return success();
}

static value_t emit_block_value(value_t method_asts, assembler_t *assm) {
  // Push a block scope that refracts any symbols accessed outside the block.
  block_scope_o block_scope;
  TRY(assembler_push_block_scope(assm, &block_scope));

  TRY_DEF(space, build_methodspace_from_method_asts(method_asts, assm));

  // Pop the block scope off, we're done refracting.
  assembler_pop_block_scope(assm, &block_scope);

  // Finally emit the bytecode that will create the block.
  TRY(assembler_emit_create_block(assm, space));
  return success();
}

value_t emit_block_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofBlockAst, self);
  // Record the stack offset where the value is being pushed.
  size_t offset = assm->stack_height + get_genus_descriptor(dgBlockSection)->field_count;
  value_t method_asts = get_block_ast_methods(self);
  TRY(emit_block_value(method_asts, assm));
  // Record in the scope chain that the symbol is bound and where the value is
  // located on the stack.
  value_t symbol = get_block_ast_symbol(self);
  CHECK_FAMILY(ofSymbolAst, symbol);
  if (assembler_is_symbol_bound(assm, symbol))
    // We're trying to redefine an already defined symbol. That's not valid.
    return new_invalid_syntax_condition(isSymbolAlreadyBound);
  single_symbol_scope_o scope;
  assembler_push_single_symbol_scope(assm, &scope, symbol, btLocal, offset);
  value_t body = get_block_ast_body(self);
  // Emit the body in scope of the local.
  TRY(emit_value(body, assm));
  assembler_pop_single_symbol_scope(assm, &scope);
  // Ensure that the block is dead now that we're leaving its scope.
  TRY(assembler_emit_dispose_block(assm));
  return success();
}

value_t block_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofBlockAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_block_ast_symbol(self));
  return success();
}

value_t plankton_set_block_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol, methods, body);
  set_block_ast_symbol(object, symbol_value);
  set_block_ast_methods(object, methods_value);
  set_block_ast_body(object, body_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_block_ast(runtime_t *runtime) {
  return new_heap_block_ast(runtime, afMutable, nothing(), nothing(), nothing());
}


// --- W i t h   e s c a p e   a s t ---

TRIVIAL_PRINT_ON_IMPL(WithEscapeAst, with_escape_ast);

ACCESSORS_IMPL(WithEscapeAst, with_escape_ast, acInFamilyOpt, ofSymbolAst,
    Symbol, symbol);
ACCESSORS_IMPL(WithEscapeAst, with_escape_ast, acIsSyntaxOpt, 0, Body, body);

value_t with_escape_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofWithEscapeAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_with_escape_ast_symbol(self));
  return success();
}

value_t emit_with_escape_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofWithEscapeAst, self);
  // Capture the escape.
  short_buffer_cursor_t dest;
  TRY(assembler_emit_create_escape(assm, &dest));
  size_t code_start_offset = assembler_get_code_cursor(assm);
  // The capture will be pushed as the bottom value of the stack barrier, so
  // stack-barrier-size down from the current stack height.
  size_t stack_offset = assm->stack_height - 1;
  // Record in the scope chain that the symbol is bound and where the value is
  // located on the stack.
  value_t symbol = get_with_escape_ast_symbol(self);
  CHECK_FAMILY(ofSymbolAst, symbol);
  if (assembler_is_symbol_bound(assm, symbol))
    // We're trying to redefine an already defined symbol. That's not valid.
    return new_invalid_syntax_condition(isSymbolAlreadyBound);
  single_symbol_scope_o scope;
  assembler_push_single_symbol_scope(assm, &scope, symbol, btLocal, stack_offset);
  value_t body = get_with_escape_ast_body(self);
  // Emit the body in scope of the local.
  TRY(emit_value(body, assm));
  assembler_pop_single_symbol_scope(assm, &scope);
  // If the escape is ever fired it will drop down to this location, leaving
  // the value on top of the stack. That way the stack cleanup happens the same
  // way whether you return normally or escape.
  size_t code_end_offset = assembler_get_code_cursor(assm);
  short_buffer_cursor_set(&dest, code_end_offset - code_start_offset);
  // Ensure that the escape is dead and remove the section, leaving just the
  // value of the body or the escaped value.
  TRY(assembler_emit_dispose_escape(assm));
  return success();
}

value_t plankton_set_with_escape_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol, body);
  set_with_escape_ast_symbol(object, symbol_value);
  set_with_escape_ast_body(object, body_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_with_escape_ast(runtime_t *runtime) {
  return new_heap_with_escape_ast(runtime, afMutable, nothing(), nothing());
}


// --- V a r i a b l e   a s s i g n m e n t   a s t ---

TRIVIAL_PRINT_ON_IMPL(VariableAssignmentAst, variable_assignment_ast);

ACCESSORS_IMPL(VariableAssignmentAst, variable_assignment_ast, acIsSyntaxOpt, 0,
    Target, target);
ACCESSORS_IMPL(VariableAssignmentAst, variable_assignment_ast, acIsSyntaxOpt, 0,
    Value, value);

value_t variable_assignment_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofVariableAssignmentAst, self);
  return success();
}

value_t plankton_set_variable_assignment_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, target, value);
  set_variable_assignment_ast_target(object, target_value);
  set_variable_assignment_ast_value(object, value_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_variable_assignment_ast(runtime_t *runtime) {
  return new_heap_variable_assignment_ast(runtime, afMutable, nothing(), nothing());
}

// Loads the value of the given symbol onto the stack. If the variable is
// mutable the reference that holds the value is read as appropriate.
static value_t assembler_load_symbol(value_t symbol, assembler_t *assm) {
  bool is_ref = false;
  TRY(assembler_access_symbol(symbol, assm, &is_ref));
  if (is_ref)
    TRY(assembler_emit_get_reference(assm));
  return success();
}

value_t emit_variable_assignment_ast(value_t self, assembler_t *assm) {
  // First push the value we're going to store. This will be left on the stack
  // as the value of the whole expression.
  value_t value = get_variable_assignment_ast_value(self);
  TRY(emit_value(value, assm));
  // Then load the reference to store the value in.
  value_t variable = get_variable_assignment_ast_target(self);
  CHECK_FAMILY(ofLocalVariableAst, variable);
  value_t symbol = get_local_variable_ast_symbol(variable);
  bool is_ref = false;
  TRY(assembler_access_symbol(symbol, assm, &is_ref));
  CHECK_TRUE("assigning immutable", is_ref);
  TRY(assembler_emit_set_reference(assm));
  return success();
}


// --- L o c a l   v a r i a b l e ---

GET_FAMILY_PRIMARY_TYPE_IMPL(local_variable_ast);
NO_BUILTIN_METHODS(local_variable_ast);

ACCESSORS_IMPL(LocalVariableAst, local_variable_ast, acInFamilyOpt, ofSymbolAst,
    Symbol, symbol);

value_t emit_local_variable_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofLocalVariableAst, self);
  value_t symbol = get_local_variable_ast_symbol(self);
  TRY(assembler_load_symbol(symbol, assm));
  return success();
}

value_t local_variable_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLocalVariableAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_local_variable_ast_symbol(self));
  return success();
}

void local_variable_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<local variable ast: ");
  value_print_inner_on(get_local_variable_ast_symbol(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_local_variable_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol);
  set_local_variable_ast_symbol(object, symbol_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_local_variable_ast(runtime_t *runtime) {
  return new_heap_local_variable_ast(runtime, afMutable, nothing());
}


// --- N a m e s p a c e   v a r i a b l e ---

GET_FAMILY_PRIMARY_TYPE_IMPL(namespace_variable_ast);
NO_BUILTIN_METHODS(namespace_variable_ast);
TRIVIAL_PRINT_ON_IMPL(NamespaceVariableAst, namespace_variable_ast);

ACCESSORS_IMPL(NamespaceVariableAst, namespace_variable_ast, acInFamilyOpt,
    ofIdentifier, Identifier, identifier);

value_t emit_namespace_variable_ast(value_t self, assembler_t *assm) {
  value_t ident = get_namespace_variable_ast_identifier(self);
  value_t target = get_module_fragment_predecessor_at(assm->fragment,
      get_identifier_stage(ident));
  value_t path = get_identifier_path(ident);
  assembler_emit_load_global(assm, path, target);
  return success();
}

value_t namespace_variable_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespaceVariableAst, self);
  return success();
}

value_t plankton_set_namespace_variable_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_namespace_variable_ast_identifier(object, name_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_namespace_variable_ast(runtime_t *runtime) {
  return new_heap_namespace_variable_ast(runtime, afMutable, nothing());
}


// --- S y m b o l ---

GET_FAMILY_PRIMARY_TYPE_IMPL(symbol_ast);
NO_BUILTIN_METHODS(symbol_ast);

ACCESSORS_IMPL(SymbolAst, symbol_ast, acNoCheck, 0, Name, name);
ACCESSORS_IMPL(SymbolAst, symbol_ast, acNoCheck, 0, Origin, origin);

value_t symbol_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofSymbolAst, self);
  return success();
}

void symbol_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<symbol ast: ");
  value_print_inner_on(get_symbol_ast_name(value), context, -1);
  string_buffer_printf(context->buf, ">");
}

value_t plankton_set_symbol_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name, origin);
  set_symbol_ast_name(object, name_value);
  set_symbol_ast_origin(object, origin_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_symbol_ast(runtime_t *runtime) {
  return new_heap_symbol_ast(runtime, afMutable, nothing(), nothing());
}


// --- L a m b d a   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(lambda_ast);
NO_BUILTIN_METHODS(lambda_ast);
TRIVIAL_PRINT_ON_IMPL(LambdaAst, lambda_ast);

ACCESSORS_IMPL(LambdaAst, lambda_ast, acInFamilyOpt, ofArray, Methods,
    methods);

value_t quick_and_dirty_evaluate_syntax(runtime_t *runtime, value_t fragment,
    value_t value_ast) {
  switch (get_heap_object_family(value_ast)) {
    case ofLiteralAst:
      return get_literal_ast_value(value_ast);
    case ofNamespaceVariableAst: {
      value_t ident = get_namespace_variable_ast_identifier(value_ast);
      value_t target = get_module_fragment_predecessor_at(fragment,
          get_identifier_stage(ident));
      return module_fragment_lookup_path_full(runtime, target, get_identifier_path(ident));
    }
    default:
      ERROR("Quick-and-dirty evaluation doesn't work for %v", value_ast);
      return new_invalid_input_condition();
  }
}

value_t build_method_signature(runtime_t *runtime, value_t fragment,
    reusable_scratch_memory_t *scratch, value_t signature_ast) {
  value_t param_asts = get_signature_ast_parameters(signature_ast);
  size_t param_astc = get_array_length(param_asts);

  // Calculate the parameter ordering. Note that we're assuming that this will
  // give the same order as the signature, which got its order from a different
  // call to call_parameter_ast_ordering with the same (though possibly
  // relocated) parameter array. This seems like a safe assumption though it
  // does rely on qsort being well-behaved.
  size_t *offsets = calc_parameter_ast_ordering(scratch, param_asts);

  // Count the tags. We'll need those for the compiled method signature's tag
  // vector.
  size_t tag_count = 0;
  for (size_t i = 0; i < param_astc; i++) {
    value_t param = get_array_at(param_asts, i);
    value_t tags = get_parameter_ast_tags(param);
    tag_count += get_array_length(tags);
  }

  TRY_DEF(tag_array, new_heap_pair_array(runtime, tag_count));

  // Build the tag vector of the signature. Tag_index counts the total number of
  // tags seen so far across all parameters.
  for (size_t i = 0, tag_index = 0; i < param_astc; i++) {
    // Add the parameter to the signature.
    value_t param_ast = get_array_at(param_asts, i);
    value_t guard_ast = get_parameter_ast_guard(param_ast);
    value_t guard = evaluate_guard(runtime, guard_ast, fragment);
    size_t param_index = offsets[i];
    value_t tags = get_parameter_ast_tags(param_ast);
    TRY_DEF(param, new_heap_parameter(runtime, afFreeze, guard, tags, false, param_index));
    // Add all this parameter's tags to the tag array.
    size_t tagc = get_array_length(tags);
    for (size_t j = 0; j < tagc; j++, tag_index++) {
      value_t tag = get_array_at(tags, j);
      set_pair_array_first_at(tag_array, tag_index, tag);
      set_pair_array_second_at(tag_array, tag_index, param);
    }
  }
  co_sort_pair_array(tag_array);

  bool allow_extra = get_boolean_value(get_signature_ast_allow_extra(signature_ast));
  // Build the result signature and store it in the out param.
  TRY_DEF(result, new_heap_signature(runtime, afFreeze, tag_array, param_astc,
      param_astc, allow_extra));

  return result;
}

static value_t sanity_check_symbol(assembler_t *assm, value_t probably_symbol) {
  if (!in_family(ofSymbolAst, probably_symbol))
    return new_invalid_syntax_condition(isExpectedSymbol);
  if (assembler_is_symbol_bound(assm, probably_symbol))
    // We're trying to redefine an already defined symbol. That's not valid.
    return new_invalid_syntax_condition(isSymbolAlreadyBound);
  return success();
}

value_t compile_method_body(assembler_t *assm, value_t method_ast) {
  CHECK_FAMILY(ofMethodAst, method_ast);

  value_t signature_ast = get_method_ast_signature(method_ast);
  runtime_t *runtime = assm->runtime;
  value_t param_asts = get_signature_ast_parameters(signature_ast);
  size_t param_astc = get_array_length(param_asts);

  // Push the scope that holds the parameters.
  map_scope_o param_scope;
  TRY(assembler_push_map_scope(assm, &param_scope));

  // Calculate the parameter ordering. The offsets array will only be valid
  // until the next ordering call so don't do any recursive emit calls while
  // using it.
  size_t *offsets = calc_parameter_ast_ordering(assembler_get_scratch_memory(assm),
      param_asts);

  // Build the tag vector of the signature. Tag_index counts the total number of
  // tags seen so far across all parameters.
  for (size_t i = 0; i < param_astc; i++) {
    // Bind the parameter in the local scope.
    value_t param_ast = get_array_at(param_asts, i);
    value_t symbol = get_parameter_ast_symbol(param_ast);
    TRY(sanity_check_symbol(assm, symbol));
    TRY(map_scope_bind(&param_scope, symbol, btArgument, offsets[i]));
  }

  value_t reified_symbol = get_signature_ast_reified(signature_ast);
  value_t reify_params = nothing();
  if (!is_nothing(reified_symbol)) {
    TRY(sanity_check_symbol(assm, reified_symbol));
    TRY(map_scope_bind(&param_scope, reified_symbol, btLocal, 0));
    TRY_SET(reify_params, new_heap_array(runtime, param_astc));
    for (size_t i = 0; i < param_astc; i++)
      set_array_at(reify_params, offsets[i], get_array_at(param_asts, i));
  }

  // We don't need this more so clear it to ensure that we don't accidentally
  // access it memory again.
  offsets = NULL;

  // Compile the code.
  value_t body_ast = get_method_ast_body(method_ast);
  TRY_DEF(result, compile_expression(runtime, body_ast, assm->fragment,
      assm->scope, reify_params));
  assembler_pop_map_scope(assm, &param_scope);
  return result;
}

value_t emit_lambda_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofLambdaAst, value);
  value_t method_asts = get_lambda_ast_methods(value);

  // Push a capture scope that captures any symbols accessed outside the lambda.
  lambda_scope_o lambda_scope;
  TRY(assembler_push_lambda_scope(assm, &lambda_scope));

  TRY_DEF(space, build_methodspace_from_method_asts(method_asts, assm));

  // Pop the capturing scope off, we're done capturing.
  assembler_pop_lambda_scope(assm, &lambda_scope);

  // Push the captured variables onto the stack so they can to be stored in the
  // lambda.
  value_t captures = lambda_scope.captures;
  size_t capture_count = get_array_buffer_length(captures);
  for (size_t i = 0; i < capture_count; i++)
    // Push the captured symbols onto the stack in reverse order just to make
    // it simpler to pop them into the capture array at runtime. It makes no
    // difference, loading a symbol has no side-effects.
    //
    // For mutable variables this will push the reference, not the value, which
    // is what we want. Reading and writing will work as expected because
    // captured or not the symbol knows if it's a value or a reference.
    assembler_access_symbol(get_array_buffer_at(captures, capture_count - i - 1),
        assm, NULL);

  // Finally emit the bytecode that will create the lambda.
  TRY(assembler_emit_lambda(assm, space, capture_count));
  return success();
}

value_t lambda_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLambdaAst, self);
  VALIDATE_FAMILY_OPT(ofArray, get_lambda_ast_methods(self));
  return success();
}

value_t plankton_set_lambda_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, methods);
  set_lambda_ast_methods(object, methods_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_lambda_ast(runtime_t *runtime) {
  return new_heap_lambda_ast(runtime, afMutable, nothing());
}


// --- P a r a m e t e r   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(parameter_ast);
NO_BUILTIN_METHODS(parameter_ast);

ACCESSORS_IMPL(ParameterAst, parameter_ast, acInFamilyOpt, ofSymbolAst, Symbol,
    symbol);
ACCESSORS_IMPL(ParameterAst, parameter_ast, acInFamilyOpt, ofArray, Tags, tags);
ACCESSORS_IMPL(ParameterAst, parameter_ast, acInFamilyOpt, ofGuardAst, Guard, guard);

value_t parameter_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofParameterAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_parameter_ast_symbol(self));
  VALIDATE_FAMILY_OPT(ofArray, get_parameter_ast_tags(self));
  VALIDATE_FAMILY_OPT(ofGuardAst, get_parameter_ast_guard(self));
  return success();
}

value_t plankton_set_parameter_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol, tags, guard);
  set_parameter_ast_symbol(object, symbol_value);
  set_parameter_ast_tags(object, tags_value);
  set_parameter_ast_guard(object, guard_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_parameter_ast(runtime_t *runtime) {
  return new_heap_parameter_ast(runtime, afMutable, nothing(), nothing(),
      nothing());
}

void parameter_ast_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofParameterAst, self);
  value_t guard = get_parameter_ast_guard(self);
  string_buffer_printf(context->buf, "#<parameter ast ");
  value_print_inner_on(guard, context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- G u a r d   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(guard_ast);
NO_BUILTIN_METHODS(guard_ast);

ENUM_ACCESSORS_IMPL(GuardAst, guard_ast, guard_type_t, Type, type);
ACCESSORS_IMPL(GuardAst, guard_ast, acNoCheck, 0, Value, value);

value_t guard_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofGuardAst, self);
  return success();
}

value_t plankton_set_guard_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, type, value);
  guard_type_t type_enum;
  // Maybe passing an integer enum will be good enough? Or does that conflict
  // with being self-describing?
  EXPECT_FAMILY(ccInvalidInput, ofUtf8, type_value);
  char type_char = get_utf8_chars(type_value)[0];
  switch (type_char) {
    case '=': type_enum = gtEq; break;
    case 'i': type_enum = gtIs; break;
    case '*': type_enum = gtAny; break;
    default: return new_invalid_input_condition();
  }
  set_guard_ast_type(object, type_enum);
  set_guard_ast_value(object, value_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_guard_ast(runtime_t *runtime) {
  return new_heap_guard_ast(runtime, afMutable, gtAny, nothing());
}

void guard_ast_print_on(value_t self, print_on_context_t *context) {
  CHECK_FAMILY(ofGuardAst, self);
  switch (get_guard_ast_type(self)) {
    case gtEq:
      string_buffer_printf(context->buf, "eq(");
      value_print_inner_on(get_guard_ast_value(self), context, -1);
      string_buffer_printf(context->buf, ")");
      break;
    case gtIs:
      string_buffer_printf(context->buf, "is(");
      value_print_inner_on(get_guard_ast_value(self), context, -1);
      string_buffer_printf(context->buf, ")");
      break;
    case gtAny:
      string_buffer_printf(context->buf, "any()");
      break;
  }
}


// --- S i g n a t u r e   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(signature_ast);
NO_BUILTIN_METHODS(signature_ast);

ACCESSORS_IMPL(SignatureAst, signature_ast, acInFamilyOpt, ofArray,
    Parameters, parameters);
ACCESSORS_IMPL(SignatureAst, signature_ast, acNoCheck, 0, AllowExtra,
    allow_extra);
ACCESSORS_IMPL(SignatureAst, signature_ast, acInFamilyOpt, ofSymbolAst,
    Reified, reified);

value_t signature_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofSignatureAst, self);
  VALIDATE_FAMILY_OPT(ofArray, get_signature_ast_parameters(self));
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_signature_ast_reified(self));
  return success();
}

value_t plankton_set_signature_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, parameters, allow_extra, reified);
  set_signature_ast_parameters(object, parameters_value);
  set_signature_ast_allow_extra(object, allow_extra_value);
  set_signature_ast_reified(object, null_to_nothing(reified_value));
  return ensure_frozen(runtime, object);
}

value_t plankton_new_signature_ast(runtime_t *runtime) {
  return new_heap_signature_ast(runtime, afMutable, nothing(), nothing(),
      nothing());
}

void signature_ast_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<signature ast ");
  value_t params = get_signature_ast_parameters(self);
  for (size_t i = 0; i < get_array_length(params); i++) {
    if (i > 0)
      string_buffer_printf(context->buf, ", ");
    value_print_inner_on(get_array_at(params, i), context, -1);
  }
  string_buffer_printf(context->buf, ">");
}


// --- M e t h o d   a s t ---

GET_FAMILY_PRIMARY_TYPE_IMPL(method_ast);
NO_BUILTIN_METHODS(method_ast);

ACCESSORS_IMPL(MethodAst, method_ast, acInFamilyOpt, ofSignatureAst, Signature,
    signature);
ACCESSORS_IMPL(MethodAst, method_ast, acIsSyntaxOpt, 0, Body, body);

value_t method_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofMethodAst, self);
  VALIDATE_FAMILY_OPT(ofSignatureAst, get_method_ast_signature(self));
  return success();
}

value_t plankton_set_method_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, signature, body);
  set_method_ast_signature(object, signature_value);
  set_method_ast_body(object, body_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_method_ast(runtime_t *runtime) {
  return new_heap_method_ast(runtime, afMutable, nothing(), nothing());
}

void method_ast_print_on(value_t self, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<method ast ");
  value_t signature = get_method_ast_signature(self);
  value_print_inner_on(signature, context, -1);
  string_buffer_printf(context->buf, " ");
  value_t body = get_method_ast_body(self);
  value_print_inner_on(body, context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- N a m e s p a c e   d e c l a r a t i o n   a s t ---

ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    acInFamilyOpt, ofArray, Annotations, annotations);
ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    acIsSyntaxOpt, 0, Value, value);

value_t namespace_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespaceDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofArray, get_namespace_declaration_ast_annotations(self));
  VALIDATE_FAMILY_OPT(ofPath, get_namespace_declaration_ast_path(self));
  return success();
}

value_t plankton_set_namespace_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, path, value, annotations);
  set_namespace_declaration_ast_annotations(object, annotations_value);
  set_namespace_declaration_ast_path(object, path_value);
  set_namespace_declaration_ast_value(object, value_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_namespace_declaration_ast(runtime_t *runtime) {
  return new_heap_namespace_declaration_ast(runtime, afMutable, nothing(),
      nothing(), nothing());
}

void namespace_declaration_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<def ");
  value_print_inner_on(get_namespace_declaration_ast_path(value), context, -1);
  string_buffer_printf(context->buf, " := ");
  value_print_inner_on(get_namespace_declaration_ast_value(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- M e t h o d   d e c l a r a t i o n   a s t ---

ACCESSORS_IMPL(MethodDeclarationAst, method_declaration_ast, acInFamilyOpt,
    ofArray, Annotations, annotations);
ACCESSORS_IMPL(MethodDeclarationAst, method_declaration_ast,
    acInFamilyOpt, ofMethodAst, Method, method);

value_t method_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofMethodDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofArray, get_method_declaration_ast_annotations(self));
  VALIDATE_FAMILY_OPT(ofMethodAst, get_method_declaration_ast_method(self));
  return success();
}

value_t plankton_set_method_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, method, annotations);
  set_method_declaration_ast_annotations(object, annotations_value);
  set_method_declaration_ast_method(object, method_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_method_declaration_ast(runtime_t *runtime) {
  return new_heap_method_declaration_ast(runtime, afMutable, nothing(), nothing());
}

void method_declaration_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<def ");
  value_print_inner_on(get_method_declaration_ast_method(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- I s   d e c l a r a t i o n   a s t ---

ACCESSORS_IMPL(IsDeclarationAst, is_declaration_ast, acIsSyntaxOpt, 0, Subtype,
    subtype);
ACCESSORS_IMPL(IsDeclarationAst, is_declaration_ast, acIsSyntaxOpt, 0, Supertype,
    supertype);

value_t is_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofIsDeclarationAst, self);
  return success();
}

value_t plankton_set_is_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, subtype, supertype);
  set_is_declaration_ast_subtype(object, subtype_value);
  set_is_declaration_ast_supertype(object, supertype_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_is_declaration_ast(runtime_t *runtime) {
  return new_heap_is_declaration_ast(runtime, afMutable, nothing(), nothing());
}

void is_declaration_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<type ");
  value_print_inner_on(get_is_declaration_ast_subtype(value), context, -1);
  string_buffer_printf(context->buf, " is ");
  value_print_inner_on(get_is_declaration_ast_supertype(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- P r o g r a m   a s t ---

ACCESSORS_IMPL(ProgramAst, program_ast, acIsSyntaxOpt, 0, EntryPoint, entry_point);
ACCESSORS_IMPL(ProgramAst, program_ast, acNoCheck, 0, Module, module);

value_t program_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofProgramAst, self);
  return success();
}

value_t plankton_set_program_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, entry_point, module);
  set_program_ast_entry_point(object, entry_point_value);
  set_program_ast_module(object, module_value);
  return ensure_frozen(runtime, object);
}

value_t plankton_new_program_ast(runtime_t *runtime) {
  return new_heap_program_ast(runtime, afMutable, nothing(), nothing());
}

void program_ast_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "#<program ast: ");
  value_print_inner_on(get_program_ast_entry_point(value), context, -1);
  string_buffer_printf(context->buf, " ");
  value_print_inner_on(get_program_ast_module(value), context, -1);
  string_buffer_printf(context->buf, ">");
}


// --- C u r r e n t   m o d u l e   a s t ---

FIXED_GET_MODE_IMPL(current_module_ast, vmDeepFrozen);
TRIVIAL_PRINT_ON_IMPL(CurrentModuleAst, current_module_ast);

value_t current_module_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofCurrentModuleAst, self);
  return success();
}

value_t plankton_new_current_module_ast(runtime_t *runtime) {
  return new_heap_current_module_ast(runtime);
}

value_t plankton_set_current_module_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  // Current module asts have no fields.
  return success();
}

value_t emit_current_module_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofCurrentModuleAst, value);
  return assembler_emit_push(assm, get_module_fragment_private(assm->fragment));
}


// --- C o d e   g e n e r a t i o n ---

value_t emit_value(value_t value, assembler_t *assm) {
  if (!in_domain(vdHeapObject, value))
    return new_invalid_syntax_condition(isNotSyntax);
  switch (get_heap_object_family(value)) {
#define __EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family)                     \
    case of##Family:                                                           \
      return emit_##family(value, assm);
#define __EMIT_SYNTAX_FAMILY_CASE__(Family, family, MD, SR, MINOR, N)\
    mfEm MINOR (                                                               \
      __EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family),                      \
      )
    ENUM_HEAP_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_CASE__)
#undef __EMIT_SYNTAX_FAMILY_CASE__
#undef __EMIT_SYNTAX_FAMILY_CASE_HELPER__
    default:
      return new_invalid_syntax_condition(isNotSyntax);
  }
}


// --- F a c t o r i e s ---

value_t init_plankton_syntax_factories(value_t map, runtime_t *runtime) {
  ADD_PLANKTON_FACTORY(map, "ast:Argument", argument_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Array", array_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Block", block_ast);
  ADD_PLANKTON_FACTORY(map, "ast:CallLiteral", call_literal_ast);
  ADD_PLANKTON_FACTORY(map, "ast:CallLiteralArgument", call_literal_argument_ast);
  ADD_PLANKTON_FACTORY(map, "ast:CurrentModule", current_module_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Ensure", ensure_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Guard", guard_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Invocation", invocation_ast);
  ADD_PLANKTON_FACTORY(map, "ast:IsDeclaration", is_declaration_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Lambda", lambda_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Literal", literal_ast);
  ADD_PLANKTON_FACTORY(map, "ast:LocalDeclaration", local_declaration_ast);
  ADD_PLANKTON_FACTORY(map, "ast:LocalVariable", local_variable_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Method", method_ast);
  ADD_PLANKTON_FACTORY(map, "ast:MethodDeclaration", method_declaration_ast);
  ADD_PLANKTON_FACTORY(map, "ast:NamespaceDeclaration", namespace_declaration_ast);
  ADD_PLANKTON_FACTORY(map, "ast:NamespaceVariable", namespace_variable_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Parameter", parameter_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Program", program_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Sequence", sequence_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Signal", signal_ast);
  ADD_PLANKTON_FACTORY(map, "ast:SignalHandler", signal_handler_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Signature", signature_ast);
  ADD_PLANKTON_FACTORY(map, "ast:Symbol", symbol_ast);
  ADD_PLANKTON_FACTORY(map, "ast:VariableAssignment", variable_assignment_ast);
  ADD_PLANKTON_FACTORY(map, "ast:WithEscape", with_escape_ast);
  return success();
}
