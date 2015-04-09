//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "interp.h"
#include "try-inl.h"
#include "utils/log.h"
#include "value-inl.h"

void builtin_arguments_init(builtin_arguments_t *args, runtime_t *runtime,
    frame_t *frame, value_t process) {
  args->runtime = runtime;
  args->frame = frame;
  args->process = process;
}

value_t get_builtin_argument(builtin_arguments_t *args, size_t index) {
  return frame_get_argument(args->frame, kImplicitArgumentCount + index);
}

value_t get_builtin_subject(builtin_arguments_t *args) {
  return frame_get_argument(args->frame, 0);
}

runtime_t *get_builtin_runtime(builtin_arguments_t *args) {
  return args->runtime;
}

value_t get_builtin_process(builtin_arguments_t *args) {
  return args->process;
}

value_t escape_builtin(builtin_arguments_t *args, value_array_t values) {
  // Push the values onto the stack.
  for (size_t i = 0; i < values.length; i++)
    frame_push_value(args->frame, values.start[i]);
  // Push the appropriate record on there too, the interpreter will grab it
  // off before doing the method lookup.
  value_t escape_records = ROOT(args->runtime, escape_records);
  value_t record = get_array_at(escape_records, values.length);
  frame_push_value(args->frame, record);
  return new_signal_condition(true);
}

value_t add_builtin_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t arg_count, builtin_implementation_t impl,
    int leave_argc) {
  CHECK_FAMILY(ofIdHashMap, map);
  assembler_t assm;
  TRY_FINALLY {
    E_TRY(assembler_init(&assm, runtime, nothing(), scope_get_bottom()));
    if (leave_argc == -1) {
      // Simple case where there can be no signals.
      E_TRY(assembler_emit_builtin(&assm, impl));
      E_TRY(assembler_emit_return(&assm));
    } else {
      short_buffer_cursor_t dest;
      size_t code_start_offset = assembler_get_code_cursor(&assm);
      // Invoke the builtin. This will either keep going or, if there is a
      // failure, jump to the destination.
      E_TRY(assembler_emit_builtin_maybe_escape(&assm, impl, leave_argc, &dest));
      E_TRY(assembler_emit_return(&assm));
      size_t code_end_offset = assembler_get_code_cursor(&assm);
      short_buffer_cursor_set(&dest, (short_t) (code_end_offset - code_start_offset));
      // If we failed the signal state will have been pushed onto the stack so
      // adjust the stack height to reflect that.
      assembler_adjust_stack_height(&assm, leave_argc);
      E_TRY(assembler_emit_leave_or_fire_barrier(&assm, leave_argc));
    }
    E_TRY_DEF(code_block, assembler_flush(&assm));
    E_TRY_DEF(name, new_heap_utf8(runtime, new_c_string(name_c_str)));
    E_TRY_DEF(builtin, new_heap_builtin_implementation(runtime, afFreeze,
        name, code_block, arg_count, new_flag_set(kFlagSetAllOff)));
    E_RETURN(set_id_hash_map_at(runtime, map, name, builtin));
  } FINALLY {
    assembler_dispose(&assm);
  } YRT
}

value_t add_custom_method_impl(runtime_t *runtime, value_t map,
    const char *name_c_str, size_t posc, value_t method_flags,
    custom_method_emitter_t emitter) {
  CHECK_FAMILY(ofIdHashMap, map);
  // Build the implementation.
  assembler_t assm;
  TRY(assembler_init(&assm, runtime, nothing(), scope_get_bottom()));
  TRY(emitter(&assm));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(code_block, assembler_flush(&assm));
  assembler_dispose(&assm);
  TRY_DEF(name, new_heap_utf8(runtime, new_c_string(name_c_str)));
  TRY_DEF(builtin, new_heap_builtin_implementation(runtime, afFreeze,
      name, code_block, posc, method_flags));
  return set_id_hash_map_at(runtime, map, name, builtin);
}

value_t add_builtin_implementations(runtime_t *runtime, safe_value_t s_map) {
  add_integer_builtin_implementations(runtime, s_map);
  add_heap_object_builtin_implementations(runtime, s_map);

  // The family built-ins.
#define __EMIT_FAMILY_BUILTINS_CALL__(Family, family, MD, SR, MINOR, N)        \
  SR(                                                                          \
    TRY(add_##family##_builtin_implementations(runtime, s_map));,              \
    )
  ENUM_HEAP_OBJECT_FAMILIES(__EMIT_FAMILY_BUILTINS_CALL__)
#undef __EMIT_ADD_BUILTINS_CALL__

  // The phylum built-ins.
#define __EMIT_PHYLUM_BUILTINS_CALL__(Phylum, phylum, SR, MINOR, N)            \
  SR(                                                                          \
    TRY(add_##phylum##_builtin_implementations(runtime, s_map));,              \
    )
  ENUM_CUSTOM_TAGGED_PHYLUMS(__EMIT_PHYLUM_BUILTINS_CALL__)
#undef __EMIT_PHYLUM_BUILTINS_CALL__

  return success();
}
