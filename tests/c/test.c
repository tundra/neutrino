#include <stdio.h>
#include <stdlib.h>
#include "test.h"

// Data associated with a particular test run.
typedef struct {
  int argc;
  char **argv;
} unit_test_data_t;


// Macros used in the generated TOC file.
#define ENUMERATE_TESTS_HEADER void enumerate_tests(unit_test_data_t *data)
#define ENUMERATE_TEST(suite, name) run_unit_test(#suite, #name, data, test_##suite##_##name)
#define DECLARE_TEST(suite, name) extern TEST(suite, name);


// Callback invoked for each test case by the test enumerator.
void run_unit_test(const char *suite, const char *name, unit_test_data_t *data, void (unit_test)()) {
  printf("- %s/%s\n", suite, name);
  unit_test();
}


// Include the generated TOC.
#include "toc.c"


// Run!
int main(int argc, char *argv[]) {
  unit_test_data_t data = {argc, argv};
  enumerate_tests(&data);
  return 0;
}


void fail(const char *error, const char *file, int line) {
  printf("%s:%i: %s\n", file, line, error);
  exit(1);
}
