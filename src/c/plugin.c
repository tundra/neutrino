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

value_t new_native_remote(runtime_t *runtime, service_descriptor_t *impl) {
  return new_heap_native_remote(runtime, impl);
}

bool native_request_fulfill(native_request_t *request, pton_variant_t *result) {
  return opaque_promise_fulfill(request->impl_promise, p2o(result));
}

void service_method_init(service_method_t *method, pton_variant_t selector,
    unary_callback_t *callback) {
  method->selector = selector;
  method->callback = callback;
}

void service_descriptor_init(service_descriptor_t *remote,
    pton_variant_t namespace_name, pton_variant_t display_name,
    size_t methodc, service_method_t *methodv) {
  remote->namespace_name = namespace_name;
  remote->display_name = display_name;
  remote->methodc = methodc;
  remote->methodv = methodv;
}

opaque_t native_time_current(opaque_t opaque_request) {
  native_request_t *request = (native_request_t*) o2p(opaque_request);
  real_time_clock_t *clock = request->runtime->system_time;
  uint64_t time = real_time_clock_millis_since_epoch_utc(clock);
  pton_variant_t result = pton_integer(time);
  native_request_fulfill(request, &result);
  return opaque_null();
}

// Has this module's static initializer been run?
static bool has_run_static_init = false;

// Data for the native remote @time.
typedef struct {
  service_descriptor_t service;
  service_method_t methods[1];
} time_service_descriptor_t;

// Singleton @time implementation.
static time_service_descriptor_t time_impl;

// Initializes the singleton time instance.
static void run_time_impl_static_init() {
  service_method_init(&(time_impl.methods[0]), pton_c_str("current"),
      callback_invisible_clone(unary_callback_new_0(native_time_current)));
  service_descriptor_init(&time_impl.service, pton_c_str("time"),
      pton_c_str("Time"), 1, time_impl.methods);
}

service_descriptor_t *native_remote_time() {
  CHECK_TRUE("plugin statics not inited", has_run_static_init);
  return &time_impl.service;
}

void run_plugin_static_init() {
  has_run_static_init = true;
  run_time_impl_static_init();
}
