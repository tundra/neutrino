//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// Internal plugin support. This is mainly just a way to organize built-in
/// libraries, it's not for external consumption.

#ifndef _PLUGIN
#define _PLUGIN

#include "async/promise.h"

#include "builtin.h"
#include "value.h"

// Description of a method on a c object.
typedef struct {
  // The name of the method.
  const char *selector;
  // The number of positional arguments.
  size_t posc;
  // Native implementation.
  builtin_implementation_t impl;
} c_object_method_t;

// Expands to a built-in method struct with the given string selector,
// positional argument count, and native implementation.
#define BUILTIN_METHOD(SEL, POSC, IMPL)                                        \
  { (SEL), (POSC), (IMPL) }

// Description of the layout of a C object. Note that unlike fully general
// built-in objects, all C object instances of the same species must have the
// same layout. To get variable size data or field count use a value field that
// holds a blob or array.
typedef struct {
  // Size in bytes of the data stored in the object.
  size_t data_size;
  // Number of field values stored in the object.
  size_t value_count;
} c_object_layout_t;

// Full description of a c object.
struct c_object_info_t {
  // The object's layout.
  c_object_layout_t layout;
  // Description of the methods.
  const c_object_method_t *methods;
  // Number of methods supported by this type.
  size_t method_count;
  // The tag used to identify instances. An instance of this type will return
  // this value from get_c_object_tag.
  value_t tag;
};

// Clears all the state in the given c object info.
void c_object_info_reset(c_object_info_t *info);

// Sets the methods to make available for instances created from this object
// descriptor.
void c_object_info_set_methods(c_object_info_t *info, const c_object_method_t *methods,
    size_t method_count);

// Sets the tag used to identify instances.
void c_object_info_set_tag(c_object_info_t *info, value_t tag);

// Sets the values used to determine the layout of instances.
void c_object_info_set_layout(c_object_info_t *info, size_t data_size,
    size_t value_count);

// Returns the tag that was given to the constructor of the factory that was
// used to produce the given object.
value_t get_c_object_tag(value_t self);

// Creates a new object that can be used to produce C objects. The object's
// methods are installed in the given methodspace.
value_t new_c_object_factory(runtime_t *runtime, const c_object_info_t *info,
    value_t methodspace);

// Creates a new c object instance from the given factory.
value_t new_c_object(runtime_t *runtime, value_t factory, blob_t data,
    value_array_t values);

// Data associated with a request issued from the vm to a native remote
// implementation.
typedef struct {
  opaque_promise_t *promise;
} native_request_t;

// Deliver the given value as the successful result of the given request. If the
// request has already been resolved, successfully or not, this does nothing.
// Returns true iff it did something.
bool native_request_fulfill(native_request_t *request, value_t value);

// Callback called by the runtime to schedule a request. The result of the
// request must be delivered by fulfilling the promise the is given as part
// of the request. The request struct is guaranteed to be alive at only until
// the promise is fulfilled.
typedef void (*schedule_request_t)(native_request_t *request);

// Data associated with a native remote request handler.
typedef struct {
  const char *display_name;
  schedule_request_t schedule_request;
} native_remote_t;

// Creates a native remote object that delivers requests through the given
// implementation. The implementation struct must be valid as long as the native
// remote wrapper is used.
value_t new_native_remote(runtime_t *runtime, native_remote_t *impl);

// Returns a native remote that implements the time api.
native_remote_t *native_remote_time();

// Returns a value that has been wrapped in an opaque.
static inline value_t o2v(opaque_t opaque) {
  return decode_value(o2u(opaque));
}

// Returns an opaque that wraps the given value.
static inline opaque_t v2o(value_t value) {
  return u2o(value.encoded);
}

#endif // _PLUGIN
