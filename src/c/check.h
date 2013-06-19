// Runtime assertions.

#include "globals.h"

#ifndef _CHECK
#define _CHECK

// External function used to signal a check failure.
void check_fail(const char *file, int line, const char *fmt, ...);

// Fails unless the two values are equal.
#define CHECK_EQ(M, A, B) do {                                           \
  if (!((A) == (B)))                                                     \
    check_fail(__FILE__, __LINE__, "Check failed (" M "): %s == %s.",    \
        #A, #B);                                                         \
} while (false)

// Fails if the given expression doesn't evaluate to true.
#define CHECK_TRUE(M, E) CHECK_EQ(M, E, true)

// Fails if the given expression doesn't evaluate to false.
#define CHECK_FALSE(M, E) CHECK_EQ(M, E, false)

// Check that a given value belongs to a particular class, where the class is
// given by calling a particular getter on the value. You generally don't want
// to use this directly.
#define CHECK_CLASS(class_t, cExpected, EXPR, get_class) do {                  \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected)                                                  \
    check_fail(__FILE__, __LINE__, "Check failed: %s(%s) == %s.\n  Found: %i", \
        #get_class, #EXPR, #cExpected, __class__);                             \
} while (false)

// Check that fails unless the value is in the specified domain.
#define CHECK_DOMAIN(vdDomain, EXPR) \
CHECK_CLASS(value_domain_t, vdDomain, EXPR, get_value_domain)

// Check that fails unless the object is in the specified family.
#define CHECK_FAMILY(ofFamily, EXPR) \
CHECK_CLASS(object_family_t, ofFamily, EXPR, get_object_family)

// Fails if executed.
#define UNREACHABLE(M) do {                                                    \
  check_fail(__FILE__, __LINE__, "Unreachable (" M ").");                      \
} while (false)

#endif // _CHECK
