//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// ## OOK!
///
/// Primitive support for virtual object types in C. Because C has limited
/// syntactic abstraction it's _very_ difficult to build a proper object system
/// purely in C (hence C++) so this is just stuff to help what is essentially
/// a completely by-convention object system. The system is somewhat cumbersome
/// and the primary concern has been to have reasonable support with a minimum
/// of potential security issues, so a minimum of casting around.
///
/// There are interfaces and implementations. You forward declare them using the
/// `INTERFACE` and `IMPLEMENTATION` macros,
///
///     INTERFACE(point_o);
///     IMPLEMENTATION(cartesian_o, point_o);
///
/// Object types have names that end with `_o` -- as in `point_o`. Each object
/// type has a set of associated methods, for instance a `point_o` would have a
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
///     struct point_o_vtable_t {
///       point_get_x_m get_x;
///       point_get_y_m get_y;
///     };
///
/// The point type itself has a pointer to the vtable in its header, which must
/// be the first part of the struct,
///
///     struct point_o {
///       INTERFACE_HEADER(point_o);
///     }
///
/// Calling a point method is verbose; use the `METHOD` macro like so,
///
///     point_o *p = ...;
///     int32_t x = METHOD(p, get_x)(p);
///     int32_t x = METHOD(p, get_y)(p);
///
/// To create a subclass you create implementations of all the methods. The
/// method signatures must be the same as the declared methods so you need to
/// downcast the `self` parameter, which you can do safely using this pattern:
///
///     static int32_t cartesian_get_x(point_o *super_self) {
///       cartesian_o *self = DOWNCAST(cartesian_o, super_self);
///       return self->x;
///     }
///
///     static int32_t cartesian_get_y(point_o *super_self) {
///       cartesian_o *self = DOWNCAST(cartesian_o, super_self);
///       return self->y;
///     }
///
/// You pack the methods into a vtable using the `VTABLE` macro,
///
///     VTABLE(cartesian_o, point_o) {
///       cartesian_get_x,
///       cartesian_get_y
///     };
///
/// The type itself has an implementation header inline as its first field,
///
///     struct cartesian_o {
///       IMPLEMENTATION_HEADER(cartesian_o, point_o);
///       int32_t x;
///       int32_t y;
///     }
///
/// To create an instance either, depending on what's most convenient, return
/// an instance like so,
///
///     cartesian_o cartesian_new(int32_t x, int32_t y) {
///       cartesian_o result;
///       VTABLE_INIT(cartesian_o, UPCAST(&result));
///       result.x = x;
///       result.y = y;
///       return result;
///     }
///
/// or initialize an instance like so,
///
///     void cartesian_init(cartesian_o *self, int32_t x, int32_t y) {
///       VTABLE_INIT(cartesian_o, UPCAST(&self));
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

// Declares an interface of the given name.
#define INTERFACE(name_o)                                                      \
  FORWARD(name_o);                                                             \
  FORWARD(name_o##_vtable_t)

// Declares an implementation with the given name of the given interface.
#define IMPLEMENTATION(name_o, super_o)                                        \
  FORWARD(name_o);                                                             \
  extern struct super_o##_vtable_t name_o##_vtable

// Expands to a declaration of the vtable for the given implementation type.
#define VTABLE(name_o, super_o) struct super_o##_vtable_t name_o##_vtable =

// Expands to an initializer that sets the given object's vtable.
#define VTABLE_INIT(name_o, OBJ) (OBJ)->header.vtable = &name_o##_vtable

// Expands to the vtable field declaration for the specified interface type.
#define INTERFACE_HEADER(name_o) union {                                       \
  name_o##_vtable_t *vtable;                                                   \
} header;

// Expands to the header for an interface type that extends the given super
// type.
#define SUB_INTERFACE_HEADER(name_o, super_o) union {                          \
  name_o##_vtable_t *vtable;                                                   \
  super_o super;                                                               \
} header

// Expands to the header for the specified implementation type.
#define IMPLEMENTATION_HEADER(name_o, super_o) union {                         \
  super_o##_vtable_t *vtable;                                                  \
  super_o super;                                                               \
} header

// Returns the given object viewed as its immediate supertype.
#define UPCAST(OBJ) (&((OBJ)->header.super))

// Returns a function pointer to the method with the given name.
#define METHOD(OBJ, NAME) ((OBJ)->header.vtable->NAME)

#endif // _OOK
