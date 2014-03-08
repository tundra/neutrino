// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.h"

static value_t do_check(bool value, consition_cause_t cause) {
  COND_CHECK_TRUE("foo", cause, value);
  return success();
}

TEST(crash, soft_check_failures) {
  IF_CHECKS_DISABLED(return);
  check_recorder_t recorder;
  install_check_recorder(&recorder);

  ASSERT_EQ(0, recorder.count);
  ASSERT_SUCCESS(do_check(true, ccNothing));
  ASSERT_EQ(0, recorder.count);
  ASSERT_CONDITION(ccOutOfBounds, do_check(false, ccOutOfBounds));
  ASSERT_EQ(1, recorder.count);
  ASSERT_EQ(ccOutOfBounds, recorder.last_cause);
  ASSERT_CONDITION(ccNotFound, do_check(false, ccNotFound));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(ccNotFound, recorder.last_cause);
  ASSERT_SUCCESS(do_check(true, ccSystemError));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(ccNotFound, recorder.last_cause);

  check_recorder_t inner;
  install_check_recorder(&inner);
  ASSERT_CONDITION(ccOutOfBounds, do_check(false, ccOutOfBounds));
  ASSERT_EQ(2, recorder.count);
  ASSERT_EQ(ccNotFound, recorder.last_cause);
  ASSERT_EQ(1, inner.count);
  ASSERT_EQ(ccOutOfBounds, inner.last_cause);
  uninstall_check_recorder(&inner);

  ASSERT_CONDITION(ccOutOfBounds, do_check(false, ccOutOfBounds));
  ASSERT_EQ(3, recorder.count);
  ASSERT_EQ(ccOutOfBounds, recorder.last_cause);

  uninstall_check_recorder(&recorder);
}

TEST(crash, checks_disabled) {
  IF_CHECKS_ENABLED(return);
  CHECK_TRUE("test", false);
  CHECK_FALSE("test", true);
  CHECK_EQ("test", 1, 2);
  CHECK_FAMILY(ofString, new_integer(0));
  CHECK_DOMAIN(vdHeapObject, new_integer(0));
  CHECK_DIVISION(sdCompact, new_integer(0));
}
