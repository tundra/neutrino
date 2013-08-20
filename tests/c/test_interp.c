// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "syntax.h"
#include "test.h"

TEST(interp, exec) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);

  // Literal
  {
    value_t ast = new_heap_literal_ast(&runtime, new_integer(121));
    value_t code_block = compile_syntax(&runtime, ast, space);
    value_t value = run_code_block(&runtime, code_block);
    ASSERT_VALEQ(new_integer(121), value);
  }

  // Array
  {
    value_t elements = new_heap_array(&runtime, 2);
    set_array_at(elements, 0, new_heap_literal_ast(&runtime, new_integer(98)));
    set_array_at(elements, 1, new_heap_literal_ast(&runtime, new_integer(87)));
    value_t ast = new_heap_array_ast(&runtime, elements);
    value_t code_block = compile_syntax(&runtime, ast, space);
    value_t value = run_code_block(&runtime, code_block);
    ASSERT_FAMILY(ofArray, value);
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
