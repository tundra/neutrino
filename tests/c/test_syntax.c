// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "runtime.h"
#include "syntax.h"
#include "test.h"
#include "value-inl.h"

TEST(syntax, emitting) {
  CREATE_RUNTIME();

  value_t ast = new_heap_literal_ast(runtime, runtime_bool(runtime, false));
  assembler_t assm;
  ASSERT_SUCCESS(assembler_init(&assm, runtime, NULL));
  ASSERT_SUCCESS(emit_value(ast, &assm));
  assembler_emit_return(&assm);
  value_t code = assembler_flush(&assm);
  ASSERT_SUCCESS(code);
  value_t result = run_code_block_until_signal(runtime, code);
  ASSERT_VALEQ(runtime_bool(runtime, true), result);
  assembler_dispose(&assm);

  DISPOSE_RUNTIME();
}

// Shorthand for running an ordering index test.
#define CHECK_ORDERING_INDEX(N, V)                                             \
  ASSERT_EQ((N), get_parameter_order_index_for_array(variant_to_value(runtime, (V))))

TEST(syntax, parameter_order_index) {
  CREATE_RUNTIME();

  variant_t subject_key = vValue(ROOT(runtime, subject_key));
  variant_t selector_key = vValue(ROOT(runtime, selector_key));

  CHECK_ORDERING_INDEX(0, vArray(1, subject_key));
  CHECK_ORDERING_INDEX(1, vArray(1, selector_key));
  CHECK_ORDERING_INDEX(0, vArray(2, subject_key o selector_key));

  CHECK_ORDERING_INDEX(2, vArray(1, vInt(0)));
  CHECK_ORDERING_INDEX(3, vArray(1, vInt(1)));
  CHECK_ORDERING_INDEX(4, vArray(1, vInt(2)));
  CHECK_ORDERING_INDEX(2, vArray(2, vInt(0) o vInt(2)));
  CHECK_ORDERING_INDEX(2, vArray(2, vInt(2) o vInt(0)));
  CHECK_ORDERING_INDEX(1, vArray(2, vInt(2) o selector_key));

  CHECK_ORDERING_INDEX(kMaxOrderIndex, vArray(1, vStr("foo")));
  CHECK_ORDERING_INDEX(102, vArray(2, vStr("foo") o vInt(100)));

  DISPOSE_RUNTIME();
}

// Shorthand for running an ordering test.
#define CHECK_ORDERING(N, PARAMS, EXPECTED) do {                               \
  value_t params = new_heap_array(runtime, (N));                               \
  variant_t var_params = (PARAMS);                                             \
  variant_t *elms = var_params.value.as_array.elements;                        \
  for (size_t i = 0; i < (N); i++) {                                           \
    value_t tags = variant_to_value(runtime, elms[i]);                         \
    set_array_at(params, i, new_heap_parameter_ast(runtime,                    \
        ROOT(runtime, nothing), tags, ROOT(runtime, any_guard)));              \
  }                                                                            \
  size_t *ordering = calc_parameter_ast_ordering(&scratch, params);            \
  size_t expected[N] = {EXPECTED};                                             \
  for (size_t i = 0; i < (N); i++)                                             \
    ASSERT_EQ(ordering[i], expected[i]);                                       \
} while (false)

TEST(syntax, param_ordering) {
  CREATE_RUNTIME();

  reusable_scratch_memory_t scratch;
  reusable_scratch_memory_init(&scratch);

  variant_t sub = vValue(ROOT(runtime, subject_key));
  variant_t sel = vValue(ROOT(runtime, selector_key));
  variant_t just_sub = vArray(1, sub);
  variant_t just_sel = vArray(1, sel);
  variant_t just_0 = vArray(1, vInt(0));
  variant_t just_1 = vArray(1, vInt(1));
  variant_t just_2 = vArray(1, vInt(2));
  variant_t just_3 = vArray(1, vInt(3));

  CHECK_ORDERING(4, vArray(4, just_0 o just_1 o just_2 o just_3), 0 o 1 o 2 o 3);
  CHECK_ORDERING(4, vArray(4, just_3 o just_2 o just_1 o just_0), 3 o 2 o 1 o 0);
  CHECK_ORDERING(4, vArray(4, just_2 o just_0 o just_3 o just_1), 2 o 0 o 3 o 1);
  CHECK_ORDERING(3, vArray(3, just_2 o just_0 o just_3), 1 o 0 o 2);
  CHECK_ORDERING(2, vArray(2, just_2 o just_3), 0 o 1);

  CHECK_ORDERING(3, vArray(3, just_2 o vArray(2, vInt(0) o vInt(1)) o just_3), 1 o 0 o 2);
  CHECK_ORDERING(3, vArray(3, just_2 o vArray(2, vInt(0) o vInt(4)) o just_3), 1 o 0 o 2);
  CHECK_ORDERING(3, vArray(3, just_2 o vArray(2, vInt(5) o vInt(4)) o just_3), 0 o 2 o 1);

  CHECK_ORDERING(4, vArray(4, just_0 o just_1 o just_2 o just_sel), 1 o 2 o 3 o 0);
  CHECK_ORDERING(4, vArray(4, just_0 o just_sub o just_2 o just_sel), 2 o 0 o 3 o 1);
  CHECK_ORDERING(3, vArray(3, just_0 o vArray(2, sub o sel) o just_3), 1 o 0 o 2);

  reusable_scratch_memory_dispose(&scratch);

  DISPOSE_RUNTIME();
}
