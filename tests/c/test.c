// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "check.h"
#include "crash.h"
#include "log.h"
#include "runtime.h"
#include "test.h"
#include "utils.h"
#include "try-inl.h"
#include "value-inl.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// ARRRG! Features. Are. Awful!
#define __USE_POSIX199309
#include <time.h>
extern char *strdup(const char*);


// Data associated with a particular test run.
typedef struct {
  // If non-null, the test suite to run.
  char *suite;
  // If non-null, the test case to run.
  char *test;
} unit_test_data_t;

// Macros used in the generated TOC file.
#define ENUMERATE_TESTS_HEADER void enumerate_tests(unit_test_data_t *data)
#define ENUMERATE_TEST(suite, name) run_unit_test(#suite, #name, data, test_##suite##_##name)
#define DECLARE_TEST(suite, name) extern TEST(suite, name)

// Returns the current time since epoch counted in seconds.
static double get_current_time_seconds() {
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  return spec.tv_sec + (spec.tv_nsec / 1000000000.0);
}

// Match the given suite and test name against the given unit test data, return
// true iff the test should be run.
static bool match_test_data(unit_test_data_t *data, const char *suite, const char *test) {
  if (data->suite == NULL)
    return true;
  if (strcmp(suite, data->suite) != 0)
    return false;
  if (data->test == NULL)
    return true;
  return strcmp(test, data->test) == 0;
}

// Callback invoked for each test case by the test enumerator.
void run_unit_test(const char *suite, const char *test, unit_test_data_t *data,
    void (unit_test)()) {
  static const size_t kTimeColumn = 32;
  if (match_test_data(data, suite, test)) {
    printf("- %s/%s", suite, test);
    fflush(stdout);
    double start = get_current_time_seconds();
    unit_test();
    double end = get_current_time_seconds();
    for (size_t i = strlen(suite) + strlen(test); i < kTimeColumn; i++)
      putc(' ', stdout);
    printf(" (%.3fs)\n", (end - start));
    fflush(stdout);
  }
}


// Include the generated TOC.
#include "toc.c"

static void parse_test_data(const char *str, unit_test_data_t *data) {
  char *dupped = strdup(str);
  data->suite = dupped;
  char *test = strchr(dupped, '/');
  if (test != NULL) {
    test[0] = 0;
    data->test = &test[1];
  } else {
    data->test = NULL;
  }
}

static void dispose_test_data(unit_test_data_t *data) {
  free(data->suite);
}

// Run!
int main(int argc, char *argv[]) {
  install_crash_handler();
  if (argc >= 2) {
    // If there are arguments run the relevant test suites.
    for (int i = 1; i < argc; i++) {
      unit_test_data_t data;
      parse_test_data(argv[i], &data);
      enumerate_tests(&data);
      dispose_test_data(&data);
    }
    return 0;
  } else {
    // If there are no arguments just run everything.
    unit_test_data_t data = {NULL, NULL};
    enumerate_tests(&data);
  }
}


// Dump an error on test failure.
void fail(const char *file, int line, const char *fmt, ...) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Print the formatted error message.
  log_message(llError, file, line, "%s", str.chars);
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


// --- V a r i a n t s ---

value_t variant_to_value(runtime_t *runtime, variant_t variant) {
  switch (variant.type) {
    case vtInteger:
      return new_integer(variant.value.as_integer);
    case vtString: {
      string_t chars = STR(variant.value.as_string);
      return new_heap_string(runtime, &chars);
    }
    case vtBool:
      return runtime_bool(runtime, variant.value.as_bool);
    case vtNull:
      return ROOT(runtime, null);
    case vtValue:
      return variant.value.as_value;
    case vtArray: {
      size_t length = variant.value.as_array.length;
      TRY_DEF(result, new_heap_array(runtime, length));
      for (size_t i = 0; i < length; i++) {
        TRY_DEF(element, variant_to_value(runtime, variant.value.as_array.elements[i]));
        set_array_at(result, i, element);
      }
      return result;
    }
    default:
      UNREACHABLE("unknown variant type");
      return new_signal(scWat);
  }
}


bool advance_lexical_permutation(int64_t *elms, size_t elmc) {
  // This implementation follows an outline from wikipedia's article on
  // "Algorithms to generate permutations".
  bool found_k = false;
  // Find the largest k such that a[k] < a[k + 1].
  size_t k = 0;
  for (size_t k_plus_1 = elmc - 1; k_plus_1 > 0; k_plus_1--) {
    k = k_plus_1 - 1;
    if (elms[k] < elms[k + 1]) {
      found_k = true;
      break;
    }
  }
  if (!found_k)
    return false;
  // Find the largest l such that a[k] < a[l].
  size_t l;
  for (size_t l_plus_1 = elmc; l_plus_1 > 0; l_plus_1--) {
    l = l_plus_1 - 1;
    if (elms[k] < elms[l])
      break;
  }
  // Swap the value of a[k] with that of a[l].
  int64_t temp = elms[k];
  elms[k] = elms[l];
  elms[l] = temp;
  // Reverse the sequence from a[k + 1] up to and including the final element
  // a[n].
  for (size_t i = 0; (elmc - i - 1) > (k + 1 + i); i++) {
    int64_t temp = elms[elmc - i - 1];
    elms[elmc - i - 1] = elms[k + 1 + i];
    elms[k + 1 + i] = temp;
  }
  return true;
}
