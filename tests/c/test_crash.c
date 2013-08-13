// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.h"

static value_t do_check(bool value, signal_cause_t cause) {
  SIG_CHECK_TRUE("foo", cause, value);
  return success();
}

TEST(crash, soft_check_failures) {
#ifndef CHECKS_ENABLED
  return;
#endif
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
