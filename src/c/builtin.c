// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "interp.h"
#include "try-inl.h"
#include "value-inl.h"

void builtin_arguments_init(builtin_arguments_t *args, runtime_t *runtime,
    frame_t *frame) {
  args->runtime = runtime;
  args->frame = frame;
}

value_t get_builtin_argument(builtin_arguments_t *args, size_t index) {
  return frame_get_argument(args->frame, 2 + index);
}

value_t get_builtin_subject(builtin_arguments_t *args) {
  return frame_get_argument(args->frame, 0);
}

runtime_t *get_builtin_runtime(builtin_arguments_t *args) {
  return args->runtime;
}

// Builds a signature for the built-in method with the given receiver, name,
// and posc positional arguments.
static value_t build_signature(runtime_t *runtime, value_t receiver,
    const char *name_c_str, size_t posc, bool allow_extra) {
  size_t argc = posc + 2;
  TRY_DEF(vector, new_heap_pair_array(runtime, argc));
  // The subject parameter.
  TRY_DEF(subject_guard, new_heap_guard(runtime, afFreeze, gtIs, receiver));
  TRY_DEF(subject_param, new_heap_parameter(runtime, afFreeze, subject_guard,
      ROOT(runtime, empty_array), false, 0));
  set_pair_array_first_at(vector, 0, ROOT(runtime, subject_key));
  set_pair_array_second_at(vector, 0, subject_param);
  // The selector parameter.
  string_t name_str;
  string_init(&name_str, name_c_str);
  TRY_DEF(name, new_heap_string(runtime, &name_str));
  TRY_DEF(name_guard, new_heap_guard(runtime, afFreeze, gtEq, name));
  TRY_DEF(name_param, new_heap_parameter(runtime, afFreeze, name_guard,
      ROOT(runtime, empty_array), false, 1));
  set_pair_array_first_at(vector, 1, ROOT(runtime, selector_key));
  set_pair_array_second_at(vector, 1, name_param);
  // The positional parameters.
  for (size_t i = 0; i < posc; i++) {
    TRY_DEF(param, new_heap_parameter(runtime, afFreeze, ROOT(runtime, any_guard),
        ROOT(runtime, empty_array), false, 2 + i));
    set_pair_array_first_at(vector, 2 + i, new_integer(i));
    set_pair_array_second_at(vector, 2 + i, param);
  }
  co_sort_pair_array(vector);
  return new_heap_signature(runtime, afFreeze, vector, argc, argc, allow_extra);
}

value_t add_methodspace_builtin_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name_c_str, size_t posc,
    builtin_method_t implementation) {
  CHECK_FAMILY(ofMethodspace, space);
  CHECK_FAMILY(ofProtocol, receiver);
  // Build the implementation.
  E_BEGIN_TRY_FINALLY();
    assembler_t assm;
    E_TRY(assembler_init(&assm, runtime, NULL));
    E_TRY(assembler_emit_builtin(&assm, implementation));
    E_TRY(assembler_emit_return(&assm));
    E_TRY_DEF(code_block, assembler_flush(&assm));
    E_TRY_DEF(signature, build_signature(runtime, receiver, name_c_str, posc, false));
    E_TRY_DEF(method, new_heap_method(runtime, afFreeze, signature,
        ROOT(runtime, nothing), code_block));
    E_RETURN(add_methodspace_method(runtime, space, method));
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

value_t add_methodspace_custom_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name_c_str, size_t posc, bool allow_extra,
    custom_method_emitter_t emitter) {
  CHECK_FAMILY(ofMethodspace, space);
  CHECK_FAMILY(ofProtocol, receiver);
  // Build the implementation.
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, NULL));
  TRY(emitter(&assm));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  TRY_DEF(signature, build_signature(runtime, receiver, name_c_str, posc, allow_extra));
  TRY_DEF(method, new_heap_method(runtime, afFreeze, signature,
      ROOT(runtime, nothing), code_block));
  return add_methodspace_method(runtime, space, method);
}

value_t add_methodspace_builtin_methods(runtime_t *runtime, safe_value_t s_self) {
  TRY(add_integer_builtin_methods(runtime, s_self));
#define __EMIT_ADD_BUILTINS_CALL__(Family, family, CM, ID, CT, SR, NL, FU, EM, MD, OW)\
  SR(                                                                          \
    TRY(add_##family##_builtin_methods(runtime, s_self));,                     \
    )
  ENUM_OBJECT_FAMILIES(__EMIT_ADD_BUILTINS_CALL__)
#undef __EMIT_ADD_BUILTINS_CALL__
  return success();
}
