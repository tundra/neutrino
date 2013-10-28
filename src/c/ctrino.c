// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "builtin.h"
#include "ctrino.h"
#include "log.h"
#include "value-inl.h"


TRIVIAL_PRINT_ON_IMPL(Ctrino, ctrino);
GET_FAMILY_PROTOCOL_IMPL(ctrino);
FIXED_GET_MODE_IMPL(ctrino, vmDeepFrozen);

value_t ctrino_validate(value_t self) {
  VALIDATE_FAMILY(ofCtrino, self);
  return success();
}

value_t ctrino_fail(builtin_arguments_t *args) {
  log_message(llError, NULL, 0, "@ctrino.fail()");
  exit(1);
  runtime_t *runtime = get_builtin_runtime(args);
  return ROOT(runtime, nothing);
}

value_t add_ctrino_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(ctrino, INFIX("fail"), 0, ctrino_fail);
  return success();
}
