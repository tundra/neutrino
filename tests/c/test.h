#include "value.h"

// Declare a unit test method. The suite name must match the file the test
// case is declared in.
#define TEST(suite, name) void test_##suite##_##name()


// Aborts exception, signalling an error.
void fail(const char *file, int line, const char *fmt, ...);

// Returns true iff the two values are structurally equal.
bool value_structural_equal(value_t a, value_t b);

// Fails unless the two values are equal.
#define ASSERT_EQ(A, B) do {                                         \
  if (!((A) == (B)))                                                 \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.", #A, #B); \
} while (false)

// Fails unless the two given strings are equal.
#define ASSERT_STREQ(A, B) do {                                               \
  string_t *__a__ = (A);                                                      \
  string_t *__b__ = (B);                                                      \
  if (!(string_equals(__a__, __b__)))                                         \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.\n  Expected: %s\n  Found: %s", \
        #A, #B, __a__->chars, __b__->chars);                                  \
} while (false)

// Check that the two values A and B are structurally equivalent. Note that this
// only handles object trees, not cyclical graphs of objects.
#define ASSERT_VALEQ(A, B) do {                                        \
  value_t __a__ = (A);                                                 \
  value_t __b__ = (B);                                                 \
  if (!(value_structural_equal(__a__, __b__))) {                       \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.\n", #A, #B); \
  }                                                                    \
} while (false)

// Fails unless the condition is true.
#define ASSERT_TRUE(COND) ASSERT_EQ(COND, true)

// Fails unless the condition is false.
#define ASSERT_FALSE(COND) ASSERT_EQ(COND, false)

// Fails unless the given value is within the given domain.
#define ASSERT_DOMAIN(vdDomain, EXPR) \
ASSERT_CLASS(value_domain_t, vdDomain, EXPR, get_value_domain)

// Fails unless the given value is within the given family.
#define ASSERT_FAMILY(ofFamily, EXPR) \
ASSERT_CLASS(object_family_t, ofFamily, EXPR, get_object_family)

// Fails unless the given value is a signal of the given type.
#define ASSERT_SIGNAL(scCause, EXPR) \
ASSERT_CLASS(signal_cause_t, scCause, EXPR, get_signal_cause)

#define ASSERT_CLASS(class_t, cExpected, EXPR, get_class) do {                 \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected)                                                  \
    fail(__FILE__, __LINE__, "Assertion failed: %s(%s) == %s.\n  Found: %i",   \
        #get_class, #EXPR, #cExpected, __class__);                             \
} while (false)

// Fails if the given value is a signal.
#define ASSERT_SUCCESS(EXPR) do {                                              \
  value_t __value__ = (EXPR);                                                  \
  if (get_value_domain(__value__) == vdSignal) {                               \
    fail(__FILE__, __LINE__, "Assertion failed: %s is a value.\n  Was signal: %i",\
        #EXPR, get_signal_cause(__value__));                                   \
  }                                                                            \
} while (false)

// Declares a new string_t variable and initializes it with the given contents.
#define DEF_STRING(name, contents) \
string_t name;                     \
string_init(&name, contents)
