//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "ctrino.h"
#include "freeze.h"
#include "utils/log.h"
#include "value-inl.h"

const char *get_c_object_int_tag_name(builtin_tag_t tag) {
  switch (tag) {
#define __EMIT_TAG_CASE__(Name) case bt##Name: return #Name;
  FOR_EACH_BUILTIN_TAG(__EMIT_TAG_CASE__)
#undef __EMIT_DOMAIN_CASE__
    default:
      return "invalid builtin tag";
  }
}

static int32_t get_c_object_int_tag(value_t self) {
  return get_integer_value(get_c_object_tag(self));
}


// --- F r a m e w o r k ---

// Builds a signature for the built-in ctrino method with the given name and
// positional argument count.
static value_t build_builtin_method_signature(runtime_t *runtime,
    const c_object_method_t *method, value_t subject, value_t selector) {
  size_t argc = method->posc + 2;
  TRY_DEF(vector, new_heap_pair_array(runtime, argc));
  // The subject parameter.
  TRY_DEF(subject_guard, new_heap_guard(runtime, afFreeze, gtIs, subject));
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
  for (size_t i = 0; i < method->posc; i++) {
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
    string_t name_str;
    string_init(&name_str, method->selector);
    E_TRY_DEF(name, new_heap_string(runtime, &name_str));
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


/// ## Ctrino

static value_t ctrino_get_builtin_type(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t name = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
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
  CHECK_C_OBJECT_TAG(btCtrino, self);
  return new_heap_function(runtime, afMutable, display_name);
}

static value_t ctrino_new_instance_manager(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t display_name = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  return new_heap_instance_manager(get_builtin_runtime(args), display_name);
}

static value_t ctrino_new_array(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t length = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  CHECK_DOMAIN(vdInteger, length);
  return new_heap_array(get_builtin_runtime(args), get_integer_value(length));
}

static value_t ctrino_new_float_32(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t decimal = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
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
  CHECK_C_OBJECT_TAG(btCtrino, self);
  INFO("%9v", value);
  return null();
}

static value_t ctrino_print_ln(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  print_ln("%9v", value);
  return value;
}

static value_t ctrino_to_string(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  CHECK_C_OBJECT_TAG(btCtrino, self);
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
  CHECK_C_OBJECT_TAG(btCtrino, self);
  return new_heap_builtin_marker(runtime, name);
}

static value_t ctrino_delay(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  value_t thunk = get_builtin_argument(args, 0);
  value_t promise = get_builtin_argument(args, 1);
  value_t guard = get_builtin_argument(args, 2);
  if (is_null(guard))
    guard = nothing();
  CHECK_C_OBJECT_TAG(btCtrino, self);
  CHECK_FAMILY(ofLambda, thunk);
  CHECK_FAMILY_OPT(ofPromise, promise);
  CHECK_FAMILY_OPT(ofPromise, guard);
  value_t process = get_builtin_process(args);
  runtime_t *runtime = get_builtin_runtime(args);
  job_t job;
  job_init(&job, ROOT(runtime, call_thunk_code_block), thunk, promise, guard);
  TRY(offer_process_job(runtime, process, &job));
  return null();
}

static value_t ctrino_freeze(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  TRY(ensure_frozen(runtime, value));
  return null();
}

static value_t ctrino_is_frozen(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  return new_boolean(is_frozen(value));
}

static value_t ctrino_is_deep_frozen(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  value_t value = get_builtin_argument(args, 0);
  runtime_t *runtime = get_builtin_runtime(args);
  return new_boolean(try_validate_deep_frozen(runtime, value, NULL));
}

static value_t ctrino_new_pending_promise(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  runtime_t *runtime = get_builtin_runtime(args);
  return new_heap_pending_promise(runtime);
}

#define BUILTIN_METHOD(SUBJ, SEL, ARGC, IMPL)                                  \
  { (SEL), (ARGC), (IMPL) }

#define kCtrinoMethodCount 15
static const c_object_method_t kCtrinoMethods[kCtrinoMethodCount] = {
  BUILTIN_METHOD(ctrino, "builtin", 1, ctrino_builtin),
  BUILTIN_METHOD(ctrino, "delay", 3, ctrino_delay),
  BUILTIN_METHOD(ctrino, "freeze", 1, ctrino_freeze),
  BUILTIN_METHOD(ctrino, "get_builtin_type", 1, ctrino_get_builtin_type),
  BUILTIN_METHOD(ctrino, "get_current_backtrace", 0, ctrino_get_current_backtrace),
  BUILTIN_METHOD(ctrino, "is_deep_frozen?", 1, ctrino_is_deep_frozen),
  BUILTIN_METHOD(ctrino, "is_frozen?", 1, ctrino_is_frozen),
  BUILTIN_METHOD(ctrino, "log_info", 1, ctrino_log_info),
  BUILTIN_METHOD(ctrino, "new_array", 1, ctrino_new_array),
  BUILTIN_METHOD(ctrino, "new_float_32", 1, ctrino_new_float_32),
  BUILTIN_METHOD(ctrino, "new_function", 1, ctrino_new_function),
  BUILTIN_METHOD(ctrino, "new_instance_manager", 1, ctrino_new_instance_manager),
  BUILTIN_METHOD(ctrino, "new_pending_promise", 0, ctrino_new_pending_promise),
  BUILTIN_METHOD(ctrino, "print_ln", 1, ctrino_print_ln),
  BUILTIN_METHOD(ctrino, "to_string", 1, ctrino_to_string)
};

value_t create_ctrino_factory(runtime_t *runtime, value_t space) {
  c_object_info_t ctrino_info;
  c_object_info_reset(&ctrino_info);
  c_object_info_set_methods(&ctrino_info, kCtrinoMethods, kCtrinoMethodCount);
  c_object_info_set_tag(&ctrino_info, new_integer(btCtrino));
  return new_c_object_factory(runtime, &ctrino_info, space);
}

/// ## C object species

void get_c_object_species_layout(value_t value, heap_object_layout_t *layout) {
  heap_object_layout_set(layout, kCObjectSpeciesSize, kSpeciesHeaderSize);
}

void c_object_info_reset(c_object_info_t *info) {
  info->data_size = 0;
  info->aligned_data_size = 0;
  info->value_count = 0;
  info->methods = NULL;
  info->method_count = 0;
  info->tag = nothing();
}

void c_object_info_set_methods(c_object_info_t *info, const c_object_method_t *methods,
    size_t method_count) {
  info->methods = methods;
  info->method_count = method_count;
}

void c_object_info_set_tag(c_object_info_t *info, value_t tag) {
  info->tag = tag;
}

void c_object_info_set_layout(c_object_info_t *info, size_t data_size,
    size_t value_count) {
  info->data_size = data_size;
  info->aligned_data_size = align_size(kValueSize, data_size);
  info->value_count = value_count;
}

void get_c_object_species_object_info(value_t raw_self, c_object_info_t *info_out) {
  value_t self = chase_moved_object(raw_self);
  // Access the fields directly rather than use the accessors because the
  // accessors assume the heap is in a consistent state which it may not be
  // because of gc when this is called.
  size_t data_size = get_integer_value(*access_heap_object_field(self,
      kCObjectSpeciesDataSizeOffset));
  size_t value_count = get_integer_value(*access_heap_object_field(self,
      kCObjectSpeciesValueCountOffset));
  c_object_info_reset(info_out);
  c_object_info_set_layout(info_out, data_size, value_count);
}

CHECKED_SPECIES_ACCESSORS_IMPL(CObject, c_object, CObject, c_object,
    acInDomain, vdInteger, DataSize, data_size);

CHECKED_SPECIES_ACCESSORS_IMPL(CObject, c_object, CObject, c_object,
    acInDomain, vdInteger, ValueCount, value_count);

CHECKED_SPECIES_ACCESSORS_IMPL(CObject, c_object, CObject, c_object,
    acInFamily, ofType, Type, type);

CHECKED_SPECIES_ACCESSORS_IMPL(CObject, c_object, CObject, c_object,
    acNoCheck, 0, Tag, tag);


/// ## C object
///
/// Some C data and functionality exposed through a neutrino object.

NO_BUILTIN_METHODS(c_object);

size_t calc_c_object_size(c_object_info_t *info) {
  return kCObjectHeaderSize
       + info->aligned_data_size
       + (info->value_count * kValueSize);
}

value_mode_t get_c_object_mode(value_t self) {
  value_t mode = *access_heap_object_field(self, kCObjectModeOffset);
  return (value_mode_t) get_integer_value(mode);
}

value_t get_c_object_primary_type(value_t self, runtime_t *runtime) {
  value_t species = get_heap_object_species(self);
  return get_c_object_species_type(species);
}

void set_c_object_mode_unchecked(runtime_t *runtime, value_t self, value_mode_t mode) {
  *access_heap_object_field(self, kCObjectModeOffset) = new_integer(mode);
}

value_t c_object_validate(value_t value) {
  VALIDATE_FAMILY(ofCObject, value);
  return success();
}

void c_object_print_on(value_t value, print_on_context_t *context) {
  string_buffer_printf(context->buf, "c_object");
}

void get_c_object_layout(value_t self, heap_object_layout_t *layout) {
  value_t species = get_heap_object_species(self);
  c_object_info_t info;
  get_c_object_species_object_info(species, &info);
  size_t size = calc_c_object_size(&info);
  heap_object_layout_set(layout, size, kCObjectHeaderSize + info.aligned_data_size);
}

byte_t *get_c_object_data_start(value_t self) {
  CHECK_FAMILY(ofCObject, self);
  return (byte_t*) access_heap_object_field(self, kCObjectHeaderSize);
}

blob_t get_mutable_c_object_data(value_t self) {
  CHECK_FAMILY(ofCObject, self);
  CHECK_MUTABLE(self);
  value_t species = get_heap_object_species(self);
  value_t data_size_val = get_c_object_species_data_size(species);
  return new_blob(get_c_object_data_start(self), get_integer_value(data_size_val));
}

value_t *get_c_object_value_start(value_t self) {
  CHECK_FAMILY(ofCObject, self);
  value_t species = get_heap_object_species(self);
  c_object_info_t info;
  get_c_object_species_object_info(species, &info);
  return access_heap_object_field(self, kCObjectHeaderSize + info.aligned_data_size);
}

static value_array_t get_c_object_values(value_t self) {
  CHECK_FAMILY(ofCObject, self);
  value_t species = get_heap_object_species(self);
  value_t value_count_val = get_c_object_species_value_count(species);
  return new_value_array(get_c_object_value_start(self),
      get_integer_value(value_count_val));
}

value_array_t get_mutable_c_object_values(value_t self) {
  CHECK_MUTABLE(self);
  return get_c_object_values(self);
}

value_t get_c_object_value_at(value_t self, size_t index) {
  value_array_t values = get_c_object_values(self);
  COND_CHECK_TRUE("c object value index out of bounds", ccOutOfBounds,
      index < values.length);
    return values.start[index];
}

value_t new_c_object_factory(runtime_t *runtime, c_object_info_t *info,
    value_t methodspace) {
  TRY_DEF(subject, new_heap_type(runtime, afFreeze, nothing(), nothing()));
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
