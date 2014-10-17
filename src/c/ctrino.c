//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "builtin.h"
#include "ctrino.h"
#include "freeze.h"
#include "utils/log.h"
#include "value-inl.h"

const char *get_c_object_int_tag_name(uint32_t tag) {
  switch (tag) {
#define __EMIT_TAG_CASE__(Name) case bt##Name: return #Name;
  FOR_EACH_BUILTIN_TAG(__EMIT_TAG_CASE__)
#undef __EMIT_DOMAIN_CASE__
    default:
      return "invalid builtin tag";
  }
}

static uint32_t get_c_object_int_tag(value_t self) {
  return get_integer_value(get_c_object_tag(self));
}


/// ## Ctrino

static value_t ctrino_get_builtin_type(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t name = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  CHECK_FAMILY(ofUtf8, name);
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

static value_t ctrino_new_plugin_instance(builtin_arguments_t *args) {
  runtime_t *runtime = get_builtin_runtime(args);
  value_t self = get_builtin_subject(args);
  value_t index = get_builtin_argument(args, 0);
  CHECK_C_OBJECT_TAG(btCtrino, self);
  CHECK_DOMAIN(vdInteger, index);
  value_t factory = get_runtime_plugin_factory_at(runtime, get_integer_value(index));
  return new_c_object(runtime, factory, new_blob(NULL, 0), new_value_array(NULL, 0));
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
  utf8_t as_string = string_buffer_flush(&buf);
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF(result, new_heap_utf8(runtime, as_string));
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

#define kCtrinoMethodCount 16
static const c_object_method_t kCtrinoMethods[kCtrinoMethodCount] = {
  BUILTIN_METHOD("builtin", 1, ctrino_builtin),
  BUILTIN_METHOD("delay", 3, ctrino_delay),
  BUILTIN_METHOD("freeze", 1, ctrino_freeze),
  BUILTIN_METHOD("get_builtin_type", 1, ctrino_get_builtin_type),
  BUILTIN_METHOD("get_current_backtrace", 0, ctrino_get_current_backtrace),
  BUILTIN_METHOD("is_deep_frozen?", 1, ctrino_is_deep_frozen),
  BUILTIN_METHOD("is_frozen?", 1, ctrino_is_frozen),
  BUILTIN_METHOD("log_info", 1, ctrino_log_info),
  BUILTIN_METHOD("new_array", 1, ctrino_new_array),
  BUILTIN_METHOD("new_float_32", 1, ctrino_new_float_32),
  BUILTIN_METHOD("new_function", 1, ctrino_new_function),
  BUILTIN_METHOD("new_instance_manager", 1, ctrino_new_instance_manager),
  BUILTIN_METHOD("new_pending_promise", 0, ctrino_new_pending_promise),
  BUILTIN_METHOD("new_plugin_instance", 1, ctrino_new_plugin_instance),
  BUILTIN_METHOD("print_ln", 1, ctrino_print_ln),
  BUILTIN_METHOD("to_string", 1, ctrino_to_string)
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
  info->layout.data_size = 0;
  info->layout.value_count = 0;
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
  info->layout.data_size = data_size;
  info->layout.value_count = value_count;
}

void get_c_object_species_layout_gc_tolerant(value_t raw_self,
    c_object_layout_t *layout_out) {
  value_t self = chase_moved_object(raw_self);
  // Access the fields directly rather than use the accessors because the
  // accessors assume the heap is in a consistent state which it may not be
  // because of gc when this is called.
  c_object_layout_t layout;
  layout.data_size = get_integer_value(*access_heap_object_field(self,
      kCObjectSpeciesDataSizeOffset));
  layout.value_count = get_integer_value(*access_heap_object_field(self,
      kCObjectSpeciesValueCountOffset));
  *layout_out = layout;
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

// Returns the offset in bytes at which the value section of a c object with the
// given data size starts.
static size_t calc_c_object_values_offset(size_t data_size) {
  return kCObjectHeaderSize + align_size(kValueSize, data_size);
}

size_t calc_c_object_size(c_object_layout_t *layout) {
  return calc_c_object_values_offset(layout->data_size)
       + (layout->value_count * kValueSize);
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
  c_object_layout_t info;
  get_c_object_species_layout_gc_tolerant(species, &info);
  size_t size = calc_c_object_size(&info);
  size_t values_offset = calc_c_object_values_offset(info.data_size);
  heap_object_layout_set(layout, size, values_offset);
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

size_t get_c_object_species_values_offset(value_t self) {
  CHECK_DIVISION(sdCObject, self);
  value_t data_size_val = get_c_object_species_data_size(self);
  return calc_c_object_values_offset(get_integer_value(data_size_val));
}

value_t *get_c_object_value_start(value_t self) {
  CHECK_FAMILY(ofCObject, self);
  value_t species = get_heap_object_species(self);
  size_t offset = get_c_object_species_values_offset(species);
  return access_heap_object_field(self, offset);
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
