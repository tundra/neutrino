// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "log.h"
#include "syntax.h"
#include "value-inl.h"

// --- M i s c ---

static value_t resolve_syntax_factory(value_t key, runtime_t *runtime, void *data) {
  // We only accept keys of the form ["ast", <name>] so first we check that the
  // key does indeed have that shape.
  if (get_object_family(key) != ofArray || get_array_length(key) != 2)
    return new_signal(scNothing);

  // Check that the first element is the string "ast".
  value_t first_obj = get_array_at(key, 0);
  if (get_object_family(first_obj) != ofString)
    return new_signal(scNothing);
  string_t first;
  get_string_contents(first_obj, &first);
  if (!string_equals_cstr(&first, "ast"))
    return new_signal(scNothing);

  // Check that the second element is a string and look it up if it is.
  value_t second_obj = get_array_at(key, 1);
  if (get_object_family(second_obj) != ofString)
    return new_signal(scNothing);

  value_t result = get_id_hash_map_at(runtime->roots.syntax_factories, second_obj);
  if (is_signal(scNotFound, result)) {
    value_to_string_t buf;
    WARN("Syntax factory %s could not be resolved", value_to_string(&buf, key));
    dispose_value_to_string(&buf);
  }
  return result;
}

// Initialize a value mapping such that it maps syntax constructors to syntax
// value factories from the given runtime.
value_t init_syntax_mapping(value_mapping_t *mapping, runtime_t *runtime) {
  value_mapping_init(mapping, resolve_syntax_factory, NULL);
  return success();
}

value_t compile_syntax(runtime_t *runtime, value_t program, value_t space) {
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, space));
  TRY(emit_value(program, &assm));
  assembler_emit_return(&assm);
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  return code_block;
}


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
  return new_heap_literal_ast(runtime, runtime->roots.null);
}

value_t set_literal_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(value, get_id_hash_map_at(contents, runtime->roots.string_table.value));
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
  TRY_DEF(elements, get_id_hash_map_at(contents, runtime->roots.string_table.elements));
  set_array_ast_elements(object, elements);
  return success();
}

static value_t new_array_ast(runtime_t *runtime) {
  return new_heap_array_ast(runtime, runtime->roots.empty_array);
}


// --- I n v o c a t i o n ---

TRIVIAL_PRINT_ON_IMPL(InvocationAst, invocation_ast);
GET_FAMILY_PROTOCOL_IMPL(invocation_ast);
NO_BUILTIN_METHODS(invocation_ast);

CHECKED_ACCESSORS_IMPL(InvocationAst, invocation_ast, Array, Arguments, arguments);

value_t emit_invocation_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofInvocationAst, value);
  value_t arguments = get_invocation_ast_arguments(value);
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
  TRY(assembler_emit_invocation(assm, assm->space, record));
  return success();
}

value_t invocation_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofInvocationAst, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_invocation_ast_arguments(value));
  return success();
}

value_t set_invocation_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(arguments, get_id_hash_map_at(contents, runtime->roots.string_table.arguments));
  set_invocation_ast_arguments(object, arguments);
  return success();
}

static value_t new_invocation_ast(runtime_t *runtime) {
  return new_heap_invocation_ast(runtime, runtime->roots.empty_array);
}


// --- A r g u m e n t ---

TRIVIAL_PRINT_ON_IMPL(ArgumentAst, argument_ast);
GET_FAMILY_PROTOCOL_IMPL(argument_ast);
NO_BUILTIN_METHODS(argument_ast);

UNCHECKED_ACCESSORS_IMPL(ArgumentAst, argument_ast, Tag, tag);
UNCHECKED_ACCESSORS_IMPL(ArgumentAst, argument_ast, Value, value);

value_t emit_argument_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofArgumentAst, value);
  UNREACHABLE("emitting argument ast");
  return success();
}

value_t argument_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofArgumentAst, value);
  return success();
}

value_t set_argument_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(tag, get_id_hash_map_at(contents, runtime->roots.string_table.tag));
  TRY_DEF(value, get_id_hash_map_at(contents, runtime->roots.string_table.value));
  set_argument_ast_tag(object, tag);
  set_argument_ast_value(object, value);
  return success();
}

static value_t new_argument_ast(runtime_t *runtime) {
  return new_heap_argument_ast(runtime, runtime_null(runtime), runtime_null(runtime));
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
    TRY(assembler_emit_push(assm, runtime_null(assm->runtime)));
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
  TRY_DEF(values, get_id_hash_map_at(contents, runtime->roots.string_table.values));
  set_sequence_ast_values(object, values);
  return success();
}

static value_t new_sequence_ast(runtime_t *runtime) {
  return new_heap_sequence_ast(runtime, runtime->roots.empty_array);
}


// --- L o c a l   d e c l a r a t i o n ---

GET_FAMILY_PROTOCOL_IMPL(local_declaration_ast);
NO_BUILTIN_METHODS(local_declaration_ast);

UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Symbol, symbol);
UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Value, value);
UNCHECKED_ACCESSORS_IMPL(LocalDeclarationAst, local_declaration_ast, Body, body);

value_t emit_local_declaration_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofLocalDeclarationAst, value);
  UNREACHABLE("emitting local declaration");
  return new_signal(scUnsupportedBehavior);
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
  TRY_DEF(symbol, get_id_hash_map_at(contents, runtime->roots.string_table.symbol));
  TRY_DEF(value, get_id_hash_map_at(contents, runtime->roots.string_table.value));
  TRY_DEF(body, get_id_hash_map_at(contents, runtime->roots.string_table.body));
  set_local_declaration_ast_symbol(object, symbol);
  set_local_declaration_ast_value(object, value);
  set_local_declaration_ast_body(object, body);
  return success();
}

static value_t new_local_declaration_ast(runtime_t *runtime) {
  value_t null = runtime_null(runtime);
  return new_heap_local_declaration_ast(runtime, null, null, null);
}


// --- V a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(variable_ast);
NO_BUILTIN_METHODS(variable_ast);

UNCHECKED_ACCESSORS_IMPL(VariableAst, variable_ast, Symbol, symbol);

value_t emit_variable_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofVariableAst, value);
  UNREACHABLE("emitting variable ast");
  return new_signal(scUnsupportedBehavior);
}

value_t variable_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofVariableAst, value);
  return success();
}

void variable_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<variable ast: ");
  value_print_atomic_on(get_variable_ast_symbol(value), buf);
  string_buffer_printf(buf, ">");
}

void variable_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<variable ast>");
}

value_t set_variable_ast_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(symbol, get_id_hash_map_at(contents, runtime->roots.string_table.symbol));
  set_variable_ast_symbol(object, symbol);
  return success();
}

static value_t new_variable_ast(runtime_t *runtime) {
  return new_heap_variable_ast(runtime, runtime_null(runtime));
}


// --- V a r i a b l e ---

GET_FAMILY_PROTOCOL_IMPL(symbol_ast);
NO_BUILTIN_METHODS(symbol_ast);

UNCHECKED_ACCESSORS_IMPL(SymbolAst, symbol_ast, Name, name);

value_t emit_symbol_ast(value_t value, assembler_t *assm) {
  CHECK_FAMILY(ofSymbolAst, value);
  UNREACHABLE("emitting symbol as ast");
  return new_signal(scUnsupportedBehavior);
}

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
  TRY_DEF(name, get_id_hash_map_at(contents, runtime->roots.string_table.name));
  set_symbol_ast_name(object, name);
  return success();
}

static value_t new_symbol_ast(runtime_t *runtime) {
  return new_heap_symbol_ast(runtime, runtime_null(runtime));
}


// --- C o d e   g e n e r a t i o n ---

value_t emit_value(value_t value, assembler_t *assm) {
  if (!in_domain(vdObject, value))
    return new_signal(scInvalidSyntax);
  switch (get_object_family(value)) {
#define __EMIT_SYNTAX_FAMILY_CASE__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)\
    case of##Family:                                                           \
      return emit_##family(value, assm);
    ENUM_SYNTAX_OBJECT_FAMILIES(__EMIT_SYNTAX_FAMILY_CASE__)
#undef __EMIT_SYNTAX_FAMILY_CASE__
    default:
      return new_signal(scInvalidSyntax);
  }
}


// --- F a c t o r i e s ---

// Adds a syntax factory object to the given syntax factory map under the given
// name.
static value_t add_syntax_factory(value_t map, const char *name,
    factory_constructor_t constructor, runtime_t *runtime) {
  string_t key_str;
  string_init(&key_str, name);
  TRY_DEF(key_obj, new_heap_string(runtime, &key_str));
  TRY_DEF(factory, new_heap_factory(runtime, constructor));
  TRY(set_id_hash_map_at(runtime, map, key_obj, factory));
  return success();
}

value_t init_syntax_factory_map(value_t map, runtime_t *runtime) {
  TRY(add_syntax_factory(map, "Argument", new_argument_ast, runtime));
  TRY(add_syntax_factory(map, "Array", new_array_ast, runtime));
  TRY(add_syntax_factory(map, "Invocation", new_invocation_ast, runtime));
  TRY(add_syntax_factory(map, "Literal", new_literal_ast, runtime));
  TRY(add_syntax_factory(map, "LocalDeclaration", new_local_declaration_ast, runtime));
  TRY(add_syntax_factory(map, "Sequence", new_sequence_ast, runtime));
  TRY(add_syntax_factory(map, "Symbol", new_symbol_ast, runtime));
  TRY(add_syntax_factory(map, "Variable", new_variable_ast, runtime));
  return success();
}
