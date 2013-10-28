// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "builtin.h"
#include "method.h"
#include "safe-inl.h"
#include "test.h"

static void test_builtin(runtime_t *runtime, value_t space, variant_t expected,
    variant_t receiver, builtin_operation_t operation, variant_t args) {
  ASSERT_EQ(vtArray, args.type);
  size_t positional_count = args.value.as_array.length;
  size_t arg_count = 2 + positional_count;

  // Build an ast that implements the requested call.
  value_t args_ast = new_heap_array(runtime, arg_count);
  // The subject argument.
  set_array_at(args_ast, 0, new_heap_argument_ast(runtime,
      ROOT(runtime, subject_key),
      new_heap_literal_ast(runtime,
          variant_to_value(runtime, receiver))));
  // The selector argument.
  value_t selector = builtin_operation_to_value(runtime, &operation);
  set_array_at(args_ast, 1, new_heap_argument_ast(runtime,
      ROOT(runtime, selector_key),
      new_heap_literal_ast(runtime, selector)));
  // The positional arguments.
  for (size_t i = 0; i < positional_count; i++) {
    variant_t var_arg = args.value.as_array.elements[i];
    set_array_at(args_ast, 2 + i, new_heap_argument_ast(runtime,
        new_integer(i),
        new_heap_literal_ast(runtime,
            variant_to_value(runtime, var_arg))));
  }
  value_t invocation = new_heap_invocation_ast(runtime, args_ast, space);

  // Compile and execute the syntax.
  value_t code = compile_expression(runtime, invocation,
      scope_lookup_callback_get_bottom());
  value_t result = run_code_block_until_signal(runtime, code);
  ASSERT_SUCCESS(result);
  ASSERT_VALEQ(variant_to_value(runtime, expected), result);
}

TEST(builtin, integers) {
  CREATE_RUNTIME();
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);

  value_t space = ROOT(runtime, builtin_methodspace);

  test_builtin(runtime, space, vInt(2), vInt(1), INFIX_BUILTIN("+"),
      vArray(1, vInt(1)));
  test_builtin(runtime, space, vInt(3), vInt(2), INFIX_BUILTIN("+"),
      vArray(1, vInt(1)));
  test_builtin(runtime, space, vInt(5), vInt(2), INFIX_BUILTIN("+"),
      vArray(1, vInt(3)));

  test_builtin(runtime, space, vInt(0), vInt(1), INFIX_BUILTIN("-"),
      vArray(1, vInt(1)));
  test_builtin(runtime, space, vInt(1), vInt(2), INFIX_BUILTIN("-"),
      vArray(1, vInt(1)));
  test_builtin(runtime, space, vInt(-1), vInt(2), INFIX_BUILTIN("-"),
      vArray(1, vInt(3)));

  test_builtin(runtime, space, vInt(-1), vInt(1), SUFFIX_BUILTIN("-"), vEmptyArray());

  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_RUNTIME();
}

TEST(builtin, strings) {
  CREATE_RUNTIME();
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);

  value_t space = ROOT(runtime, builtin_methodspace);

  test_builtin(runtime, space, vStr("abcd"), vStr("ab"), INFIX_BUILTIN("+"),
      vArray(1, vStr("cd")));
  test_builtin(runtime, space, vStr(""), vStr(""), INFIX_BUILTIN("+"),
      vArray(1, vStr("")));

  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_RUNTIME();
}
