// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "ctrino.h"
#include "log.h"
#include "value-inl.h"


GET_FAMILY_PRIMARY_TYPE_IMPL(ctrino);
FIXED_GET_MODE_IMPL(ctrino, vmDeepFrozen);

value_t ctrino_validate(value_t self) {
  VALIDATE_FAMILY(ofCtrino, self);
  return success();
}

void ctrino_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "ctrino");
}

static value_t ctrino_fail(builtin_arguments_t *args) {
  log_message(llError, NULL, 0, "@ctrino.fail()");
  exit(1);
  return nothing();
}

static value_t ctrino_get_builtin_type(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  CHECK_FAMILY(ofString, name);
#define __CHECK_BUILTIN_TYPE__(family)                                         \
  do {                                                                         \
    value_t type = ROOT(runtime, family##_type);                               \
    if (value_identity_compare(name, get_type_display_name(type)))             \
      return type;                                                             \
  } while (false);
  // Match against the built-in families.
#define __CHECK_BUILTIN_FAMILY_OPT__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW) \
  SR(__CHECK_BUILTIN_TYPE__(family),)
  ENUM_OBJECT_FAMILIES(__CHECK_BUILTIN_FAMILY_OPT__)
#undef __CHECK_BUILTIN_FAMILY_OPT__
  // Match against the built-in phylums.
#define __CHECK_BUILTIN_PHYLUM_OPT__(Phylum, phylum, CM, SR)                   \
  SR(__CHECK_BUILTIN_TYPE__(phylum),)
  ENUM_CUSTOM_TAGGED_PHYLUMS(__CHECK_BUILTIN_PHYLUM_OPT__)
#undef __CHECK_BUILTIN_PHYLUM_OPT__
  // Special cases.
  __CHECK_BUILTIN_TYPE__(integer);
#undef __CHECK_BUILTIN_TYPE__
  WARN("Couldn't resolve builtin type %v.", name);
  return null();
}

static value_t ctrino_new_function(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_function(runtime, afMutable, display_name);
}

static value_t ctrino_new_instance_manager(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_instance_manager(get_builtin_runtime(args), display_name);
}

static value_t ctrino_new_array(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t length = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  CHECK_DOMAIN(vdInteger, length);
  return new_heap_array(get_builtin_runtime(args), get_integer_value(length));
}

static value_t ctrino_new_float_32(builtin_arguments_t *args) {
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

static value_t ctrino_log_info(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  INFO("%9v", value);
  return null();
}

static value_t ctrino_print_ln(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  CHECK_FAMILY(ofCtrino, self);
  print_ln("%9v", value);
  return null();
}

static value_t ctrino_to_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  CHECK_FAMILY(ofCtrino, self);
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_printf(&buf, "%9v", value);
  string_t as_string;
  string_buffer_flush(&buf, &as_string);
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(result, new_heap_string(runtime, &as_string));
    E_RETURN(result);
  E_FINALLY();
    string_buffer_dispose(&buf);
  E_END_TRY_FINALLY();
}

static value_t ctrino_get_current_backtrace(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  frame_t *frame = args->frame;
  return capture_backtrace(runtime, frame);
}

value_t add_ctrino_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  DEF_INFIX(infix_fail, "fail");
  ADD_BUILTIN(ctrino, infix_fail, 0, ctrino_fail);
  DEF_INFIX(infix_get_builtin_type, "get_builtin_type");
  ADD_BUILTIN(ctrino, infix_get_builtin_type, 1, ctrino_get_builtin_type);
  DEF_INFIX(infix_log_info, "log_info");
  ADD_BUILTIN(ctrino, infix_log_info, 1, ctrino_log_info);
  DEF_INFIX(infix_print_ln, "print_ln");
  ADD_BUILTIN(ctrino, infix_print_ln, 1, ctrino_print_ln);
  DEF_INFIX(infix_new_array, "new_array");
  ADD_BUILTIN(ctrino, infix_new_array, 1, ctrino_new_array);
  DEF_INFIX(infix_new_float_32, "new_float_32");
  ADD_BUILTIN(ctrino, infix_new_float_32, 1, ctrino_new_float_32);
  DEF_INFIX(infix_new_function, "new_function");
  ADD_BUILTIN(ctrino, infix_new_function, 1, ctrino_new_function);
  DEF_INFIX(infix_new_instance_manager, "new_instance_manager");
  ADD_BUILTIN(ctrino, infix_new_instance_manager, 1, ctrino_new_instance_manager);
  DEF_INFIX(infix_to_string, "to_string");
  ADD_BUILTIN(ctrino, infix_to_string, 1, ctrino_to_string);
  DEF_INFIX(infix_get_current_backtrace, "get_current_backtrace");
  ADD_BUILTIN(ctrino, infix_get_current_backtrace, 0, ctrino_get_current_backtrace);
  return success();
}
