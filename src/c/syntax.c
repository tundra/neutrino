// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "log.h"
#include "runtime-inl.h"
#include "syntax.h"
#include "try-inl.h"
#include "utils-inl.h"
#include "value-inl.h"


// --- M i s c ---

static value_t resolve_syntax_factory(value_t key, runtime_t *runtime, void *data) {
  value_t result = get_id_hash_map_at(ROOT(runtime, plankton_environment), key);
  if (is_signal(scNotFound, result)) {
    return new_heap_unknown(runtime, RSTR(runtime, environment_reference), key);
  } else {
    return result;
  }
}

value_t init_plankton_environment_mapping(value_mapping_t *mapping,
    runtime_t *runtime) {
  value_mapping_init(mapping, resolve_syntax_factory, NULL);
  return success();
}

value_t compile_expression(runtime_t *runtime, value_t program,
    value_t fragment, scope_lookup_callback_t *scope_callback) {
  assembler_t assm;
  // Don't try to execute cleanup if this fails since there'll not be an
  // assembler to dispose.
  TRY(assembler_init(&assm, runtime, fragment, scope_callback));
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(code_block, compile_expression_with_assembler(runtime, program,
        &assm));
    E_RETURN(code_block);
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

value_t compile_expression_with_assembler(runtime_t *runtime, value_t program,
    assembler_t *assm) {
  TRY(emit_value(program, assm));
  assembler_emit_return(assm);
  TRY_DEF(code_block, assembler_flush(assm));
  return code_block;
}

value_t safe_compile_expression(runtime_t *runtime, safe_value_t ast,
    safe_value_t module, scope_lookup_callback_t *scope_callback) {
  RETRY_ONCE_IMPL(runtime, compile_expression(runtime, deref(ast), deref(module),
      scope_callback));
}

size_t get_parameter_order_index_for_array(value_t tags) {
  size_t result = kMaxOrderIndex;
  for (size_t i = 0; i < get_array_length(tags); i++) {
    value_t tag = get_array_at(tags, i);
    if (in_domain(vdInteger, tag)) {
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
#define __EMIT_SYNTAX_FAMILY_EMIT__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)\
    EM(                                                                            \
      value_t emit_##family(value_t, assembler_t *);,                              \
      )
    ENUM_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_EMIT__)
#undef __EMIT_SYNTAX_FAMILY_EMIT__

// --- L i t e r a l ---

GET_FAMILY_PROTOCOL_IMPL(literal_ast);
NO_BUILTIN_METHODS(literal_ast);
FIXED_GET_MODE_IMPL(literal_ast, vmMutable);

ACCESSORS_IMPL(LiteralAst, literal_ast, acNoCheck, 0, Value, value);

value_t literal_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLiteralAst, self);
  return success();
}

void literal_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<");
  value_print_inner_on(get_literal_ast_value(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_new_literal_ast(runtime_t *runtime) {
  return new_heap_literal_ast(runtime, ROOT(runtime, null));
}

value_t plankton_set_literal_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, value);
  set_literal_ast_value(object, value);
  return success();
}

value_t emit_literal_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofLiteralAst, value);
  return assembler_emit_push(assm, get_literal_ast_value(value));
}


// --- A r r a y ---

GET_FAMILY_PROTOCOL_IMPL(array_ast);
NO_BUILTIN_METHODS(array_ast);
FIXED_GET_MODE_IMPL(array_ast, vmMutable);

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

void array_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<array ast: ");
  value_print_inner_on(get_array_ast_elements(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_array_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, elements);
  set_array_ast_elements(object, elements);
  return success();
}

value_t plankton_new_array_ast(runtime_t *runtime) {
  return new_heap_array_ast(runtime, ROOT(runtime, nothing));
}


// --- I n v o c a t i o n ---

TRIVIAL_PRINT_ON_IMPL(InvocationAst, invocation_ast);
GET_FAMILY_PROTOCOL_IMPL(invocation_ast);
NO_BUILTIN_METHODS(invocation_ast);
FIXED_GET_MODE_IMPL(invocation_ast, vmMutable);

ACCESSORS_IMPL(InvocationAst, invocation_ast, acInFamilyOpt, ofArray, Arguments,
    arguments);

value_t emit_invocation_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofInvocationAst, value);
  value_t arguments = get_invocation_ast_arguments(value);
  value_t methodspace = get_module_fragment_methodspace(assm->fragment);
  CHECK_FAMILY(ofMethodspace, methodspace);
  size_t arg_count = get_array_length(arguments);
  // Build the invocation record and emit the values at the same time.
  TRY_DEF(arg_vector, new_heap_pair_array(assm->runtime, arg_count));
  for (size_t i = 0; i < arg_count; i++) {
    value_t argument = get_array_at(arguments, i);
    // Add the tag to the invocation record.
    value_t tag = get_argument_ast_tag(argument);
    set_pair_array_first_at(arg_vector, i, tag);
    set_pair_array_second_at(arg_vector, i, new_integer(arg_count - i - 1));
    // Emit the value.
    value_t value = get_argument_ast_value(argument);
    TRY(emit_value(value, assm));
  }
  TRY(co_sort_pair_array(arg_vector));
  TRY_DEF(record, new_heap_invocation_record(assm->runtime, afFreeze, arg_vector));
  TRY(assembler_emit_invocation(assm, methodspace, record));
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
  set_invocation_ast_arguments(object, arguments);
  return success();
}

value_t plankton_new_invocation_ast(runtime_t *runtime) {
  return new_heap_invocation_ast(runtime, ROOT(runtime, nothing));
}


// --- A r g u m e n t ---

TRIVIAL_PRINT_ON_IMPL(ArgumentAst, argument_ast);
GET_FAMILY_PROTOCOL_IMPL(argument_ast);
NO_BUILTIN_METHODS(argument_ast);
FIXED_GET_MODE_IMPL(argument_ast, vmMutable);

ACCESSORS_IMPL(ArgumentAst, argument_ast, acNoCheck, 0, Tag, tag);
ACCESSORS_IMPL(ArgumentAst, argument_ast, acIsSyntaxOpt, 0, Value, value);

value_t argument_ast_validate(value_t value) {
  VALIDATE_FAMILY(ofArgumentAst, value);
  return success();
}

value_t plankton_set_argument_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, tag, value);
  set_argument_ast_tag(object, tag);
  set_argument_ast_value(object, value);
  return success();
}

value_t plankton_new_argument_ast(runtime_t *runtime) {
  return new_heap_argument_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}


// --- S e q u e n c e ---

GET_FAMILY_PROTOCOL_IMPL(sequence_ast);
NO_BUILTIN_METHODS(sequence_ast);
FIXED_GET_MODE_IMPL(sequence_ast, vmMutable);

ACCESSORS_IMPL(SequenceAst, sequence_ast, acInFamily, ofArray, Values, values);

value_t emit_sequence_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofSequenceAst, value);
  value_t values = get_sequence_ast_values(value);
  size_t length = get_array_length(values);
  if (length == 0) {
    // A no-element sequence has value null.
    TRY(assembler_emit_push(assm, ROOT(assm->runtime, null)));
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

void sequence_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<sequence ast: ");
  value_print_inner_on(get_sequence_ast_values(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_sequence_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, values);
  set_sequence_ast_values(object, values);
  return success();
}

value_t plankton_new_sequence_ast(runtime_t *runtime) {
  return new_heap_sequence_ast(runtime, ROOT(runtime, empty_array));
}


// --- L o c a l   d e c l a r a t i o n ---

GET_FAMILY_PROTOCOL_IMPL(local_declaration_ast);
NO_BUILTIN_METHODS(local_declaration_ast);
FIXED_GET_MODE_IMPL(local_declaration_ast, vmMutable);

ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acInFamilyOpt,
    ofSymbolAst, Symbol, symbol);
ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acIsSyntaxOpt,
    0, Value, value);
ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, acIsSyntaxOpt,
    0, Body, body);

value_t emit_local_declaration_ast(value_t self, assembler_t *assm) {
  CHECK_FAMILY(ofLocalDeclarationAst, self);
  size_t offset = assm->stack_height;
  value_t value = get_local_declaration_ast_value(self);
  TRY(emit_value(value, assm));
  value_t symbol = get_local_declaration_ast_symbol(self);
  CHECK_FAMILY(ofSymbolAst, symbol);
  if (assembler_is_symbol_bound(assm, symbol))
    // We're trying to redefine an already defined symbol. That's not valid.
    return new_invalid_syntax_signal(isSymbolAlreadyBound);
  single_symbol_scope_t scope;
  assembler_push_single_symbol_scope(assm, &scope, symbol, btLocal, offset);
  value_t body = get_local_declaration_ast_body(self);
  TRY(emit_value(body, assm));
  assembler_pop_single_symbol_scope(assm, &scope);
  return success();
}

value_t local_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLocalDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofSymbolAst, get_local_declaration_ast_symbol(self));
  return success();
}

void local_declaration_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<local declaration ast: ");
  value_print_inner_on(get_local_declaration_ast_symbol(value), buf, flags,
      depth - 1);
  string_buffer_printf(buf, " := ");
  value_print_inner_on(get_local_declaration_ast_value(value), buf, flags,
      depth - 1);
  string_buffer_printf(buf, " in ");
  value_print_inner_on(get_local_declaration_ast_body(value), buf, flags,
      depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_local_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol, value, body);
  set_local_declaration_ast_symbol(object, symbol);
  set_local_declaration_ast_value(object, value);
  set_local_declaration_ast_body(object, body);
  return success();
}

value_t plankton_new_local_declaration_ast(runtime_t *runtime) {
  return new_heap_local_declaration_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing), ROOT(runtime, nothing));
}


// --- L o c a l   v a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(local_variable_ast);
NO_BUILTIN_METHODS(local_variable_ast);
FIXED_GET_MODE_IMPL(local_variable_ast, vmMutable);

ACCESSORS_IMPL(LocalVariableAst, local_variable_ast, acInFamilyOpt, ofSymbolAst,
    Symbol, symbol);

// Pushes the value of a symbol onto the stack.
static value_t assembler_load_symbol(value_t symbol, assembler_t *assm) {
  CHECK_FAMILY(ofSymbolAst, symbol);
  binding_info_t binding;
  if (is_signal(scNotFound, assembler_lookup_symbol(assm, symbol, &binding)))
    // We're trying to access a symbol that hasn't been defined here. That's
    // not valid.
    return new_invalid_syntax_signal(isSymbolNotBound);
  switch (binding.type) {
    case btLocal:
      TRY(assembler_emit_load_local(assm, binding.data));
      break;
    case btArgument:
      TRY(assembler_emit_load_argument(assm, binding.data));
      break;
    case btCaptured:
      TRY(assembler_emit_load_outer(assm, binding.data));
      break;
    default:
      WARN("Unknown binding type %i", binding.type);
      UNREACHABLE("unknown binding type");
      break;
  }
  return success();
}

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

void local_variable_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<local variable ast: ");
  value_print_inner_on(get_local_variable_ast_symbol(value), buf, flags,
      depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_local_variable_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, symbol);
  set_local_variable_ast_symbol(object, symbol);
  return success();
}

value_t plankton_new_local_variable_ast(runtime_t *runtime) {
  return new_heap_local_variable_ast(runtime, ROOT(runtime, nothing));
}


// --- N a m e s p a c e   v a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(namespace_variable_ast);
NO_BUILTIN_METHODS(namespace_variable_ast);
TRIVIAL_PRINT_ON_IMPL(NamespaceVariableAst, namespace_variable_ast);
FIXED_GET_MODE_IMPL(namespace_variable_ast, vmMutable);

ACCESSORS_IMPL(NamespaceVariableAst, namespace_variable_ast, acInFamilyOpt,
    ofIdentifier, Identifier, identifier);

value_t emit_namespace_variable_ast(value_t self, assembler_t *assm) {
  assembler_emit_load_global(assm, get_namespace_variable_ast_identifier(self),
      get_module_fragment_module(assm->fragment));
  return success();
}

value_t namespace_variable_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespaceVariableAst, self);
  return success();
}

value_t plankton_set_namespace_variable_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_namespace_variable_ast_identifier(object, name);
  return success();
}

value_t plankton_new_namespace_variable_ast(runtime_t *runtime) {
  return new_heap_namespace_variable_ast(runtime, ROOT(runtime, nothing));
}


// --- S y m b o l ---

GET_FAMILY_PROTOCOL_IMPL(symbol_ast);
NO_BUILTIN_METHODS(symbol_ast);
FIXED_GET_MODE_IMPL(symbol_ast, vmMutable);

ACCESSORS_IMPL(SymbolAst, symbol_ast, acNoCheck, 0, Name, name);

value_t symbol_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofSymbolAst, self);
  return success();
}

void symbol_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<symbol ast: ");
  value_print_inner_on(get_symbol_ast_name(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}

value_t plankton_set_symbol_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, name);
  set_symbol_ast_name(object, name);
  return success();
}

value_t plankton_new_symbol_ast(runtime_t *runtime) {
  return new_heap_symbol_ast(runtime, ROOT(runtime, nothing));
}


// --- L a m b d a   a s t ---

GET_FAMILY_PROTOCOL_IMPL(lambda_ast);
NO_BUILTIN_METHODS(lambda_ast);
TRIVIAL_PRINT_ON_IMPL(LambdaAst, lambda_ast);
FIXED_GET_MODE_IMPL(lambda_ast, vmMutable);

ACCESSORS_IMPL(LambdaAst, lambda_ast, acInFamilyOpt, ofMethodAst, Method,
    method);

static value_t quick_and_dirty_evaluate_syntax(runtime_t *runtime,
    value_t fragment, value_t value_ast) {
  switch (get_object_family(value_ast)) {
    case ofLiteralAst:
      return get_literal_ast_value(value_ast);
    case ofNamespaceVariableAst: {
      value_t ident = get_namespace_variable_ast_identifier(value_ast);
      value_t module = get_module_fragment_module(fragment);
      return module_lookup_identifier(runtime, module, ident);
    }
    default:
      ERROR("Cannot evaluate guard value %v", value_ast);
      return new_invalid_input_signal();
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
    guard_type_t guard_type = get_guard_ast_type(guard_ast);
    value_t guard;
    if (guard_type == gtAny) {
      guard = ROOT(runtime, any_guard);
    } else {
      value_t guard_value_ast = get_guard_ast_value(guard_ast);
      TRY_DEF(guard_value, quick_and_dirty_evaluate_syntax(runtime, fragment,
          guard_value_ast));
      TRY_SET(guard, new_heap_guard(runtime, afFreeze, guard_type,
          guard_value));
    }
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

  // Build the result signature and store it in the out param.
  TRY_DEF(result, new_heap_signature(runtime, afFreeze, tag_array, param_astc,
      param_astc, false));

  return result;
}

value_t compile_method_body(assembler_t *assm, value_t method_ast) {
  CHECK_FAMILY(ofMethodAst, method_ast);

  value_t signature_ast = get_method_ast_signature(method_ast);
  runtime_t *runtime = assm->runtime;
  value_t param_asts = get_signature_ast_parameters(signature_ast);
  size_t param_astc = get_array_length(param_asts);

  // Push the scope that holds the parameters.
  map_scope_t param_scope;
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
    if (!in_family(ofSymbolAst, symbol))
      return new_invalid_syntax_signal(isExpectedSymbol);
    if (assembler_is_symbol_bound(assm, symbol))
      // We're trying to redefine an already defined symbol. That's not valid.
      return new_invalid_syntax_signal(isSymbolAlreadyBound);
    TRY(map_scope_bind(&param_scope, symbol, btArgument, offsets[i]));
  }

  // We don't need this more so clear it to ensure that we don't accidentally
  // access it memory again.
  offsets = NULL;

  // Compile the code.
  value_t body_ast = get_method_ast_body(method_ast);
  TRY_DEF(result, compile_expression(runtime, body_ast, assm->fragment,
      assm->scope_callback));
  assembler_pop_map_scope(assm, &param_scope);
  return result;
}

value_t emit_lambda_ast(value_t value, assembler_t *assm) {
  // Emitting a lambda takes a fair amount of code but most of it is
  // straightforward -- it's more just verbose than actually complex.
  CHECK_FAMILY(ofLambdaAst, value);

  // Push a capture scope that captures any symbols accessed outside the lambda.
  capture_scope_t capture_scope;
  TRY(assembler_push_capture_scope(assm, &capture_scope));

  runtime_t *runtime = assm->runtime;

  // Compile the signature and, if we're in a nontrivial inner scope, the
  // body of the lambda.
  value_t method_ast = get_lambda_ast_method(value);
  value_t body_code = ROOT(runtime, nothing);
  if (assm->scope_callback != scope_lookup_callback_get_bottom())
    TRY_SET(body_code, compile_method_body(assm, method_ast));
  TRY_DEF(signature, build_method_signature(assm->runtime, assm->fragment,
      assembler_get_scratch_memory(assm), get_method_ast_signature(method_ast)));

  // Build a method space in which to store the method.
  TRY_DEF(method, new_heap_method(runtime, afFreeze, signature,
      ROOT(runtime, nothing), body_code, ROOT(runtime, nothing)));
  TRY_DEF(space, new_heap_methodspace(runtime));
  TRY(add_methodspace_method(runtime, space, method));

  // Pop the capturing scope off, we're done capturing.
  assembler_pop_capture_scope(assm, &capture_scope);

  // Push the captured variables onto the stack so they can to be stored in the
  // lambda.
  value_t captures = capture_scope.captures;
  size_t capture_count = get_array_buffer_length(captures);
  for (size_t i = 0; i < capture_count; i++)
    // Push the captured symbols onto the stack in reverse order just to make
    // it simpler to pop them into the capture array at runtime. It makes no
    // difference, loading a symbol has no side-effects.
    assembler_load_symbol(get_array_buffer_at(captures, capture_count - i - 1),
        assm);

  // Finally emit the bytecode that will create the lambda.
  TRY(assembler_emit_lambda(assm, space, capture_count));
  return success();
}

value_t lambda_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofLambdaAst, self);
  VALIDATE_FAMILY_OPT(ofMethodAst, get_lambda_ast_method(self));
  return success();
}

value_t plankton_set_lambda_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, method);
  set_lambda_ast_method(object, method);
  return success();
}

value_t plankton_new_lambda_ast(runtime_t *runtime) {
  return new_heap_lambda_ast(runtime, ROOT(runtime, nothing));
}


// --- P a r a m e t e r   a s t ---

GET_FAMILY_PROTOCOL_IMPL(parameter_ast);
NO_BUILTIN_METHODS(parameter_ast);
FIXED_GET_MODE_IMPL(parameter_ast, vmMutable);

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
  set_parameter_ast_symbol(object, symbol);
  set_parameter_ast_tags(object, tags);
  set_parameter_ast_guard(object, guard);
  return success();
}

value_t plankton_new_parameter_ast(runtime_t *runtime) {
  return new_heap_parameter_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing), ROOT(runtime, nothing));
}

void parameter_ast_print_on(value_t self, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  CHECK_FAMILY(ofParameterAst, self);
  value_t guard = get_parameter_ast_guard(self);
  string_buffer_printf(buf, "#<parameter ast ");
  value_print_inner_on(guard, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- G u a r d   a s t ---

GET_FAMILY_PROTOCOL_IMPL(guard_ast);
NO_BUILTIN_METHODS(guard_ast);
FIXED_GET_MODE_IMPL(guard_ast, vmMutable);

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
  EXPECT_FAMILY(scInvalidInput, ofString, type);
  char type_char = get_string_chars(type)[0];
  switch (type_char) {
    case '=': type_enum = gtEq; break;
    case 'i': type_enum = gtIs; break;
    case '*': type_enum = gtAny; break;
    default: return new_invalid_input_signal();
  }
  set_guard_ast_type(object, type_enum);
  set_guard_ast_value(object, value);
  return success();
}

value_t plankton_new_guard_ast(runtime_t *runtime) {
  return new_heap_guard_ast(runtime, gtAny, ROOT(runtime, nothing));
}

void guard_ast_print_on(value_t self, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  CHECK_FAMILY(ofGuardAst, self);
  switch (get_guard_ast_type(self)) {
    case gtEq:
      string_buffer_printf(buf, "eq(");
      value_print_inner_on(get_guard_ast_value(self), buf, flags, depth - 1);
      string_buffer_printf(buf, ")");
      break;
    case gtIs:
      string_buffer_printf(buf, "is(");
      value_print_inner_on(get_guard_ast_value(self), buf, flags, depth - 1);
      string_buffer_printf(buf, ")");
      break;
    case gtAny:
      string_buffer_printf(buf, "any()");
      break;
  }
}


// --- S i g n a t u r e   a s t ---

GET_FAMILY_PROTOCOL_IMPL(signature_ast);
NO_BUILTIN_METHODS(signature_ast);
FIXED_GET_MODE_IMPL(signature_ast, vmMutable);

ACCESSORS_IMPL(SignatureAst, signature_ast, acInFamilyOpt, ofArray,
    Parameters, parameters);

value_t signature_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofSignatureAst, self);
  VALIDATE_FAMILY_OPT(ofArray, get_signature_ast_parameters(self));
  return success();
}

value_t plankton_set_signature_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, parameters);
  set_signature_ast_parameters(object, parameters);
  return success();
}

value_t plankton_new_signature_ast(runtime_t *runtime) {
  return new_heap_signature_ast(runtime, ROOT(runtime, nothing));
}

void signature_ast_print_on(value_t self, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<signature ast ");
  value_t params = get_signature_ast_parameters(self);
  for (size_t i = 0; i < get_array_length(params); i++) {
    if (i > 0)
      string_buffer_printf(buf, ", ");
    value_print_inner_on(get_array_at(params, i), buf, flags, depth - 1);
  }
  string_buffer_printf(buf, ">");
}


// --- M e t h o d   a s t ---

GET_FAMILY_PROTOCOL_IMPL(method_ast);
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
  set_method_ast_signature(object, signature);
  set_method_ast_body(object, body);
  return success();
}

value_t plankton_new_method_ast(runtime_t *runtime) {
  return new_heap_method_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}

void method_ast_print_on(value_t self, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<method ast ");
  value_t signature = get_method_ast_signature(self);
  value_print_inner_on(signature, buf, flags, depth - 1);
  string_buffer_printf(buf, " ");
  value_t body = get_method_ast_body(self);
  value_print_inner_on(body, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- N a m e s p a c e   d e c l a r a t i o n   a s t ---

FIXED_GET_MODE_IMPL(namespace_declaration_ast, vmMutable);

ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    acIsSyntaxOpt, 0, Value, value);

value_t namespace_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofNamespaceDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofPath, get_namespace_declaration_ast_path(self));
  return success();
}

value_t plankton_set_namespace_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, path, value);
  set_namespace_declaration_ast_path(object, path);
  set_namespace_declaration_ast_value(object, value);
  return success();
}

value_t plankton_new_namespace_declaration_ast(runtime_t *runtime) {
  return new_heap_namespace_declaration_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}

void namespace_declaration_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<def ");
  value_print_inner_on(get_namespace_declaration_ast_path(value), buf, flags, depth - 1);
  string_buffer_printf(buf, " := ");
  value_print_inner_on(get_namespace_declaration_ast_value(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- M e t h o d   d e c l a r a t i o n   a s t ---

FIXED_GET_MODE_IMPL(method_declaration_ast, vmMutable);

ACCESSORS_IMPL(MethodDeclarationAst, method_declaration_ast,
    acInFamilyOpt, ofMethodAst, Method, method);

value_t method_declaration_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofMethodDeclarationAst, self);
  VALIDATE_FAMILY_OPT(ofMethodAst, get_method_declaration_ast_method(self));
  return success();
}

value_t plankton_set_method_declaration_ast_contents(value_t object,
    runtime_t *runtime, value_t contents) {
  UNPACK_PLANKTON_MAP(contents, method);
  set_method_declaration_ast_method(object, method);
  return success();
}

value_t plankton_new_method_declaration_ast(runtime_t *runtime) {
  return new_heap_method_declaration_ast(runtime, ROOT(runtime, nothing));
}

void method_declaration_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<def ");
  value_print_inner_on(get_method_declaration_ast_method(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- P r o g r a m   a s t ---

FIXED_GET_MODE_IMPL(program_ast, vmMutable);

ACCESSORS_IMPL(ProgramAst, program_ast, acIsSyntaxOpt, 0, EntryPoint, entry_point);
ACCESSORS_IMPL(ProgramAst, program_ast, acNoCheck, 0, Module, module);

value_t program_ast_validate(value_t self) {
  VALIDATE_FAMILY(ofProgramAst, self);
  return success();
}

value_t plankton_set_program_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, entry_point, module);
  set_program_ast_entry_point(object, entry_point);
  set_program_ast_module(object, module);
  return success();
}

value_t plankton_new_program_ast(runtime_t *runtime) {
  return new_heap_program_ast(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}

void program_ast_print_on(value_t value, string_buffer_t *buf,
    print_flags_t flags, size_t depth) {
  string_buffer_printf(buf, "#<program ast: ");
  value_print_inner_on(get_program_ast_entry_point(value), buf, flags, depth - 1);
  string_buffer_printf(buf, " ");
  value_print_inner_on(get_program_ast_module(value), buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- C o d e   g e n e r a t i o n ---

value_t emit_value(value_t value, assembler_t *assm) {
  if (!in_domain(vdObject, value))
    return new_invalid_syntax_signal(isNotSyntax);
  switch (get_object_family(value)) {
#define __EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family)                     \
    case of##Family:                                                           \
      return emit_##family(value, assm);
#define __EMIT_SYNTAX_FAMILY_CASE__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)\
    EM(                                                                        \
      __EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family),                      \
      )
    ENUM_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_CASE__)
#undef __EMIT_SYNTAX_FAMILY_CASE__
#undef __EMIT_SYNTAX_FAMILY_CASE_HELPER__
    default:
      return new_invalid_syntax_signal(isNotSyntax);
  }
}


// --- F a c t o r i e s ---

value_t init_plankton_syntax_factories(value_t map, runtime_t *runtime) {
  value_t ast = RSTR(runtime, ast);
  TRY(add_plankton_factory(map, ast, "Argument", plankton_new_argument_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Array", plankton_new_array_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Guard", plankton_new_guard_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Invocation", plankton_new_invocation_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Lambda", plankton_new_lambda_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Literal", plankton_new_literal_ast, runtime));
  TRY(add_plankton_factory(map, ast, "LocalDeclaration", plankton_new_local_declaration_ast, runtime));
  TRY(add_plankton_factory(map, ast, "LocalVariable", plankton_new_local_variable_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Method", plankton_new_method_ast, runtime));
  TRY(add_plankton_factory(map, ast, "MethodDeclaration", plankton_new_method_declaration_ast, runtime));
  TRY(add_plankton_factory(map, ast, "NamespaceDeclaration", plankton_new_namespace_declaration_ast, runtime));
  TRY(add_plankton_factory(map, ast, "NamespaceVariable", plankton_new_namespace_variable_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Parameter", plankton_new_parameter_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Program", plankton_new_program_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Sequence", plankton_new_sequence_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Signature", plankton_new_signature_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Symbol", plankton_new_symbol_ast, runtime));
  return success();
}
