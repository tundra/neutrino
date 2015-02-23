//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"
#include "include/neutrino.hh"

TEST(neutrino, runtime_init_failure) {
  neutrino::RuntimeConfig config;
  // Way too small, initialization will definitely fail.
  config.semispace_size_bytes = 1024;
  neutrino::Runtime runtime;
  neutrino::Maybe<void> result = runtime.initialize(&config);
  ASSERT_FALSE(result.has_value());
  ASSERT_TRUE(result.message() != NULL);
}
