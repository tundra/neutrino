//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "io.h"
#include "sync.h"
#include "undertaking.h"
END_C_INCLUDES

template <typename S>
void check_undertaking() {
  // The cast between an undertaking viewed as its concrete state and
  // undertaking_t must be the identity function. If this fails maybe you didn't
  // have undertaking_t as_undertaking as the first member?
  S *state = NULL;
  ASSERT_PTREQ(state, UPCAST_UNDERTAKING(state));
}

TEST(undertaking, casts) {
#define __CHECK_UNDERTAKING__(Name, name, type_t) check_undertaking<type_t>();
  ENUM_UNDERTAKINGS(__CHECK_UNDERTAKING__)
#undef __CHECK_UNDERTAKING__
}
