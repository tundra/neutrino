// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "log.h"
#include "syntax.h"
#include "try-inl.h"
#include "value-inl.h"

// --- M i s c ---

static value_t resolve_syntax_factory(value_t key, runtime_t *runtime, void *data) {
  value_t result = get_id_hash_map_at(ROOT(runtime, plankton_environment), key);
  if (is_signal(scNotFound, result)) {
    value_to_string_t buf;
    WARN("Environment reference %s could not be resolved", value_to_string(&buf, key));
    dispose_value_to_string(&buf);
  }
  return result;
}

value_t init_plankton_environment_mapping(value_mapping_t *mapping,
    runtime_t *runtime) {
  value_mapping_init(mapping, resolve_syntax_factory, NULL);
  return success();
}

value_t compile_expression(runtime_t *runtime, value_t program,
    scope_lookup_callback_t *scope_callback) {
  assembler_t assm;
  // Don't try to execute cleanup if this fails since there'll not be an
  // assembler to dispose.
  TRY(assembler_init(&assm, runtime, scope_callback));
  E_BEGIN_TRY_FINALLY();
    E_TRY(emit_value(program, &assm));
    assembler_emit_return(&assm);
    E_TRY_DEF(code_block, assembler_flush(&assm));
    E_RETURN(code_block);
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

// Forward declare all the emit methods.
#define __EMIT_SYNTAX_FAMILY_EMIT__(Family, family, CMP, CID, CNT, SUR, NOL, FIX, EMT)\
    EMT(value_t emit_##family(value_t, assembler_t *);,)
    ENUM_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_EMIT__)
#undef __EMIT_SYNTAX_FAMILY_EMIT__


// --- L i t e r a l ---

GET_FAMILY_PROTOCOL_IMPL(literal_ast);
NO_BUILTIN_METHODS(literal_ast);

UNCHECKED_ACCESSORS_IMPL(LiteralAst, literal_ast, Value, value);

value_t literal_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLiteralAst, value);
  return success();
}

void literal_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<literal ast: ");
  value_print_atomic_on(get_literal_ast_value(value), buf);
  string_buffer_printf(buf, ">");
}

void literal_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<literal ast>");
}

static value_t new_literal_ast(runtime_t *runtime) {
  return new_heap_literal_ast(runtime, ROOT(runtime, null));
}

value_t set_literal_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(value, get_id_hash_map_at(contents, RSTR(runtime, value)));
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

CHECKED_ACCESSORS_IMPL(ArrayAst, array_ast, Array, Elements, elements);

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
  VALIDATE_VALUE_FAMILY(ofArrayAst, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_array_ast_elements(value));
  return success();
}

void array_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<array ast: ");
  value_print_atomic_on(get_array_ast_elements(value), buf);
  string_buffer_printf(buf, ">");
}

void array_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<array ast>");
}

value_t set_array_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(elements, get_id_hash_map_at(contents, RSTR(runtime, elements)));
  set_array_ast_elements(object, elements);
  return success();
}

static value_t new_array_ast(runtime_t *runtime) {
  return new_heap_array_ast(runtime, ROOT(runtime, empty_array));
}


// --- I n v o c a t i o n ---

TRIVIAL_PRINT_ON_IMPL(InvocationAst, invocation_ast);
GET_FAMILY_PROTOCOL_IMPL(invocation_ast);
NO_BUILTIN_METHODS(invocation_ast);

CHECKED_ACCESSORS_IMPL(InvocationAst, invocation_ast, Array, Arguments, arguments);
UNCHECKED_ACCESSORS_IMPL(InvocationAst, invocation_ast, Methodspace, methodspace);

value_t emit_invocation_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofInvocationAst, value);
  value_t arguments = get_invocation_ast_arguments(value);
  value_t methodspace = get_invocation_ast_methodspace(value);
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
  TRY_DEF(record, new_heap_invocation_record(assm->runtime, arg_vector));
  TRY(assembler_emit_invocation(assm, methodspace, record));
  return success();
}

value_t invocation_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofInvocationAst, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_invocation_ast_arguments(value));
  return success();
}

value_t set_invocation_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(arguments, get_id_hash_map_at(contents, RSTR(runtime, arguments)));
  TRY_DEF(methodspace, get_id_hash_map_at(contents, RSTR(runtime, methodspace)));
  set_invocation_ast_arguments(object, arguments);
  set_invocation_ast_methodspace(object, methodspace);
  return success();
}

static value_t new_invocation_ast(runtime_t *runtime) {
  return new_heap_invocation_ast(runtime, ROOT(runtime, empty_array),
      ROOT(runtime, null));
}


// --- A r g u m e n t ---

TRIVIAL_PRINT_ON_IMPL(ArgumentAst, argument_ast);
GET_FAMILY_PROTOCOL_IMPL(argument_ast);
NO_BUILTIN_METHODS(argument_ast);

UNCHECKED_ACCESSORS_IMPL(ArgumentAst, argument_ast, Tag, tag);
UNCHECKED_ACCESSORS_IMPL(ArgumentAst, argument_ast, Value, value);

value_t argument_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArgumentAst, value);
  return success();
}

value_t set_argument_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(tag, get_id_hash_map_at(contents, RSTR(runtime, tag)));
  TRY_DEF(value, get_id_hash_map_at(contents, RSTR(runtime, value)));
  set_argument_ast_tag(object, tag);
  set_argument_ast_value(object, value);
  return success();
}

static value_t new_argument_ast(runtime_t *runtime) {
  return new_heap_argument_ast(runtime, ROOT(runtime, null), ROOT(runtime, null));
}


// --- S e q u e n c e ---

GET_FAMILY_PROTOCOL_IMPL(sequence_ast);
NO_BUILTIN_METHODS(sequence_ast);

CHECKED_ACCESSORS_IMPL(SequenceAst, sequence_ast, Array, Values, values);

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
  VALIDATE_VALUE_FAMILY(ofSequenceAst, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_sequence_ast_values(value));
  return success();
}

void sequence_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<sequence ast: ");
  value_print_atomic_on(get_sequence_ast_values(value), buf);
  string_buffer_printf(buf, ">");
}

void sequence_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<sequence ast>");
}

value_t set_sequence_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(values, get_id_hash_map_at(contents, RSTR(runtime, values)));
  set_sequence_ast_values(object, values);
  return success();
}

static value_t new_sequence_ast(runtime_t *runtime) {
  return new_heap_sequence_ast(runtime, ROOT(runtime, empty_array));
}


// --- L o c a l   d e c l a r a t i o n ---

GET_FAMILY_PROTOCOL_IMPL(local_declaration_ast);
NO_BUILTIN_METHODS(local_declaration_ast);

UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Symbol, symbol);
UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Value, value);
UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Body, body);

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

value_t local_declaration_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLocalDeclarationAst, value);
  return success();
}

void local_declaration_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<local declaration ast: ");
  value_print_atomic_on(get_local_declaration_ast_symbol(value), buf);
  string_buffer_printf(buf, " := ");
  value_print_atomic_on(get_local_declaration_ast_value(value), buf);
  string_buffer_printf(buf, " in ");
  value_print_atomic_on(get_local_declaration_ast_body(value), buf);
  string_buffer_printf(buf, ">");
}

void local_declaration_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<local declaration ast>");
}

value_t set_local_declaration_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(symbol, get_id_hash_map_at(contents, RSTR(runtime, symbol)));
  TRY_DEF(value, get_id_hash_map_at(contents, RSTR(runtime, value)));
  TRY_DEF(body, get_id_hash_map_at(contents, RSTR(runtime, body)));
  set_local_declaration_ast_symbol(object, symbol);
  set_local_declaration_ast_value(object, value);
  set_local_declaration_ast_body(object, body);
  return success();
}

static value_t new_local_declaration_ast(runtime_t *runtime) {
  value_t null = ROOT(runtime, null);
  return new_heap_local_declaration_ast(runtime, null, null, null);
}


// --- L o c a l   v a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(local_variable_ast);
NO_BUILTIN_METHODS(local_variable_ast);

UNCHECKED_ACCESSORS_IMPL(LocalVariableAst, local_variable_ast, Symbol, symbol);

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

value_t local_variable_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLocalVariableAst, value);
  return success();
}

void local_variable_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<local variable ast: ");
  value_print_atomic_on(get_local_variable_ast_symbol(value), buf);
  string_buffer_printf(buf, ">");
}

void local_variable_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<local variable ast>");
}

value_t set_local_variable_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(symbol, get_id_hash_map_at(contents, RSTR(runtime, symbol)));
  set_local_variable_ast_symbol(object, symbol);
  return success();
}

static value_t new_local_variable_ast(runtime_t *runtime) {
  return new_heap_local_variable_ast(runtime, ROOT(runtime, null));
}


// --- N a m e s p a c e   v a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(namespace_variable_ast);
NO_BUILTIN_METHODS(namespace_variable_ast);
TRIVIAL_PRINT_ON_IMPL(NamespaceVariableAst, namespace_variable_ast);

UNCHECKED_ACCESSORS_IMPL(NamespaceVariableAst, namespace_variable_ast, Name, name);
UNCHECKED_ACCESSORS_IMPL(NamespaceVariableAst, namespace_variable_ast, Namespace, namespace);

value_t emit_namespace_variable_ast(value_t self, assembler_t *assm) {
  assembler_emit_load_global(assm, get_namespace_variable_ast_name(self),
      get_namespace_variable_ast_namespace(self));
  return success();
}

value_t namespace_variable_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNamespaceVariableAst, value);
  return success();
}

value_t set_namespace_variable_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(name, get_id_hash_map_at(contents, RSTR(runtime, name)));
  TRY_DEF(namespace, get_id_hash_map_at(contents, RSTR(runtime, namespace)));
  set_namespace_variable_ast_name(object, name);
  set_namespace_variable_ast_namespace(object, namespace);
  return success();
}

static value_t new_namespace_variable_ast(runtime_t *runtime) {
  return new_heap_namespace_variable_ast(runtime, ROOT(runtime, null),
      ROOT(runtime, null));
}


// --- S y m b o l ---

GET_FAMILY_PROTOCOL_IMPL(symbol_ast);
NO_BUILTIN_METHODS(symbol_ast);

UNCHECKED_ACCESSORS_IMPL(SymbolAst, symbol_ast, Name, name);

value_t symbol_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofSymbolAst, value);
  return success();
}

void symbol_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<symbol ast: ");
  value_print_atomic_on(get_symbol_ast_name(value), buf);
  string_buffer_printf(buf, ">");
}

void symbol_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<symbol ast>");
}

value_t set_symbol_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(name, get_id_hash_map_at(contents, RSTR(runtime, name)));
  set_symbol_ast_name(object, name);
  return success();
}

static value_t new_symbol_ast(runtime_t *runtime) {
  return new_heap_symbol_ast(runtime, ROOT(runtime, null));
}


// --- L a m b d a ---

GET_FAMILY_PROTOCOL_IMPL(lambda_ast);
NO_BUILTIN_METHODS(lambda_ast);
TRIVIAL_PRINT_ON_IMPL(LambdaAst, lambda_ast);

UNCHECKED_ACCESSORS_IMPL(LambdaAst, lambda_ast, Parameters, parameters);
UNCHECKED_ACCESSORS_IMPL(LambdaAst, lambda_ast, Body, body);

value_t emit_lambda_ast(value_t value, assembler_t *assm) {
  // Emitting a lambda takes a fair amount of code but most of it is
  // straightforward -- it's more just verbose than actually complex.
  CHECK_FAMILY(ofLambdaAst, value);
  runtime_t *runtime = assm->runtime;

  // Build the signature. First part is the standard preamble: subject and
  // selector.
  value_t params = get_lambda_ast_parameters(value);
  size_t implicit_argc = 2;
  size_t explicit_argc = get_array_length(params);
  size_t total_argc = implicit_argc + explicit_argc;
  TRY_DEF(vector, new_heap_pair_array(runtime, total_argc));
  set_pair_array_first_at(vector, 0, ROOT(runtime, subject_key));
  value_t any_guard = ROOT(runtime, any_guard);
  TRY_DEF(subject_param, new_heap_parameter(runtime, any_guard, false, 0));
  set_pair_array_second_at(vector, 0, subject_param);
  set_pair_array_first_at(vector, 1, ROOT(runtime, selector_key));
  TRY_DEF(selector_guard, new_heap_guard(runtime, gtEq, RSTR(runtime, sausages)));
  TRY_DEF(selector_param, new_heap_parameter(runtime, selector_guard, false, 1));
  set_pair_array_second_at(vector, 1, selector_param);

  // Push a capture scope that captures any symbols accessed outside the lambda.
  capture_scope_t capture_scope;
  TRY(assembler_push_capture_scope(assm, &capture_scope));
  // Push the scope that holds the parameters. We could in principle use just a
  // single scope that does both but this way is cleaner and allows the two
  // parts to be used independently from each other. Also, we don't really need
  // the scopes until the part where the method space is built but it's
  // convenient to have the map scope to we can add stuff to it when going
  // through the parameters.
  map_scope_t param_scope;
  TRY(assembler_push_map_scope(assm, &param_scope));

  // Build the positional argument part of the signature.
  for (size_t i = 0; i < explicit_argc; i++) {
    // Add the parameter to the signature.
    size_t param_index = implicit_argc + i;
    value_t param_ast = get_array_at(params, i);
    value_t tags = get_parameter_ast_tags(param_ast);
    CHECK_EQ("multi tags", 1, get_array_length(tags));
    value_t tag = get_array_at(tags, 0);
    set_pair_array_first_at(vector, param_index, tag);
    TRY_DEF(param, new_heap_parameter(runtime, any_guard, false, param_index));
    set_pair_array_second_at(vector, param_index, param);
    // Bind the parameter in the local scope.
    value_t symbol = get_parameter_ast_symbol(param_ast);
    if (!in_family(ofSymbolAst, symbol))
      return new_invalid_syntax_signal(isExpectedSymbol);
    if (assembler_is_symbol_bound(assm, symbol))
      // We're trying to redefine an already defined symbol. That's not valid.
      return new_invalid_syntax_signal(isSymbolAlreadyBound);
    TRY(map_scope_bind(&param_scope, symbol, btArgument, param_index));
  }
  co_sort_pair_array(vector);
  TRY_DEF(sig, new_heap_signature(runtime, vector, total_argc, total_argc, false));

  // Build the method space.
  TRY_DEF(space, new_heap_methodspace(runtime));
  value_t body = get_lambda_ast_body(value);
  TRY_DEF(body_code, compile_expression(runtime, body, assm->scope_callback));
  TRY_DEF(method, new_heap_method(runtime, sig, body_code));
  TRY(add_methodspace_method(runtime, space, method));

  // Pop the two scopes back off since we're done compiling the body.
  assembler_pop_map_scope(assm, &param_scope);
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
  TRY(assembler_emit_lambda(assm, space, capture_count));

  return success();
}

value_t lambda_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLambdaAst, value);
  return success();
}

value_t set_lambda_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(parameters, get_id_hash_map_at(contents, RSTR(runtime, parameters)));
  TRY_DEF(body, get_id_hash_map_at(contents, RSTR(runtime, body)));
  set_lambda_ast_parameters(object, parameters);
  set_lambda_ast_body(object, body);
  return success();
}

static value_t new_lambda_ast(runtime_t *runtime) {
  return new_heap_lambda_ast(runtime, ROOT(runtime, null), ROOT(runtime, null));
}


// --- P a r a m e t e r ---

GET_FAMILY_PROTOCOL_IMPL(parameter_ast);
NO_BUILTIN_METHODS(parameter_ast);
TRIVIAL_PRINT_ON_IMPL(ParameterAst, parameter_ast);

UNCHECKED_ACCESSORS_IMPL(ParameterAst, parameter_ast, Symbol, symbol);
CHECKED_ACCESSORS_IMPL(ParameterAst, parameter_ast, Array, Tags, tags);

value_t parameter_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofParameterAst, value);
  return success();
}

value_t set_parameter_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(symbol, get_id_hash_map_at(contents, RSTR(runtime, symbol)));
  TRY_DEF(tags, get_id_hash_map_at(contents, RSTR(runtime, tags)));
  set_parameter_ast_symbol(object, symbol);
  set_parameter_ast_tags(object, tags);
  return success();
}

static value_t new_parameter_ast(runtime_t *runtime) {
  return new_heap_parameter_ast(runtime, ROOT(runtime, null),
      ROOT(runtime, empty_array));
}


// --- P r o g r a m ---

TRIVIAL_PRINT_ON_IMPL(ProgramAst, program_ast);

CHECKED_ACCESSORS_IMPL(ProgramAst, program_ast, Array, Elements, elements);
UNCHECKED_ACCESSORS_IMPL(ProgramAst, program_ast, EntryPoint, entry_point);
UNCHECKED_ACCESSORS_IMPL(ProgramAst, program_ast, Namespace, namespace);
UNCHECKED_ACCESSORS_IMPL(ProgramAst, program_ast, Methodspace, methodspace);

value_t program_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofProgramAst, value);
  return success();
}

value_t set_program_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(elements, get_id_hash_map_at(contents, RSTR(runtime, elements)));
  TRY_DEF(entry_point, get_id_hash_map_at(contents, RSTR(runtime, entry_point)));
  TRY_DEF(namespace, get_id_hash_map_at(contents, RSTR(runtime, namespace)));
  TRY_DEF(methodspace, get_id_hash_map_at(contents, RSTR(runtime, methodspace)));
  set_program_ast_elements(object, elements);
  set_program_ast_entry_point(object, entry_point);
  set_program_ast_namespace(object, namespace);
  set_program_ast_methodspace(object, methodspace);
  return success();
}

static value_t new_program_ast(runtime_t *runtime) {
  return new_heap_program_ast(runtime, ROOT(runtime, empty_array),
      ROOT(runtime, null), ROOT(runtime, empty_array), ROOT(runtime, null));
}


// --- N a m e s p a c e   d e c l a r a t i o n ---

TRIVIAL_PRINT_ON_IMPL(NamespaceDeclarationAst, namespace_declaration_ast);

UNCHECKED_ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    Name, name);
UNCHECKED_ACCESSORS_IMPL(NamespaceDeclarationAst, namespace_declaration_ast,
    Value, value);

value_t namespace_declaration_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNamespaceDeclarationAst, value);
  return success();
}

value_t set_namespace_declaration_ast_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(name, get_id_hash_map_at(contents, RSTR(runtime, name)));
  TRY_DEF(value, get_id_hash_map_at(contents, RSTR(runtime, value)));
  set_namespace_declaration_ast_name(object, name);
  set_namespace_declaration_ast_value(object, value);
  return success();
}

static value_t new_namespace_declaration_ast(runtime_t *runtime) {
  value_t null = ROOT(runtime, null);
  return new_heap_namespace_declaration_ast(runtime, null, null);
}


// --- N a m e ---

TRIVIAL_PRINT_ON_IMPL(NameAst, name_ast);

UNCHECKED_ACCESSORS_IMPL(NameAst, name_ast, Path, path);
UNCHECKED_ACCESSORS_IMPL(NameAst, name_ast, Phase, phase);

value_t name_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofNameAst, value);
  return success();
}

value_t set_name_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(path, get_id_hash_map_at(contents, RSTR(runtime, path)));
  TRY_DEF(phase, get_id_hash_map_at(contents, RSTR(runtime, phase)));
  set_name_ast_path(object, path);
  set_name_ast_phase(object, phase);
  return success();
}

static value_t new_name_ast(runtime_t *runtime) {
  value_t null = ROOT(runtime, null);
  return new_heap_name_ast(runtime, null, null);
}


// --- C o d e   g e n e r a t i o n ---

value_t emit_value(value_t value, assembler_t *assm) {
  if (!in_domain(vdObject, value))
    return new_invalid_syntax_signal(isNotSyntax);
  switch (get_object_family(value)) {
#define __EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family)                     \
    case of##Family:                                                           \
      return emit_##family(value, assm);
#define __EMIT_SYNTAX_FAMILY_CASE__(Family, family, CMP, CID, CNT, SUR, NOL, FIX, EMT)\
    EMT(__EMIT_SYNTAX_FAMILY_CASE_HELPER__(Family, family),)
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
  TRY(add_plankton_factory(map, ast, "Argument", new_argument_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Array", new_array_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Invocation", new_invocation_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Lambda", new_lambda_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Literal", new_literal_ast, runtime));
  TRY(add_plankton_factory(map, ast, "LocalDeclaration", new_local_declaration_ast, runtime));
  TRY(add_plankton_factory(map, ast, "LocalVariable", new_local_variable_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Name", new_name_ast, runtime));
  TRY(add_plankton_factory(map, ast, "NamespaceDeclaration", new_namespace_declaration_ast, runtime));
  TRY(add_plankton_factory(map, ast, "NamespaceVariable", new_namespace_variable_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Parameter", new_parameter_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Program", new_program_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Sequence", new_sequence_ast, runtime));
  TRY(add_plankton_factory(map, ast, "Symbol", new_symbol_ast, runtime));
  return success();
}
