// Runtime assertions.

#include "globals.h"

#ifndef _CHECK
#define _CHECK

// External function used to signal a check failure.
void check_fail(const char *file, int line);

// Fails if the given expression doesn't evaluate to true.
#define CHECK_TRUE(E) do {          \
  if (!(E))                         \
    check_fail(__FILE__, __LINE__); \
} while (false)

// Fails if the given expression doesn't evaluate to false.
#define CHECK_FALSE(E) CHECK_TRUE(!(E))

// Fails if the given values aren't equal.
#define CHECK_EQ(A, B) CHECK_TRUE((A) == (B))

// Check that fails unless the value is in the specified domain.
#define CHECK_DOMAIN(vdDomain, EXPR) CHECK_EQ(vdDomain, get_value_domain(EXPR))

// Check that fails unless the object is in the specified family.
#define CHECK_FAMILY(ofFamily, EXPR) CHECK_EQ(ofFamily, get_object_family(EXPR))

// Fails if executed.
#define UNREACHABLE() CHECK_TRUE(false)

#endif // _CHECK
