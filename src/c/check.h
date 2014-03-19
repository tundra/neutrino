// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Runtime assertions.


#ifndef _CHECK
#define _CHECK

#include "crash.h"
#include "globals.h"

// Define the IF_CHECKS_ENABLED macro appropriately. There is a choice here
// between whether to completely remove check-related code when checks are
// disabled which may lead to unused variable warnings (because you're removing
// the use of variables) or alternatively including the code but ensure that
// it doesn't get executed, which may lead to illegal code in the case where the
// code only compiles in checked mode. This does the former, includes the code,
// because you can always surround a check with an #ifdef if there's a problem
// whereas there's no simple workaround for unused variables.
#ifdef ENABLE_CHECKS
#define IF_CHECKS_ENABLED(V) V
#define IF_CHECKS_DISABLED(V) USE(V)
#else
#define IF_CHECKS_ENABLED(V) USE(V)
#define IF_CHECKS_DISABLED(V) V
#endif

#ifdef EXPENSIVE_CHECKS
#define IF_EXPENSIVE_CHECKS_ENABLED(V) V
#else
#define IF_EXPENSIVE_CHECKS_ENABLED(V) USE(V)
#endif

// Fails unless the two values are equal.
#define CHECK_EQ(M, A, B)                                                      \
IF_CHECKS_ENABLED(CHECK_REL(M, A, ==, B))

// Fails unless the two pointer values are equal.
#define CHECK_PTREQ(M, A, B)                                                   \
IF_CHECKS_ENABLED(CHECK_TRUE(M, ((void*) (A)) == ((void*) (B))))

#define __COND_CHECK_EQ_WITH_VALUE_HELPER__(M, scCause, VALUE, A, B) do {      \
  if (!((A) == (B))) {                                                         \
    cond_check_fail(__FILE__, __LINE__, scCause,                               \
        "Check failed (" M "): %s == %s.", #A, #B);                            \
    return (VALUE)            ;                                                \
  }                                                                            \
} while (false)

// Fails unless the two values are equal under hard check failures, returns the
// specified value under soft check failures. If the value you want to return
// is a condition with the given cause, which is the common case, it's easier to
// use COND_CHECK_EQ.
#define COND_CHECK_EQ_WITH_VALUE(M, scCause, VALUE, A, B)                      \
IF_CHECKS_ENABLED(__COND_CHECK_EQ_WITH_VALUE_HELPER__(M, scCause, VALUE, A, B))

// Fails unless the two values are equal under hard check failures, returns the
// specified condition under soft check failures.
#define COND_CHECK_EQ(M, scCause, A, B)                                        \
IF_CHECKS_ENABLED(__COND_CHECK_EQ_WITH_VALUE_HELPER__(M, scCause,              \
    new_condition(scCause), A, B))

// Fails if the given expression doesn't evaluate to true.
#define CHECK_TRUE(M, E)                                                       \
IF_CHECKS_ENABLED(CHECK_EQ(M, E, true))

// Fails if the given expression doesn't evaluate to the true boolean value_t.
#define CHECK_TRUE_VALUE(M, E)                                                 \
IF_CHECKS_ENABLED(CHECK_EQ(M, get_boolean_value(E), true))

// Used by CHECK_REL. Don't call directly.
#define __CHECK_REL_HELPER__(M, A, OP, B) do {                                 \
  int64_t __a__ = (int64_t) (A);                                               \
  int64_t __b__ = (int64_t) (B);                                               \
  if (!(__a__ OP __b__))                                                       \
    check_fail(__FILE__, __LINE__,                                             \
        "Check failed (" M "): %s %s %s.\n"                                    \
        "  Left: %lli\n"                                                       \
        "  Right: %lli",                                                       \
        #A, #OP, #B, __a__, __b__);                                            \
} while (false)

// Fail unless A and B are related as OP.
#define CHECK_REL(M, A, OP, B)                                                 \
IF_CHECKS_ENABLED(__CHECK_REL_HELPER__(M, A, OP, B))

// Fails if the given expression doesn't evaluate to true under hard check
// failures, returns the given value under soft check failures.
#define COND_CHECK_TRUE_WITH_VALUE(M, scCause, VALUE, E)                       \
IF_CHECKS_ENABLED(COND_CHECK_EQ_WITH_VALUE(M, scCause, VALUE, E, true))

// Fails if the given expression doesn't evaluate to true under hard check
// failures, returns the specified condition under soft check failures.
#define COND_CHECK_TRUE(M, scCause, E)                                         \
IF_CHECKS_ENABLED(COND_CHECK_EQ(M, scCause, E, true))

// Fails if the given expression doesn't evaluate to false.
#define CHECK_FALSE(M, E)                                                      \
IF_CHECKS_ENABLED(CHECK_EQ(M, E, false))

// Fails if the given expression doesn't evaluate to the false value_t.
#define CHECK_FALSE_VALUE(M, E)                                                \
IF_CHECKS_ENABLED(CHECK_EQ(M, get_boolean_value(E), false))

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

// Check that a given value is either nothing or belongs to a particular class,
// where the class is given by calling a particular getter on the value. You
// generally don't want to use this directly.
#define __CHECK_CLASS_OPT__(class_t, cExpected, EXPR, get_class) do {          \
  value_t __value__ = (EXPR);                                                  \
  if (!is_nothing(__value__)) {                                                \
    class_t __class__ = get_class(__value__);                                  \
    if (__class__ != cExpected) {                                              \
      const char *__class_name__ = get_class##_name(__class__);                \
      check_fail(__FILE__, __LINE__, "Check failed: %s(%s) == %s.\n  Found: %s", \
          #get_class, #EXPR, #cExpected, __class_name__);                      \
    }                                                                          \
  }                                                                            \
} while (false)

// Check that fails unless the value is in the specified domain.
#define CHECK_DOMAIN(vdDomain, EXPR)                                           \
IF_CHECKS_ENABLED(__CHECK_CLASS__(value_domain_t, vdDomain, EXPR, get_value_domain))

// Works the same way as CHECK_DOMAIN but can be used in hot code because it
// gets enabled and disabled as an expensive check.
#define CHECK_DOMAIN_HOT(vdDomain, EXPR)                                       \
IF_EXPENSIVE_CHECKS_ENABLED(CHECK_DOMAIN(vdDomain, EXPR))

// Check that fails unless the object is in the specified family or nothing.
#define CHECK_DOMAIN_OPT(vdDomain, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS_OPT__(value_domain_t, vdDomain, EXPR, get_value_domain))

// Check that fails unless the value is a custom tagged value in the given phylum.
#define CHECK_PHYLUM(tpPhylum, EXPR)                                           \
IF_CHECKS_ENABLED(__CHECK_CLASS__(custom_tagged_phylum_t, tpPhylum, EXPR, get_custom_tagged_phylum))

// Check that fails unless the value is a custom tagged value in the given phylum
// or is nothing.
#define CHECK_PHYLUM_OPT(tpPhylum, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS_OPT__(custom_tagged_phylum_t, tpPhylum, EXPR, get_custom_tagged_phylum))

// Check that fails unless the object is in the specified family.
#define CHECK_FAMILY(ofFamily, EXPR)                                           \
IF_CHECKS_ENABLED(__CHECK_CLASS__(heap_object_family_t, ofFamily, EXPR, get_heap_object_family))

// Check that fails unless the object is in the specified family.
#define CHECK_GENUS(dgGenus, EXPR)                                             \
IF_CHECKS_ENABLED(__CHECK_CLASS__(derived_object_genus_t, dgGenus, EXPR, get_derived_object_genus))

// Check that fails unless the object is nothing or in the specified genus.
#define CHECK_GENUS_OPT(dgGenus, EXPR)                                         \
IF_CHECKS_ENABLED(__CHECK_CLASS_OPT__(derived_object_genus_t, dgGenus, EXPR, get_derived_object_genus))

// Check that fails unless the given expression is in a mutable mode.
#define CHECK_MUTABLE(EXPR)                                                    \
IF_CHECKS_ENABLED(CHECK_TRUE("mutable", is_mutable(EXPR)))

// Check that fails unless the given expression is in a mutable mode.
#define CHECK_DEEP_FROZEN(EXPR)                                                \
IF_CHECKS_ENABLED(CHECK_TRUE("deep frozen", peek_deep_frozen(EXPR)))

// Check that fails unless the object is in the specified family or nothing.
#define CHECK_FAMILY_OPT(ofFamily, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS_OPT__(heap_object_family_t, ofFamily, EXPR, get_heap_object_family))

// Check that fails unless the object is in a syntax family or nothing.
#define CHECK_SYNTAX_FAMILY_OPT(EXPR)                                          \
IF_CHECKS_ENABLED(__CHECK_CLASS_OPT__(bool, true, EXPR, in_syntax_family))

// Check that fails unless the species is in the specified division.
#define CHECK_DIVISION(sdDivision, EXPR)                                       \
IF_CHECKS_ENABLED(__CHECK_CLASS__(species_division_t, sdDivision, EXPR, get_species_division))

// Fails if executed. These aren't disabled when checks are off since they
// won't get executed under normal circumstances so they're free.
#define UNREACHABLE(M) do {                                                    \
  check_fail(__FILE__, __LINE__, "Unreachable (" M ").");                      \
} while (false)

// Check that a given value belongs to a particular class and otherwise returns
// a condition, where the class is given by calling a particular getter on the
// value. You generally don't want to use this directly.
#define EXPECT_CLASS(scCause, class_t, cExpected, EXPR, get_class) do {        \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected)                                                  \
    return new_condition(scCause);                                             \
} while (false)

// Check that returns a condition unless the object is in the specified family.
#define EXPECT_FAMILY(scCause, ofFamily, EXPR)                                 \
EXPECT_CLASS(scCause, heap_object_family_t, ofFamily, EXPR, get_heap_object_family)

#endif // _CHECK
