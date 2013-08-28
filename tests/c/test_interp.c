// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "syntax.h"
#include "test.h"

// Evaluates the given syntax tree and checks that the result is the given
// expected value.
static void assert_ast_value(runtime_t *runtime, variant_t expected, value_t ast) {
  value_t space = new_heap_method_space(runtime);
  add_method_space_builtin_methods(runtime, space);
  value_t code_block = compile_syntax(runtime, ast, space);
  value_t result = run_code_block(runtime, code_block);
  ASSERT_VALEQ(variant_to_value(runtime, expected), result);
}

TEST(interp, execution) {
  CREATE_RUNTIME();

  // Literal
  {
    value_t ast = new_heap_literal_ast(runtime, new_integer(121));
    assert_ast_value(runtime, vInt(121), ast);
  }

  // Array
  {
    value_t elements = new_heap_array(runtime, 2);
    set_array_at(elements, 0, new_heap_literal_ast(runtime, new_integer(98)));
    set_array_at(elements, 1, new_heap_literal_ast(runtime, new_integer(87)));
    value_t ast = new_heap_array_ast(runtime, elements);
    assert_ast_value(runtime, vArray(2, vInt(98) o vInt(87)), ast);
  }

  // 0-element sequence
  {
    value_t ast = new_heap_sequence_ast(runtime, runtime->roots.empty_array);
    assert_ast_value(runtime, vNull(), ast);
  }

  // 1-element sequence
  {
    value_t values = new_heap_array(runtime, 1);
    set_array_at(values, 0, new_heap_literal_ast(runtime, new_integer(98)));
    value_t ast = new_heap_sequence_ast(runtime, values);
    assert_ast_value(runtime, vInt(98), ast);
  }

  // 2-element sequence
  {
    value_t values = new_heap_array(runtime, 2);
    set_array_at(values, 0, new_heap_literal_ast(runtime, new_integer(98)));
    set_array_at(values, 1, new_heap_literal_ast(runtime, new_integer(87)));
    value_t ast = new_heap_sequence_ast(runtime, values);
    assert_ast_value(runtime, vInt(87), ast);
  }

  // Simple local definition
  {
    value_t sym = new_heap_symbol_ast(runtime, runtime_null(runtime));
    value_t var = new_heap_variable_ast(runtime, sym);
    value_t ast = new_heap_local_declaration_ast(runtime, sym,
        new_heap_literal_ast(runtime, new_integer(3)), var);
    assert_ast_value(runtime, vInt(3), ast);
  }

  // Simple lambda
  {
    value_t lam = new_heap_lambda_ast(runtime, runtime->roots.empty_array,
        new_heap_literal_ast(runtime, new_integer(13)));
    value_t this_arg = new_heap_argument_ast(runtime, runtime->roots.string_table.this,
        lam);
    value_t name_arg = new_heap_argument_ast(runtime, runtime->roots.string_table.name,
        new_heap_literal_ast(runtime, runtime->roots.string_table.sausages));
    value_t args = new_heap_array(runtime, 2);
    set_array_at(args, 0, this_arg);
    set_array_at(args, 1, name_arg);
    value_t ast = new_heap_invocation_ast(runtime, args);
    assert_ast_value(runtime, vInt(13), ast);
  }

  DISPOSE_RUNTIME();
}

// Tries to compile the given syntax tree and expects it to fail with the
// specified signal.
static void assert_compile_failure(runtime_t *runtime, value_t ast,
    invalid_syntax_cause_t cause) {
  value_t space = new_heap_method_space(runtime);
  value_t result = compile_syntax(runtime, ast, space);
  ASSERT_SIGNAL(scInvalidSyntax, result);
  ASSERT_EQ(cause, get_invalid_syntax_signal_cause(result));
}

TEST(interp, compile_errors) {
  CREATE_RUNTIME();

  value_t l3 = new_heap_literal_ast(runtime, new_integer(3));
  // Redefinition of a local.
  {
    value_t sym = new_heap_symbol_ast(runtime, runtime_null(runtime));
    value_t var = new_heap_variable_ast(runtime, sym);
    value_t inner = new_heap_local_declaration_ast(runtime, sym, l3, var);
    value_t outer = new_heap_local_declaration_ast(runtime, sym, l3, inner);
    assert_compile_failure(runtime, outer, isSymbolAlreadyBound);
  }

  // Accessing an undefined symbol.
  {
    value_t s0 = new_heap_symbol_ast(runtime, runtime_null(runtime));
    value_t s1 = new_heap_symbol_ast(runtime, runtime_null(runtime));
    value_t var = new_heap_variable_ast(runtime, s0);
    value_t ast = new_heap_local_declaration_ast(runtime, s1, l3, var);
    assert_compile_failure(runtime, ast, isSymbolNotBound);
  }

  DISPOSE_RUNTIME();
}
