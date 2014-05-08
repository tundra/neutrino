//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Includes of C headers from C++ files should be surrounded by these macros to
// ensure that they're linked appropriately.
#if defined(__cplusplus)
#define BEGIN_C_INCLUDES extern "C" {
#define END_C_INCLUDES }
#else
#define BEGIN_C_INCLUDES
#define END_C_INCLUDES
#endif

BEGIN_C_INCLUDES
#include "globals.h"
#include "test.h"
END_C_INCLUDES

// Data that picks out a particular test or suite to run.
struct unit_test_selector_t {
  // If non-null, the test suite to run.
  char *suite;
  // If non-null, the test case to run.
  char *name;
};

class TestInfo {
public:
  typedef void (*unit_test_t)();
  TestInfo(const char *suite, const char *name, unit_test_t unit_test);
  static void run_tests(unit_test_selector_t *selector);
  void run();
  bool matches(unit_test_selector_t *selector);
private:
  static TestInfo *chain;
  const char *suite;
  const char *name;
  unit_test_t unit_test;
  TestInfo *next;
};

#define NEW_TEST(suite, name)                                                  \
  void run_##suite##_##name();                                                 \
  TestInfo* const test_info_##suite##_##name = new TestInfo(#suite, #name, run_##suite##_##name); \
  void run_##suite##_##name()
