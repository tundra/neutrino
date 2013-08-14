// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.h"

static value_t do_check(bool value, signal_cause_t cause) {
  SIG_CHECK_TRUE("foo", cause, value);
  return success();
}

TEST(crash, soft_check_failures) {
  IF_CHECKS_DISABLED(return);
  check_recorder_t recorder;
  install_check_recorder(&recorder);

  ASSERT_EQ(0, recorder.count);
  ASSERT_SUCCESS(do_check(true, scNothing));
  ASSERT_EQ(0, recorder.count);
  ASSERT_SIGNAL(scOutOfBounds, do_check(false, scOutOfBounds));
  ASSERT_EQ(1, recorder.count);
  ASSERT_EQ(scOutOfBounds, recorder.cause);
  ASSERT_SIGNAL(scNotFound, do_check(false, scNotFound));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(scNotFound, recorder.cause);
  ASSERT_SUCCESS(do_check(true, scSystemError));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(scNotFound, recorder.cause);

  check_recorder_t inner;
  install_check_recorder(&inner);
  ASSERT_SIGNAL(scOutOfBounds, do_check(false, scOutOfBounds));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(scNotFound, recorder.cause);
  ASSERT_EQ(1, inner.count);
  ASSERT_EQ(scOutOfBounds, inner.cause);
  uninstall_check_recorder(&inner);

  ASSERT_SIGNAL(scOutOfBounds, do_check(false, scOutOfBounds));
  ASSERT_EQ(3, recorder.count);
  ASSERT_EQ(scOutOfBounds, recorder.cause);

  uninstall_check_recorder(&recorder);
}

TEST(crash, checks_disabled) {
  IF_CHECKS_ENABLED(return);
  CHECK_TRUE("test", false);
  CHECK_FALSE("test", true);
  CHECK_EQ("test", 1, 2);
  CHECK_FAMILY(ofString, new_integer(0));
  CHECK_DOMAIN(vdObject, new_integer(0));
  CHECK_DIVISION(sdCompact, new_integer(0));
}
