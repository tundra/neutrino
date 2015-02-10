//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "plugin.h"
#include "async/promise.h"

// Builds a signature for the built-in ctrino method with the given name and
// positional argument count.
static value_t build_builtin_method_signature(runtime_t *runtime,
    const c_object_method_t *method, value_t subject, value_t selector) {
  size_t argc = method->posc + 2;
  TRY_DEF(tags, new_heap_pair_array(runtime, argc));
  // The subject parameter.
  TRY_DEF(subject_guard, new_heap_guard(runtime, afFreeze, gtIs, subject));
  TRY_DEF(subject_param, new_heap_parameter(runtime, afFreeze, subject_guard,
      ROOT(runtime, subject_key_array), false, 0));
  set_pair_array_first_at(tags, 0, ROOT(runtime, subject_key));
  set_pair_array_second_at(tags, 0, subject_param);
  // The selector parameter.
  TRY_DEF(name_guard, new_heap_guard(runtime, afFreeze, gtEq, selector));
  TRY_DEF(name_param, new_heap_parameter(runtime, afFreeze, name_guard,
      ROOT(runtime, selector_key_array), false, 1));
  set_pair_array_first_at(tags, 1, ROOT(runtime, selector_key));
  set_pair_array_second_at(tags, 1, name_param);
  // The positional parameters.
  for (size_t i = 0; i < method->posc; i++) {
    TRY_DEF(param, new_heap_parameter(runtime, afFreeze,
        ROOT(runtime, any_guard), ROOT(runtime, empty_array), false, 2 + i));
    set_pair_array_first_at(tags, 2 + i, new_integer(i));
    set_pair_array_second_at(tags, 2 + i, param);
  }
  co_sort_pair_array(tags);
  return new_heap_signature(runtime, afFreeze, tags, argc, argc, false);
}

// Add a ctrino method to the given method space with the given name, number of
// arguments, and implementation.
static value_t add_builtin_method(runtime_t *runtime, const c_object_method_t *method,
    value_t subject, value_t space) {
  CHECK_FAMILY(ofMethodspace, space);
  E_BEGIN_TRY_FINALLY();
    // Build the implementation.
    assembler_t assm;
    E_TRY(assembler_init(&assm, runtime, nothing(), scope_get_bottom()));
    E_TRY(assembler_emit_builtin(&assm, method->impl));
    E_TRY(assembler_emit_return(&assm));
    E_TRY_DEF(code_block, assembler_flush(&assm));
    // Build the signature.
    E_TRY_DEF(name, new_heap_utf8(runtime, new_c_string(method->selector)));
    E_TRY_DEF(selector, new_heap_operation(runtime, afFreeze, otInfix, name));
    E_TRY_DEF(signature, build_builtin_method_signature(runtime, method, subject, selector));
    E_TRY_DEF(method,new_heap_method(runtime, afFreeze, signature, nothing(),
        code_block, nothing(), new_flag_set(kFlagSetAllOff)));
    // And in the methodspace bind them.
    E_RETURN(add_methodspace_method(runtime, space, method));
  E_FINALLY();
    assembler_dispose(&assm);
  E_END_TRY_FINALLY();
}

value_t new_c_object_factory(runtime_t *runtime, const c_object_info_t *info,
    value_t methodspace) {
  TRY_DEF(subject, new_heap_type(runtime, afFreeze, nothing()));
  TRY_DEF(species, new_heap_c_object_species(runtime, afFreeze, info, subject));
  for (size_t i = 0; i < info->method_count; i++) {
    const c_object_method_t *method = &info->methods[i];
    TRY(add_builtin_method(runtime, method, subject, methodspace));
  }
  return species;
}

value_t new_c_object(runtime_t *runtime, value_t factory, blob_t data,
    value_array_t values) {
  return new_heap_c_object(runtime, afFreeze, factory, data, values);
}

value_t new_native_remote(runtime_t *runtime, native_remote_t *impl) {
  return new_heap_native_remote(runtime, impl);
}

bool native_request_fulfill(native_request_t *request, value_t value) {
  return opaque_promise_fulfill(request->promise, v2o(value));
}

#include "utils/log.h"

void schedule_time_request(native_request_t *request) {
  native_request_fulfill(request, new_integer(0));
}

native_remote_t *native_remote_time() {
  static native_remote_t kInstance = {"Time", schedule_time_request};
  return &kInstance;
}
