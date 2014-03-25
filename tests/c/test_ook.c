//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "ook-inl.h"
#include "test.h"
#include "value-inl.h"

INTERFACE(point_o);

typedef int32_t (*point_get_x_m)(point_o *self);
typedef int32_t (*point_get_y_m)(point_o *self);

struct point_o_vtable_t {
  point_get_x_m get_x;
  point_get_y_m get_y;
};

struct point_o {
  INTERFACE_HEADER(point_o);
};

IMPLEMENTATION(cartesian_o, point_o);

struct cartesian_o {
  IMPLEMENTATION_HEADER(cartesian_o, point_o);
  int32_t x;
  int32_t y;
};

int32_t cartesian_get_x(point_o *super_self) {
  cartesian_o *self = DOWNCAST(cartesian_o, super_self);
  return self->x;
}

int32_t cartesian_get_y(point_o *super_self) {
  cartesian_o *self = DOWNCAST(cartesian_o, super_self);
  return self->y;
}

VTABLE(cartesian_o, point_o) { cartesian_get_x, cartesian_get_y };

cartesian_o cartesian_new(int32_t x, int32_t y) {
  cartesian_o result;
  VTABLE_INIT(cartesian_o, UPCAST(&result));
  result.x = x;
  result.y = y;
  return result;
}

IMPLEMENTATION(origin_o, point_o);

int32_t origin_get_x(point_o *self) {
  return 0;
}

int32_t origin_get_y(point_o *self) {
  return 0;
}

VTABLE(origin_o, point_o) { origin_get_x, origin_get_y };

point_o origin_new() {
  point_o result;
  VTABLE_INIT(origin_o, &result);
  return result;
}

INTERFACE(point3d_o);

typedef int32_t (*point3d_get_z_m)(point3d_o *self);

struct point3d_o_vtable_t {
  point_o_vtable_t super;
  point3d_get_z_m get_z;
};

struct point3d_o {
  SUB_INTERFACE_HEADER(point3d_o, point_o);
};

IMPLEMENTATION(cartesian3d_o, point3d_o);

struct cartesian3d_o {
  IMPLEMENTATION_HEADER(cartesian3d_o, point3d_o);
  int32_t x;
  int32_t y;
  int32_t z;
};

int32_t cartesian3d_get_x(point_o *super_self) {
  cartesian3d_o *self = DOWNCAST(cartesian3d_o, super_self);
  return self->x;
}

int32_t cartesian3d_get_y(point_o *super_self) {
  cartesian3d_o *self = DOWNCAST(cartesian3d_o, super_self);
  return self->y;
}

int32_t cartesian3d_get_z(point3d_o *super_self) {
  cartesian3d_o *self = DOWNCAST(cartesian3d_o, super_self);
  return self->z;
}

VTABLE(cartesian3d_o, point3d_o) {
  { cartesian3d_get_x, cartesian3d_get_y },
  cartesian3d_get_z
};

cartesian3d_o cartesian3d_new(int32_t x, int32_t y, int32_t z) {
  cartesian3d_o result;
  VTABLE_INIT(cartesian3d_o, UPCAST(&result));
  result.y = y;
  result.x = x;
  result.z = z;
  return result;
}

TEST(ook, interaction) {
  cartesian_o c = cartesian_new(3, 8);
  point_o *pc = UPCAST(&c);
  ASSERT_TRUE(IS_INSTANCE(cartesian_o, pc));
  ASSERT_FALSE(IS_INSTANCE(origin_o, pc));
  ASSERT_EQ(3, METHOD(pc, get_x)(pc));
  ASSERT_EQ(8, METHOD(pc, get_y)(pc));
  ASSERT_FALSE(DOWNCAST(cartesian_o, pc) == NULL);

  point_o z = origin_new();
  point_o *pz = &z;
  ASSERT_TRUE(IS_INSTANCE(origin_o, pz));
  ASSERT_FALSE(IS_INSTANCE(cartesian_o, pz));
  ASSERT_EQ(0, METHOD(pz, get_x)(pz));
  ASSERT_EQ(0, METHOD(pz, get_y)(pz));
  ASSERT_FALSE(DOWNCAST(origin_o, pz) == NULL);

  cartesian3d_o c3 = cartesian3d_new(78, 2, 4);
  point3d_o *ppc3 = UPCAST(&c3);
  ASSERT_TRUE(IS_INSTANCE(cartesian3d_o, ppc3));
  ASSERT_FALSE(IS_INSTANCE(cartesian_o, ppc3));
  ASSERT_FALSE(IS_INSTANCE(origin_o, ppc3));
  ASSERT_EQ(78, METHOD(ppc3, super.get_x)(UPCAST(ppc3)));
  ASSERT_EQ(2, METHOD(ppc3, super.get_y)(UPCAST(ppc3)));
  ASSERT_EQ(4, METHOD(ppc3, get_z)(ppc3));
  point_o *pc3 = UPCAST(ppc3);
  ASSERT_TRUE(IS_INSTANCE(cartesian3d_o, pc3));
  ASSERT_FALSE(IS_INSTANCE(cartesian_o, pc3));
  ASSERT_FALSE(IS_INSTANCE(origin_o, pc3));
  ASSERT_EQ(78, METHOD(pc3, get_x)(pc3));
  ASSERT_EQ(2, METHOD(pc3, get_y)(pc3));
}
