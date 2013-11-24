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
  return nothing();
}

value_t ctrino_get_builtin_protocol(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  CHECK_FAMILY(ofString, name);
  return ROOT(runtime, integer_protocol);
}

value_t ctrino_new_function(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_function(runtime, afMutable, display_name);
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

value_t ctrino_new_protocol(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_protocol(get_builtin_runtime(args), afMutable, display_name);
}

value_t ctrino_new_global_field(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_global_field(get_builtin_runtime(args), display_name);
}

value_t ctrino_new_float_32(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t decimal = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  CHECK_FAMILY(ofDecimalFraction, decimal);
  // TODO: This may or may not produce the most accurate approximation of the
  //   fractional value. Either verify that it does or replace it.
  double value = get_integer_value(get_decimal_fraction_numerator(decimal));
  int32_t log_denom = get_integer_value(get_decimal_fraction_denominator(decimal));
  while (log_denom > 0) {
    value = value / 10.0;
    log_denom--;
  }
  return new_float_32(value);
}

value_t ctrino_log_info(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  INFO("%9v", value);
  return null();
}

value_t add_ctrino_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  ADD_BUILTIN(ctrino, INFIX("fail"), 0, ctrino_fail);
  ADD_BUILTIN(ctrino, INFIX("get_builtin_protocol"), 1, ctrino_get_builtin_protocol);
  ADD_BUILTIN(ctrino, INFIX("log_info"), 1, ctrino_log_info);
  ADD_BUILTIN(ctrino, INFIX("new_float_32"), 1, ctrino_new_float_32);
  ADD_BUILTIN(ctrino, INFIX("new_function"), 1, ctrino_new_function);
  ADD_BUILTIN(ctrino, INFIX("new_global_field"), 1, ctrino_new_global_field);
  ADD_BUILTIN(ctrino, INFIX("new_instance"), 1, ctrino_new_instance);
  ADD_BUILTIN(ctrino, INFIX("new_protocol"), 1, ctrino_new_protocol);
  return success();
}
