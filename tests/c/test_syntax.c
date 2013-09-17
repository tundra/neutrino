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

  value_t ast = new_heap_literal_ast(runtime, runtime_bool(runtime, true));
  assembler_t assm;
  ASSERT_SUCCESS(assembler_init(&assm, runtime, NULL));
  ASSERT_SUCCESS(emit_value(ast, &assm));
  assembler_emit_return(&assm);
  value_t code = assembler_flush(&assm);
  ASSERT_SUCCESS(code);
  value_t result = run_code_block(runtime, code);
  ASSERT_VALEQ(runtime_bool(runtime, true), result);
  assembler_dispose(&assm);

  DISPOSE_RUNTIME();
}
