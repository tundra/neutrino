#include "check.h"
#include "test.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


// Data associated with a particular test run.
typedef struct {
  const char *suite;
} unit_test_data_t;


// Macros used in the generated TOC file.
#define ENUMERATE_TESTS_HEADER void enumerate_tests(unit_test_data_t *data)
#define ENUMERATE_TEST(suite, name) run_unit_test(#suite, #name, data, test_##suite##_##name)
#define DECLARE_TEST(suite, name) extern TEST(suite, name)


// Callback invoked for each test case by the test enumerator.
void run_unit_test(const char *suite, const char *name, unit_test_data_t *data,
    void (unit_test)()) {
  if (!data->suite || (strcmp(suite, data->suite) == 0)) {
    printf("- %s/%s\n", suite, name);
    unit_test();
  }
}


// Include the generated TOC.
#include "toc.c"


// Run!
int main(int argc, char *argv[]) {
  if (argc >= 2) {
    // If there are arguments run the relevant test suites.
    for (int i = 1; i < argc; i++) {
      unit_test_data_t data = {argv[i]};
      enumerate_tests(&data);
    }
    return 0;
  } else {
    // If there are no arguments just run everything.
    unit_test_data_t data = {NULL};
    enumerate_tests(&data);
  }
}


// Dump an error on test failure.
void fail(const char *file, int line, const char *fmt, ...) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Print the formatted error message.
  printf("%s:%i: %s\n", file, line, str.chars);
  string_buffer_dispose(&buf);
  abort();
}
