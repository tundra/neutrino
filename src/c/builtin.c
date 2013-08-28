// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "interp.h"
#include "value-inl.h"

void builtin_arguments_init(builtin_arguments_t *args, runtime_t *runtime,
    frame_t *frame) {
  args->runtime = runtime;
  args->frame = frame;
}

value_t get_builtin_argument(builtin_arguments_t *args, size_t index) {
  return frame_get_argument(args->frame, 2 + index);
}

value_t get_builtin_this(builtin_arguments_t *args) {
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
  // The "this" parameter.
  TRY_DEF(this_guard, new_heap_guard(runtime, gtIs, receiver));
  TRY_DEF(this_param, new_heap_parameter(runtime, this_guard, false, 0));
  set_pair_array_first_at(vector, 0, runtime->roots.string_table.this);
  set_pair_array_second_at(vector, 0, this_param);
  // The "name" parameter.
  string_t name_str;
  string_init(&name_str, name_c_str);
  TRY_DEF(name, new_heap_string(runtime, &name_str));
  TRY_DEF(name_guard, new_heap_guard(runtime, gtEq, name));
  TRY_DEF(name_param, new_heap_parameter(runtime, name_guard, false, 1));
  set_pair_array_first_at(vector, 1, runtime->roots.string_table.name);
  set_pair_array_second_at(vector, 1, name_param);
  // The positional parameters.
  for (size_t i = 0; i < posc; i++) {
    TRY_DEF(param, new_heap_parameter(runtime, runtime->roots.any_guard, false,
        2 + i));
    set_pair_array_first_at(vector, 2 + i, new_integer(i));
    set_pair_array_second_at(vector, 2 + i, param);
  }
  co_sort_pair_array(vector);
  return new_heap_signature(runtime, vector, argc, argc, allow_extra);
}

value_t add_method_space_builtin_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name_c_str, size_t posc,
    builtin_method_t implementation) {
  CHECK_FAMILY(ofMethodSpace, space);
  CHECK_FAMILY(ofProtocol, receiver);
  // Build the implementation.
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, space, runtime_null(runtime)));
  TRY(assembler_emit_builtin(&assm, implementation));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  TRY_DEF(signature, build_signature(runtime, receiver, name_c_str, posc, false));
  TRY_DEF(method, new_heap_method(runtime, signature, code_block));
  return add_method_space_method(runtime, space, method);
}

value_t add_method_space_custom_method(runtime_t *runtime, value_t space,
    value_t receiver, const char *name_c_str, size_t posc, bool allow_extra,
    custom_method_emitter_t emitter) {
  CHECK_FAMILY(ofMethodSpace, space);
  CHECK_FAMILY(ofProtocol, receiver);
  // Build the implementation.
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, space, runtime_null(runtime)));
  TRY(emitter(&assm));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  TRY_DEF(signature, build_signature(runtime, receiver, name_c_str, posc, allow_extra));
  TRY_DEF(method, new_heap_method(runtime, signature, code_block));
  return add_method_space_method(runtime, space, method);
}

value_t add_method_space_builtin_methods(runtime_t *runtime, value_t self) {
  TRY(add_integer_builtin_methods(runtime, self));
#define __EMIT_ADD_BUILTINS_CALL__(Family, family, CMP, CID, CNT, SUR, NOL, FIX, EMT)\
  SUR(TRY(add_##family##_builtin_methods(runtime, self));,)
  ENUM_OBJECT_FAMILIES(__EMIT_ADD_BUILTINS_CALL__)
#undef __EMIT_ADD_BUILTINS_CALL__
  return success();
}
