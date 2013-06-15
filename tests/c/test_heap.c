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
  CHECK_EQ(0, align_size(4, 0));
  CHECK_EQ(4, align_size(4, 1));
  CHECK_EQ(4, align_size(4, 2));
  CHECK_EQ(4, align_size(4, 3));
  CHECK_EQ(4, align_size(4, 4));
  CHECK_EQ(8, align_size(4, 5));
  CHECK_EQ(0, align_size(8, 0));
  CHECK_EQ(8, align_size(8, 1));
  CHECK_EQ(8, align_size(8, 2));
  CHECK_EQ(8, align_size(8, 7));
  CHECK_EQ(8, align_size(8, 8));
  CHECK_EQ(16, align_size(8, 9));
}

TEST(heap, align_address) {
  CHECK_EQ((address_t) 0, align_address(4, (address_t) 0));
  CHECK_EQ((address_t) 4, align_address(4, (address_t) 1));
  CHECK_EQ((address_t) 4, align_address(4, (address_t) 4));
  CHECK_EQ((address_t) 8, align_address(4, (address_t) 5));
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
  CHECK_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  CHECK_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  CHECK_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  CHECK_TRUE(space_try_alloc(&space, kKB / 4, &addr));
  CHECK_FALSE(space_try_alloc(&space, 1, &addr));

  // Clean up.
  space_dispose(&space);
}
