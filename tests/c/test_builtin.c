// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "builtin.h"
#include "method.h"
#include "safe-inl.h"
#include "tagged.h"
#include "test.h"

static void test_builtin(runtime_t *runtime, value_t module, variant_t *expected,
    variant_t *receiver, builtin_operation_t operation, variant_t *args) {
  size_t positional_count = args->value.as_array.length;
  size_t arg_count = 2 + positional_count;

  // Build an ast that implements the requested call.
  value_t args_ast = new_heap_array(runtime, arg_count);
  // The subject argument.
  set_array_at(args_ast, 0, new_heap_argument_ast(runtime,
      ROOT(runtime, subject_key),
      new_heap_literal_ast(runtime, C(receiver))));
  // The selector argument.
  value_t selector = builtin_operation_to_value(runtime, &operation);
  set_array_at(args_ast, 1, new_heap_argument_ast(runtime,
      ROOT(runtime, selector_key),
      new_heap_literal_ast(runtime, selector)));
  // The positional arguments.
  for (size_t i = 0; i < positional_count; i++) {
    variant_t *var_arg = args->value.as_array.elements[i];
    set_array_at(args_ast, 2 + i, new_heap_argument_ast(runtime,
        new_integer(i),
        new_heap_literal_ast(runtime, C(var_arg))));
  }
  value_t invocation = new_heap_invocation_ast(runtime, args_ast);

  // Compile and execute the syntax.
  value_t code = compile_expression(runtime, invocation, module,
      scope_lookup_callback_get_bottom());
  value_t result = run_code_block_until_signal(runtime, code);
  ASSERT_SUCCESS(result);
  ASSERT_VAREQ(expected, result);
}

// XXX
static value_t new_empty_module_fragment(runtime_t *runtime) {
  TRY_DEF(module, new_heap_empty_module(runtime, nothing()));
  TRY_DEF(fragment, new_heap_module_fragment(runtime, module, present_stage(),
      nothing(), ROOT(runtime, builtin_methodspace),
      nothing()));
  TRY(add_to_array_buffer(runtime, get_module_fragments(module), fragment));
  return fragment;
}

TEST(builtin, integers) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);

  value_t fragment = new_empty_module_fragment(runtime);

  DEF_INFIX(infix_plus, "+");
  test_builtin(runtime, fragment, vInt(2), vInt(1), infix_plus, vArray(vInt(1)));
  test_builtin(runtime, fragment, vInt(3), vInt(2), infix_plus, vArray(vInt(1)));
  test_builtin(runtime, fragment, vInt(5), vInt(2), infix_plus, vArray(vInt(3)));

  DEF_INFIX(infix_minus, "-");
  test_builtin(runtime, fragment, vInt(0), vInt(1), infix_minus, vArray(vInt(1)));
  test_builtin(runtime, fragment, vInt(1), vInt(2), infix_minus, vArray(vInt(1)));
  test_builtin(runtime, fragment, vInt(-1), vInt(2), infix_minus, vArray(vInt(3)));

  DEF_PREFIX(prefix_minus, "-");
  test_builtin(runtime, fragment, vInt(-1), vInt(1), prefix_minus, vEmptyArray());

  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(builtin, strings) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);

  value_t fragment = new_empty_module_fragment(runtime);

  DEF_INFIX(infix_plus, "+");
  test_builtin(runtime, fragment, vStr("abcd"), vStr("ab"), infix_plus,
      vArray(vStr("cd")));
  test_builtin(runtime, fragment, vStr(""), vStr(""), infix_plus,
      vArray(vStr("")));

  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}
