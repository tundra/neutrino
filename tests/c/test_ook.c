// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

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
  VTABLE_FIELD(point_o);
};

IMPLEMENTATION(cartesian_o, point_o);

struct cartesian_o {
  point_o super;
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
}
