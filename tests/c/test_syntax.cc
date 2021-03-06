//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "interp.h"
#include "runtime.h"
#include "syntax.h"
#include "value-inl.h"
END_C_INCLUDES

TEST(syntax, emitting) {
  CREATE_RUNTIME();

  value_t ast = new_heap_literal_ast(runtime, afFreeze, yes());
  assembler_t assm;
  ASSERT_SUCCESS(assembler_init(&assm, runtime, nothing(), scope_get_bottom()));
  ASSERT_SUCCESS(emit_value(ast, &assm));
  assembler_emit_return(&assm);
  value_t code = assembler_flush(&assm);
  ASSERT_SUCCESS(code);
  value_t result = run_code_block_until_condition(ambience, code);
  ASSERT_VALEQ(yes(), result);
  assembler_dispose(&assm);

  DISPOSE_RUNTIME();
}

// Shorthand for running an ordering index test.
#define CHECK_ORDERING_INDEX(N, V)                                             \
  ASSERT_EQ((N), get_parameter_order_index_for_array(variant_to_value(runtime, (V))))

TEST(syntax, parameter_order_index) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *subject_key = vValue(ROOT(runtime, subject_key));
  variant_t *selector_key = vValue(ROOT(runtime, selector_key));

  CHECK_ORDERING_INDEX(0, vArray(subject_key));
  CHECK_ORDERING_INDEX(1, vArray(selector_key));
  CHECK_ORDERING_INDEX(0, vArray(subject_key, selector_key));

  CHECK_ORDERING_INDEX(3, vArray(vInt(0)));
  CHECK_ORDERING_INDEX(4, vArray(vInt(1)));
  CHECK_ORDERING_INDEX(5, vArray(vInt(2)));
  CHECK_ORDERING_INDEX(3, vArray(vInt(0), vInt(2)));
  CHECK_ORDERING_INDEX(3, vArray(vInt(2), vInt(0)));
  CHECK_ORDERING_INDEX(1, vArray(vInt(2), selector_key));

  CHECK_ORDERING_INDEX(kMaxOrderIndex, vArray(vStr("foo")));
  CHECK_ORDERING_INDEX(103, vArray(vStr("foo"), vInt(100)));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

// Shorthand for running an ordering test.
#define CHECK_ORDERING(N, PARAMS, ...) do {                                    \
  value_t params = new_heap_array(runtime, (N));                               \
  variant_t *var_params = (PARAMS);                                            \
  variant_t **elms = var_params->value.as_array.elements;                      \
  for (size_t i = 0; i < (N); i++) {                                           \
    value_t tags = variant_to_value(runtime, elms[i]);                         \
    set_array_at(params, i, new_heap_parameter_ast(runtime, afFreeze,          \
        nothing(), tags, any_guard_ast));                                      \
  }                                                                            \
  size_t *ordering = calc_parameter_ast_ordering(&scratch, params);            \
  size_t expected[N] = {__VA_ARGS__};                                          \
  for (size_t i = 0; i < (N); i++)                                             \
    ASSERT_EQ(ordering[i], expected[i]);                                       \
} while (false)

TEST(syntax, param_ordering) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t any_guard_ast = new_heap_guard_ast(runtime, afFreeze, gtAny, null());

  reusable_scratch_memory_t scratch;
  reusable_scratch_memory_init(&scratch);

  variant_t *sub = vValue(ROOT(runtime, subject_key));
  variant_t *sel = vValue(ROOT(runtime, selector_key));
  variant_t *just_sub = vArray(sub);
  variant_t *just_sel = vArray(sel);
  variant_t *just_0 = vArray(vInt(0));
  variant_t *just_1 = vArray(vInt(1));
  variant_t *just_2 = vArray(vInt(2));
  variant_t *just_3 = vArray(vInt(3));

  CHECK_ORDERING(4, vArray(just_0, just_1, just_2, just_3), 0, 1, 2, 3);
  CHECK_ORDERING(4, vArray(just_3, just_2, just_1, just_0), 3, 2, 1, 0);
  CHECK_ORDERING(4, vArray(just_2, just_0, just_3, just_1), 2, 0, 3, 1);
  CHECK_ORDERING(3, vArray(just_2, just_0, just_3), 1, 0, 2);
  CHECK_ORDERING(2, vArray(just_2, just_3), 0, 1);

  CHECK_ORDERING(3, vArray(just_2, vArray(vInt(0), vInt(1)), just_3), 1, 0, 2);
  CHECK_ORDERING(3, vArray(just_2, vArray(vInt(0), vInt(4)), just_3), 1, 0, 2);
  CHECK_ORDERING(3, vArray(just_2, vArray(vInt(5), vInt(4)), just_3), 0, 2, 1);

  CHECK_ORDERING(4, vArray(just_0, just_1, just_2, just_sel), 1, 2, 3, 0);
  CHECK_ORDERING(4, vArray(just_0, just_sub, just_2, just_sel), 2, 0, 3, 1);
  CHECK_ORDERING(3, vArray(just_0, vArray(sub, sel), just_3), 1, 0, 2);

  reusable_scratch_memory_dispose(&scratch);

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}
