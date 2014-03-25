//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "ctrino.h"
#include "freeze.h"
#include "log.h"
#include "value-inl.h"

// --- F r a m e w o r k ---

// Builds a signature for the built-in ctrino method with the given name and
// positional argument count.
static value_t build_ctrino_method_signature(runtime_t *runtime, value_t selector, size_t posc) {
  size_t argc = posc + 2;
  TRY_DEF(vector, new_heap_pair_array(runtime, argc));
  // The subject parameter.
  TRY_DEF(subject_guard, new_heap_guard(runtime, afFreeze, gtIs,
      ROOT(runtime, ctrino_type)));
  TRY_DEF(subject_param, new_heap_parameter(runtime, afFreeze, subject_guard,
      ROOT(runtime, empty_array), false, 0));
  set_pair_array_first_at(vector, 0, ROOT(runtime, subject_key));
  set_pair_array_second_at(vector, 0, subject_param);
  // The selector parameter.
  TRY_DEF(name_guard, new_heap_guard(runtime, afFreeze, gtEq, selector));
  TRY_DEF(name_param, new_heap_parameter(runtime, afFreeze, name_guard,
      ROOT(runtime, empty_array), false, 1));
  set_pair_array_first_at(vector, 1, ROOT(runtime, selector_key));
  set_pair_array_second_at(vector, 1, name_param);
  // The positional parameters.
  for (size_t i = 0; i < posc; i++) {
    TRY_DEF(param, new_heap_parameter(runtime, afFreeze,
        ROOT(runtime, any_guard), ROOT(runtime, empty_array), false, 2 + i));
    set_pair_array_first_at(vector, 2 + i, new_integer(i));
    set_pair_array_second_at(vector, 2 + i, param);
  }
  co_sort_pair_array(vector);
  return new_heap_signature(runtime, afFreeze, vector, argc, argc, false);
}

// Add a ctrino method to the given method space with the given name, number of
// arguments, and implementation.
static value_t add_ctrino_method(runtime_t *runtime, value_t space,
    const char *name_c_str, size_t posc, builtin_method_t implementation) {
  CHECK_FAMILY(ofMethodspace, space);
  E_BEGIN_TRY_FINALLY();
    // Build the implementation.
    assembler_t assm;
    E_TRY(assembler_init(&assm, runtime, nothing(), scope_get_bottom()));
    E_TRY(assembler_emit_builtin(&assm, implementation));
    E_TRY(assembler_emit_return(&assm));
    E_TRY_DEF(code_block, assembler_flush(&assm));
    // Build the signature.
    string_t name_str;
    string_init(&name_str, name_c_str);
    E_TRY_DEF(name, new_heap_string(runtime, &name_str));
    E_TRY_DEF(selector, new_heap_operation(runtime, afFreeze, otInfix, name));
    E_TRY_DEF(signature, build_ctrino_method_signature(runtime, selector, posc));
    E_TRY_DEF(method,new_heap_method(runtime, afFreeze, signature, nothing(),
        code_block, nothing(), new_flag_set(kFlagSetAllOff)));
    // And in the methodspace bind them.
    E_RETURN(add_methodspace_method(runtime, space, method));
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

// --- C t r i n o ---

FIXED_GET_MODE_IMPL(ctrino, vmDeepFrozen);
NO_BUILTIN_METHODS(ctrino);
GET_FAMILY_PRIMARY_TYPE_IMPL(ctrino);

value_t ctrino_validate(value_t self) {
  VALIDATE_FAMILY(ofCtrino, self);
  return success();
}

void ctrino_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "ctrino");
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
#define __CHECK_BUILTIN_FAMILY_OPT__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW, N) \
  SR(__CHECK_BUILTIN_TYPE__(family),)
  ENUM_HEAP_OBJECT_FAMILIES(__CHECK_BUILTIN_FAMILY_OPT__)
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

static value_t ctrino_builtin(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t name = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  CHECK_FAMILY(ofCtrino, self);
  return new_heap_builtin_marker(runtime, name);
}

static value_t ctrino_freeze(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY(ensure_frozen(runtime, value));
  return null();
}

static value_t ctrino_is_frozen(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  return new_boolean(is_frozen(value));
}

static value_t ctrino_is_deep_frozen(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  return new_boolean(try_validate_deep_frozen(runtime, value, NULL));
}

#define ADD_BUILTIN(name, argc, impl)                                          \
  TRY(add_ctrino_method(runtime, space, name, argc, impl))

value_t add_ctrino_builtin_methods(runtime_t *runtime, value_t space) {
  ADD_BUILTIN("builtin", 1, ctrino_builtin);
  ADD_BUILTIN("freeze", 1, ctrino_freeze);
  ADD_BUILTIN("get_builtin_type", 1, ctrino_get_builtin_type);
  ADD_BUILTIN("get_current_backtrace", 0, ctrino_get_current_backtrace);
  ADD_BUILTIN("is_deep_frozen", 1, ctrino_is_deep_frozen);
  ADD_BUILTIN("is_frozen", 1, ctrino_is_frozen);
  ADD_BUILTIN("log_info", 1, ctrino_log_info);
  ADD_BUILTIN("new_array", 1, ctrino_new_array);
  ADD_BUILTIN("new_float_32", 1, ctrino_new_float_32);
  ADD_BUILTIN("new_function", 1, ctrino_new_function);
  ADD_BUILTIN("new_instance_manager", 1, ctrino_new_instance_manager);
  ADD_BUILTIN("print_ln", 1, ctrino_print_ln);
  ADD_BUILTIN("to_string", 1, ctrino_to_string);
  return success();
}
