//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "heap.h"
END_C_INCLUDES

TEST(heap, init) {
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  heap_t heap;
  heap_init(&heap, &config);
  heap_dispose(&heap);
}

TEST(heap, align_size) {
  ASSERT_EQ(0, align_size(4, 0));
  ASSERT_EQ(4, align_size(4, 1));
  ASSERT_EQ(4, align_size(4, 2));
  ASSERT_EQ(4, align_size(4, 3));
  ASSERT_EQ(4, align_size(4, 4));
  ASSERT_EQ(8, align_size(4, 5));
  ASSERT_EQ(0, align_size(8, 0));
  ASSERT_EQ(8, align_size(8, 1));
  ASSERT_EQ(8, align_size(8, 2));
  ASSERT_EQ(8, align_size(8, 7));
  ASSERT_EQ(8, align_size(8, 8));
  ASSERT_EQ(16, align_size(8, 9));
}

TEST(heap, align_address) {
#define ASSERT_ALIGN(E, A, V)                                                  \
  ASSERT_PTREQ((address_t) E, align_address((A), (address_t) V));

  ASSERT_ALIGN(0, 4, 0);
  ASSERT_ALIGN(4, 4, 1);
  ASSERT_ALIGN(4, 4, 4);
  ASSERT_ALIGN(8, 4, 5);
#ifdef IS_64_BIT
  ASSERT_ALIGN(0x2ba3b9505010, 8, 0x2ba3b9505010);
#endif
}

TEST(heap, space_alloc) {
  // Configure the space.
  runtime_config_t config;
  runtime_config_init_defaults(&config);
  config.semispace_size_bytes = kKB;
  space_t space;
  space_init(&space, &config);

  // Check that we can allocate all the memory but no more.
  address_t addr;
  ASSERT_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  ASSERT_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  ASSERT_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  ASSERT_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  ASSERT_FALSE(space_try_alloc(&space, 1, &addr));

  // Clean up.
  space_dispose(&space);
}

TEST(heap, clone) {
  CREATE_RUNTIME();

  value_t a0 = new_heap_array(runtime, 3);
  set_array_at(a0, 0, new_integer(8));
  set_array_at(a0, 1, new_integer(9));
  set_array_at(a0, 2, new_integer(10));

  value_t a1 = clone_heap_object(runtime, a0);
  ASSERT_FALSE(is_same_value(a0, a1));
  ASSERT_VALEQ(new_integer(8), get_array_at(a1, 0));
  ASSERT_VALEQ(new_integer(9), get_array_at(a1, 1));
  ASSERT_VALEQ(new_integer(10), get_array_at(a1, 2));

  set_array_at(a1, 1, new_integer(11));

  ASSERT_VALEQ(new_integer(8), get_array_at(a0, 0));
  ASSERT_VALEQ(new_integer(9), get_array_at(a0, 1));
  ASSERT_VALEQ(new_integer(10), get_array_at(a0, 2));
  ASSERT_VALEQ(new_integer(8), get_array_at(a1, 0));
  ASSERT_VALEQ(new_integer(11), get_array_at(a1, 1));
  ASSERT_VALEQ(new_integer(10), get_array_at(a1, 2));

  DISPOSE_RUNTIME();
}

TEST(heap, zappers) {
  // Check that memory cleared with the heap markers result in the memory
  // holding somewhat meaningful values.
  value_t zapped;
  memset(&zapped.encoded, kUnusedHeapMarker, sizeof(zapped.encoded));
  ASSERT_EQ(vdCustomTagged, get_value_domain(zapped));
  ASSERT_EQ(tpUnusedMemory, get_custom_tagged_phylum(zapped));
  memset(&zapped.encoded, kAllocatedHeapMarker, sizeof(zapped.encoded));
  ASSERT_EQ(vdCustomTagged, get_value_domain(zapped));
  ASSERT_EQ(tpAllocatedMemory, get_custom_tagged_phylum(zapped));
  memset(&zapped.encoded, kFreedHeapMarker, sizeof(zapped.encoded));
  ASSERT_EQ(vdCustomTagged, get_value_domain(zapped));
  ASSERT_EQ(tpFreedMemory, get_custom_tagged_phylum(zapped));
}
