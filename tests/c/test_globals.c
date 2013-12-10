// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "globals.h"
#include "test.h"

TEST(globals, pointer_size) {
#ifdef IS_32_BIT
  ASSERT_EQ(4, sizeof(void*));
#endif

#ifdef IS_64_BIT
  ASSERT_EQ(8, sizeof(void*));
#endif

  ASSERT_EQ(IF_32_BIT(4, 8), sizeof(void*));
  ASSERT_EQ(IF_64_BIT(8, 4), sizeof(void*));
  ASSERT_EQ(sizeof(void*), sizeof(address_arith_t));
}
