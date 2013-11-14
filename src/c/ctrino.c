// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
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

value_t ctrino_new_protocol(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_protocol(get_builtin_runtime(args), afMutable, display_name);
}

value_t ctrino_new_instance(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t protocol = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  CHECK_FAMILY(ofProtocol, protocol);
  TRY_DEF(species, new_heap_instance_species(runtime, protocol));
  return new_heap_instance(runtime, species);
}

value_t add_ctrino_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(ctrino, INFIX("fail"), 0, ctrino_fail);
  ADD_BUILTIN(ctrino, INFIX("new_protocol"), 1, ctrino_new_protocol);
  ADD_BUILTIN(ctrino, INFIX("new_instance"), 1, ctrino_new_instance);
  return success();
}
