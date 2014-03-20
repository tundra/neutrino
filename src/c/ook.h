//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// ## OOK!
///
/// Primitive support for virtual object types in C. Because C has limited
/// syntactic abstraction it's _very_ difficult to build a proper object system
/// purely in C (hence C++) so this is just stuff to help what is essentially
/// a completely by-convention object system. The conventions are as follows.
///
/// Object types have names that end with `_o` -- as in `point_o`. Each object
/// type has a set of associated methods, for instance a point would have a
/// `get_x` and `get_y` method with types:
///
///     // Returns the point's x coordinate.
///     typedef int32_t (*point_get_x_m)(point_o *self);
///
///     // Returns the point's y coordinate.
///     typedef int32_t (*point_get_y_m)(point_o *self);
///
/// Method types end with `_m`. The methods live in a vtable, called the
/// typename plus `vtable_t`,
///
///     // Vtable for point.
///     typedef struct {
///       point_get_x_m get_x;
///       point_get_y_m get_y;
///     } point_vtable_t;
///
/// The point type itself has a pointer to the vtable as its first field,
///
///     struct point_o {
///       point_vtable_t *vtable;
///     }
///
/// Calling a point method is verbose; use the `METHOD` macro like so,
///
///     point_o *p = ...;
///     int32_t x = METHOD(p, get_x)(p);
///     int32_t x = METHOD(p, get_y)(p);
///
/// To create a subclass you create implementations of all the methods;
/// implementations are allowed to be covariant in the `self` parameter, so you
/// can do,
///
///     static int32_t cartesian_get_x(cartesian_o *self) {
///       return self->x;
///     }
///
///     static int32_t cartesian_get_y(cartesian_o *self) {
///       return self->y;
///     }
///
/// You pack the methods into a vtable, casting as necessary to account for the
/// covariance,
///
///     static point_vtable_t kCartesianVTable = {
///       (point_get_x_m) cartesian_get_x,
///       (point_get_y_m) cartesian_get_y
///     };
///
/// The type itself has the supertype inline as its first field,
///
///     struct cartesian_o {
///       point_o super;
///       int32_t x;
///       int32_t y;
///     }
///
/// To create an instance either, depending on what's most convenient, return
/// an instance like so,
///
///     cartesian_o cartesian_new(int32_t x, int32_t y) {
///       cartesian_o result;
///       result.super.vtable = &kCartesianVTable;
///       result.x = x;
///       result.y = y;
///       return result;
///     }
///
/// or initialize an instance like so,
///
///     void cartesian_init(cartesian_o *self, int32_t x, int32_t y) {
///       self->super.vtable = &kCartesianVTable;
///       self->x = x;
///       self->y = y;
///     }
///
/// To pass a cartesian as a point use the `UPCAST` macro,
///
///     cartesian_o cartesian = cartesian_new(10, 11);
///     point_o *point = UPCAST(&cartesian);
///
/// For deeper hierarchies you'll need to upcast as many steps as the distance
/// you're upcasting,

#ifndef _OOK
#define _OOK

// Returns the given object viewed as its immediate supertype.
#define UPCAST(OBJ) &((OBJ)->super)

// Returns a function pointer to the method with the given name.
#define METHOD(OBJ, NAME) ((OBJ)->vtable->NAME)

#endif // _OOK
