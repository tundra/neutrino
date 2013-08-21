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
  value_t code_block = compile_syntax(runtime, ast, space);
  value_t result = run_code_block(runtime, code_block);
  ASSERT_VALEQ(variant_to_value(runtime, expected), result);
}

TEST(interp, exec) {
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

  DISPOSE_RUNTIME();
}
