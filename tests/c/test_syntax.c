// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "interp.h"
#include "runtime.h"
#include "syntax.h"
#include "test.h"
#include "value-inl.h"

TEST(syntax, emitting) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t ast = new_heap_literal_ast(&runtime, runtime_bool(&runtime, true));
  assembler_t assm;
  ASSERT_SUCCESS(assembler_init(&assm, &runtime));
  ASSERT_SUCCESS(emit_value(ast, &assm));
  value_t code = assembler_flush(&assm);
  ASSERT_SUCCESS(code);
  // TODO: try executing the code and check that we get the right result.
  assembler_dispose(&assm);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
