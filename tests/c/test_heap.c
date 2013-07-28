// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "heap.h"
#include "test.h"


TEST(heap, init) {
  space_config_t config;
  space_config_init_defaults(&config);
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
  ASSERT_EQ((address_t) 0, align_address(4, (address_t) 0));
  ASSERT_EQ((address_t) 4, align_address(4, (address_t) 1));
  ASSERT_EQ((address_t) 4, align_address(4, (address_t) 4));
  ASSERT_EQ((address_t) 8, align_address(4, (address_t) 5));
#ifdef M64
  ASSERT_EQ((address_t) 0x2ba3b9505010, align_address(8, (address_t) 0x2ba3b9505010));
#endif
}

TEST(heap, space_alloc) {
  // Configure the space.
  space_config_t config;
  space_config_init_defaults(&config);
  config.size_bytes = kKB;
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
