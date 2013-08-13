// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "check.h"
#include "crash.h"
#include "test.h"
#include "utils.h"
#include "value-inl.h"

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
  install_crash_handler();
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

static bool array_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofArray, a);
  CHECK_FAMILY(ofArray, b);
  size_t length = get_array_length(a);
  if (get_array_length(b) != length)
    return false;
  for (size_t i = 0; i < length; i++) {
    if (!(value_structural_equal(get_array_at(a, i), get_array_at(b, i))))
      return false;
  }
  return true;
}

static bool id_hash_map_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofIdHashMap, a);
  CHECK_FAMILY(ofIdHashMap, b);
  if (get_id_hash_map_size(a) != get_id_hash_map_size(b))
    return false;
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, a);
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t a_value;
    id_hash_map_iter_get_current(&iter, &key, &a_value);
    value_t b_value = get_id_hash_map_at(b, key);
    if (is_signal(scNotFound, b_value))
      return false;
    if (!value_structural_equal(a_value, b_value))
      return false;
  }
  return true;
}

static bool instance_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofInstance, a);
  CHECK_FAMILY(ofInstance, b);
  return value_structural_equal(get_instance_fields(a), get_instance_fields(b));
}

static bool object_structural_equal(value_t a, value_t b) {
  CHECK_DOMAIN(vdObject, a);
  CHECK_DOMAIN(vdObject, b);
  object_family_t a_family = get_object_family(a);
  object_family_t b_family = get_object_family(b);
  if (a_family != b_family)
    return false;
  switch (a_family) {
    case ofArray:
      return array_structural_equal(a, b);
    case ofIdHashMap:
      return id_hash_map_structural_equal(a, b);
    case ofInstance:
      return instance_structural_equal(a, b);
    default:
      return value_identity_compare(a, b);
  }
}

bool value_structural_equal(value_t a, value_t b) {
  value_domain_t a_domain = get_value_domain(a);
  value_domain_t b_domain = get_value_domain(b);
  if (a_domain != b_domain)
    return false;
  switch (a_domain) {
    case vdObject:
      return object_structural_equal(a, b);
    default:
      return value_identity_compare(a, b);
  }
}

void recorder_abort_callback(void *data, const char *file, int line,
    int signal_cause, const char *message) {
  check_recorder_t *recorder = data;
  recorder->count++;
  recorder->cause = signal_cause;
}

void install_check_recorder(check_recorder_t *recorder) {
  recorder->count = 0;
  recorder->cause = __scFirst__;
  init_abort_callback(&recorder->callback, recorder_abort_callback, recorder);
  recorder->previous = set_abort_callback(&recorder->callback);
  CHECK_TRUE("no previous abort callback", recorder->previous != NULL);
}

void uninstall_check_recorder(check_recorder_t *recorder) {
  CHECK_TRUE("uninstalling again", recorder->previous != NULL);
  set_abort_callback(recorder->previous);
  recorder->previous = NULL;
}
