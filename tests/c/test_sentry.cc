//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "value.h"
END_C_INCLUDES

static value_t try_test_simple() {
  EXPECT_SENTRY(snInDomain(vdInteger), new_integer(0));
  return success();
}

TEST(sentry, simple) {
  value_t error;
  ASSERT_TRUE(SENTRY_TEST(snInDomain(vdInteger), new_integer(0), &error));
  ASSERT_FALSE(SENTRY_TEST(snInDomain(vdInteger), nothing(), &error));
  ASSERT_SUCCESS(try_test_simple());
  CHECK_SENTRY(snInDomain(vdInteger), new_integer(0));
  CHECK_SENTRY(snNoCheck, new_integer(0));
}
