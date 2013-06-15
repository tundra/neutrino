// Declares a unit test case.

// Declare a unit test method. The suite name must match the file the test
// case is declared in.
#define TEST(suite, name) void test_##suite##_##name()


// Aborts exception, signalling an error.
extern void fail(const char *error, const char *file, int line);

// Fails unless the condition is true.
#define ASSERT_TRUE(COND) do { \
  if (!(COND)) \
    fail("Assertion failed.", __FILE__, __LINE__); \
} while (0)

// Fails unless the condition is false.
#define ASSERT_FALSE(COND) ASSERT_TRUE(!(COND))

// Fails unless the two values are equal.
#define ASSERT_EQ(A, B) ASSERT_TRUE((A) == (B))

// Fails unless the given value is within the given domain.
#define ASSERT_DOMAIN(vdDomain, EXPR) \
  ASSERT_EQ(vdDomain, get_value_domain(EXPR))

// Fails unless the given value is within the given family.
#define ASSERT_FAMILY(ofFamily, EXPR) \
  ASSERT_TRUE(in_family(ofFamily, EXPR))
