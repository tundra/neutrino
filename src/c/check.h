// Runtime assertions.


// External function used to signal a check failure.
void check_fail(const char *file, int line);


// Fails if the given expression doesn't evaluate to true.
#define CHECK_TRUE(E) do {          \
  if (!(E))                         \
    check_fail(__FILE__, __LINE__); \
} while (false)


// Fails if the given values aren't equal.
#define CHECK_EQ(A, B) CHECK_TRUE((A) == (B))
