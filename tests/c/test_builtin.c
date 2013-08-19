// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "builtin.h"
#include "method.h"
#include "test.h"

static void test_builtin(runtime_t *runtime, value_t space, variant_t receiver,
    const char *name, variant_t args) {
  ASSERT_EQ(vtArray, args.type);
  size_t positional_count = args.value.as_array.length;
  size_t arg_count = 2 + positional_count;

  // Build an ast that implements the requested call.
  value_t args_ast = new_heap_array(runtime, arg_count);
  // The "this" argument.
  set_array_at(args_ast, 0, new_heap_argument_ast(runtime,
      variant_to_value(runtime, vStr("this")),
      new_heap_literal_ast(runtime,
          variant_to_value(runtime, receiver))));
  // The "name" argument.
  set_array_at(args_ast, 1, new_heap_argument_ast(runtime,
      variant_to_value(runtime, vStr("name")),
      new_heap_literal_ast(runtime,
          variant_to_value(runtime, vStr(name)))));
  // The positional arguments.
  for (size_t i = 0; i < positional_count; i++) {
    variant_t var_arg = args.value.as_array.elements[i];
    set_array_at(args_ast, 2 + i, new_heap_argument_ast(runtime,
        new_integer(i),
        new_heap_literal_ast(runtime,
            variant_to_value(runtime, var_arg))));

  }
  value_t invocation = new_heap_invocation_ast(runtime, args_ast);

  // Compile and execute the syntax.
  value_t code = compile_syntax(runtime, invocation, space);
  value_t result = run_code_block(runtime, code);
  value_print_ln(result);
  ASSERT_SUCCESS(result);
}

TEST(builtin, hul_igennem) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);
  add_method_space_built_in_methods(&runtime, space);

  test_builtin(&runtime, space, vInt(1), "+", vArray(1, vInt(1)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
