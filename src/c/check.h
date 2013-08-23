// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Runtime assertions.

#include "crash.h"
#include "globals.h"

#ifndef _CHECK
#define _CHECK

#ifdef ENABLE_CHECKS
#define IF_CHECKS_ENABLED(V) V
#define IF_CHECKS_DISABLED(V) USE(V)
#else
#define IF_CHECKS_ENABLED(V) USE(V)
#define IF_CHECKS_DISABLED(V) V
#endif

#define __CHECK_EQ_HELPER__(M, A, B) do {                                      \
  if (!((A) == (B)))                                                           \
    check_fail(__FILE__, __LINE__, "Check failed (" M "): %s == %s.",          \
        #A, #B);                                                               \
} while (false)

// Fails unless the two values are equal.
#define CHECK_EQ(M, A, B)                                                      \
IF_CHECKS_ENABLED(__CHECK_EQ_HELPER__(M, A, B))

#define __SIG_CHECK_EQ_HELPER__(M, scCause, A, B) do {                         \
  if (!((A) == (B))) {                                                         \
    sig_check_fail(__FILE__, __LINE__, scCause,                                \
        "Check failed (" M "): %s == %s.", #A, #B);                            \
    return new_signal(scCause);                                                \
  }                                                                            \
} while (false)

// Fails unless the two values are equal under hard check failures, returns the
// specified signal under soft check failures.
#define SIG_CHECK_EQ(M, scCause, A, B)                                         \
IF_CHECKS_ENABLED(__SIG_CHECK_EQ_HELPER__(M, scCause, A, B))

// Fails if the given expression doesn't evaluate to true.
#define CHECK_TRUE(M, E)                                                       \
IF_CHECKS_ENABLED(CHECK_EQ(M, E, true))

// Fails if the given expression doesn't evaluate to true under hard check
// failures, returns the specified signal under soft check failures.
#define SIG_CHECK_TRUE(M, scCause, E)                                          \
IF_CHECKS_ENABLED(SIG_CHECK_EQ(M, scCause, E, true))

// Fails if the given expression doesn't evaluate to false.
#define CHECK_FALSE(M, E)                                                      \
IF_CHECKS_ENABLED(CHECK_EQ(M, E, false))

// Check that a given value belongs to a particular class, where the class is
// given by calling a particular getter on the value. You generally don't want
// to use this directly.
#define __CHECK_CLASS__(class_t, cExpected, EXPR, get_class) do {              \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected) {                                                \
    const char *__class_name__ = get_class##_name(__class__);                  \
    check_fail(__FILE__, __LINE__, "Check failed: %s(%s) == %s.\n  Found: %s", \
        #get_class, #EXPR, #cExpected, __class_name__);                        \
  }                                                                            \
} while (false)

// Check that fails unless the value is in the specified domain.
#define CHECK_DOMAIN(vdDomain, EXPR)                                           \
IF_CHECKS_ENABLED(__CHECK_CLASS__(value_domain_t, vdDomain, EXPR, get_value_domain))

// Check that fails unless the object is in the specified family.
#define CHECK_FAMILY(ofFamily, EXPR)                                           \
IF_CHECKS_ENABLED(__CHECK_CLASS__(object_family_t, ofFamily, EXPR, get_object_family))

// Check that fails unless the species is in the specified division.
#define CHECK_DIVISION(sdDivision, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS__(species_division_t, sdDivision, EXPR, get_species_division))

// Fails if executed. These aren't disabled when checks are off since they
// won't get executed under normal circumstances so they're free.
#define UNREACHABLE(M) do {                                                    \
  check_fail(__FILE__, __LINE__, "Unreachable (" M ").");                      \
} while (false)

// Check that a given value belongs to a particular class and otherwise returns
// a signal, where the class is given by calling a particular getter on the
// value. You generally don't want to use this directly.
#define EXPECT_CLASS(scCause, class_t, cExpected, EXPR, get_class) do {     \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected)                                                  \
    return new_signal(scCause);                                                \
} while (false)

// Check that returns a signal unless the object is in the specified family.
#define EXPECT_FAMILY(scCause, ofFamily, EXPR)                                 \
EXPECT_CLASS(scCause, object_family_t, ofFamily, EXPR, get_object_family)

#endif // _CHECK
