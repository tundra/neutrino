//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _OOK_INL
#define _OOK_INL

#include "check.h"

// Returns true iff the given object is an instance of the concrete implementation
// that has the specified vtable.
static bool ook_has_vtable(void *vtable, void *ptr) {
  typedef struct { void *vtable; } abstract_object_o;
  abstract_object_o *obj = (abstract_object_o*) ptr;
  void *found_vtable = obj->vtable;
  return found_vtable == vtable;
}

// Expands to an expression that says whether the given object is an instance
// of the specified implementation type. This is something you only ever want
// to use for testing and assertions, not for controlling normal execution.
#define IS_INSTANCE(impl_o, OBJ) ook_has_vtable(&impl_o##_vtable, (void*) (OBJ))

// Fails if the given pointer is not an instance with the given vtable.
// Otherwise return the pointer.
static void *ook_check_downcast(void *expected_vtable, void *ptr) {
  if (ook_has_vtable(expected_vtable, ptr)) {
    return ptr;
  } else {
    // Logically this shouldn't happen but if it does it's so bad that it's
    // worth checking for and crashing even in unchecked mode.
    UNREACHABLE("downcast failed");
    return NULL;
  }
}

// Returns the given object cast to the given subtype. Note that downcasting
// fails (in checked mode) unless the type you're casting to is the _concrete_
// type. Casting to a super implementation type doesn't work.
#define DOWNCAST(TYPE, OBJ) ((TYPE*) (ook_check_downcast(&TYPE##_vtable, OBJ)))

#endif // _OOK
