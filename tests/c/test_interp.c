// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "syntax.h"
#include "test.h"

TEST(interp, exec) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t ast = new_heap_literal_ast(&runtime, new_integer(121));
  value_t code_block = compile_syntax(&runtime, ast);
  value_t value = run_code_block(&runtime, code_block);
  ASSERT_VALEQ(new_integer(121), value);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
