// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "derived-inl.h"
#include "test.h"

TEST(derived, array) {
  CREATE_RUNTIME();

  value_t host = new_heap_array(runtime, 100);
  genus_descriptor_t *desc = get_genus_descriptor(dgStackPointer);

  for (size_t i = 0; i + desc->field_count <= 100; i += desc->field_count) {
    value_array_t b0 = alloc_array_block(host, i, desc->field_count);
    value_t p0 = new_derived_stack_pointer(runtime, b0, host);
    ASSERT_DOMAIN(vdDerivedObject, p0);
    ASSERT_TRUE(is_derived_object(p0));
    ASSERT_SAME(host, get_derived_object_host(p0));
    ASSERT_GENUS(dgStackPointer, p0);
    ASSERT_TRUE(in_genus(dgStackPointer, p0));
  }

  DISPOSE_RUNTIME();
}

TEST(derived, anchors) {
  value_t d0 = new_derived_object_anchor(dgStackPointer, 0);
  ASSERT_EQ(0, get_derived_object_anchor_host_offset(d0));

  uint64_t v1 = ((uint64_t) 1) << 31;
  value_t d1 = new_derived_object_anchor(dgStackPointer, v1);
  ASSERT_EQ(v1, get_derived_object_anchor_host_offset(d1));

  uint64_t v2 = kDerivedObjectAnchorOffsetLimit - 1;
  value_t d2 = new_derived_object_anchor(dgStackPointer, v2);
  ASSERT_EQ(v2, get_derived_object_anchor_host_offset(d2));
}
