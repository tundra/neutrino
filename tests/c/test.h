// Declares a unit test case.

// Declare a unit test method. The suite name must match the file the test
// case is declared in.
#define TEST(suite, name) void test_##suite##_##name()


// Aborts exception, signalling an error.
extern void fail(const char *file, int line, const char *fmt, ...);

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

#define ASSERT_CLASS(class_t, cExpected, EXPR, get_class) do {               \
  class_t __class__ = get_class(EXPR);                                       \
  if (__class__ != cExpected)                                                \
    fail(__FILE__, __LINE__, "Assertion failed: %s(%s) == %s.\n  Found: %i", \
        #get_class, #EXPR, #cExpected, __class__);                           \
} while (false)

// Fails if the given value is a signal.
#define ASSERT_SUCCESS(EXPR) \
  ASSERT_FALSE(vdSignal == get_value_domain(EXPR))

// Declares a new string_t variable and initializes it with the given contents.
#define DEF_STRING(name, contents) \
string_t name;                     \
string_init(&name, contents)
