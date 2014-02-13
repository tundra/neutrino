// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "interp.h"
#include "log.h"
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

value_t add_builtin_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t arg_count, builtin_method_t impl) {
  CHECK_FAMILY(ofIdHashMap, map);
  E_BEGIN_TRY_FINALLY();
    assembler_t assm;
    E_TRY(assembler_init(&assm, runtime, nothing(),
        scope_lookup_callback_get_bottom()));
    E_TRY(assembler_emit_builtin(&assm, impl));
    E_TRY(assembler_emit_return(&assm));
    E_TRY_DEF(code_block, assembler_flush(&assm));
    string_t name_str;
    string_init(&name_str, name_c_str);
    E_TRY_DEF(name, new_heap_string(runtime, &name_str));
    E_TRY_DEF(builtin, new_heap_builtin_implementation(runtime, afFreeze,
        name, code_block, arg_count));
    E_RETURN(set_id_hash_map_at(runtime, map, name, builtin));
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

value_t add_custom_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t posc, custom_method_emitter_t emitter) {
  CHECK_FAMILY(ofIdHashMap, map);
  // Build the implementation.
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, nothing(),
      scope_lookup_callback_get_bottom()));
  TRY(emitter(&assm));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  string_t name_str;
  string_init(&name_str, name_c_str);
  TRY_DEF(name, new_heap_string(runtime, &name_str));
  TRY_DEF(builtin, new_heap_builtin_implementation(runtime, afFreeze,
      name, code_block, posc));
  return set_id_hash_map_at(runtime, map, name, builtin);
}

value_t add_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  add_integer_builtin_implementations(runtime, s_map);
  add_object_builtin_implementations(runtime, s_map);

  // The family built-ins.
#define __EMIT_FAMILY_BUILTINS_CALL__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW, N)\
  SR(                                                                          \
    TRY(add_##family##_builtin_implementations(runtime, s_map));,              \
    )
  ENUM_OBJECT_FAMILIES(__EMIT_FAMILY_BUILTINS_CALL__)
#undef __EMIT_ADD_BUILTINS_CALL__

  // The phylum built-ins.
#define __EMIT_PHYLUM_BUILTINS_CALL__(Phylum, phylum, CM, SR)                  \
  SR(                                                                          \
    TRY(add_##phylum##_builtin_implementations(runtime, s_map));,              \
    )
  ENUM_CUSTOM_TAGGED_PHYLUMS(__EMIT_PHYLUM_BUILTINS_CALL__)
#undef __EMIT_PHYLUM_BUILTINS_CALL__

  return success();
}
