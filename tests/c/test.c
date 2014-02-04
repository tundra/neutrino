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
#include "tagged.h"
#include "try-inl.h"
#include "value-inl.h"

#include <stdio.h>
#include <stdlib.h>

// TODO: fix timing for msvc
#ifdef IS_GCC
// ARRRG! Features. Are. Awful!
#define __USE_POSIX199309
#include <time.h>
extern char *strdup(const char*);
#endif


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
  // TODO: fix timing for msvc.
#ifdef IS_GCC
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  return spec.tv_sec + (spec.tv_nsec / 1000000000.0);
#else
  return 0;
#endif
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

static bool array_buffer_structural_equal(value_t a, value_t b) {
  CHECK_FAMILY(ofArrayBuffer, a);
  CHECK_FAMILY(ofArrayBuffer, b);
  size_t length = get_array_buffer_length(a);
  if (get_array_buffer_length(b) != length)
    return false;
  for (size_t i = 0; i < length; i++) {
    if (!(value_structural_equal(get_array_buffer_at(a, i),
        get_array_buffer_at(b, i))))
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
    if (is_condition(ccNotFound, b_value))
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
    case ofArrayBuffer:
      return array_buffer_structural_equal(a, b);
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

static void recorder_abort_callback(void *data, abort_message_t *message) {
  check_recorder_t *recorder = (check_recorder_t*) data;
  recorder->count++;
  recorder->last_cause = (consition_cause_t) message->condition_cause;
}

void install_check_recorder(check_recorder_t *recorder) {
  recorder->count = 0;
  recorder->last_cause = __ccFirst__;
  init_abort_callback(&recorder->callback, recorder_abort_callback, recorder);
  recorder->previous = set_abort_callback(&recorder->callback);
  CHECK_TRUE("no previous abort callback", recorder->previous != NULL);
}

void uninstall_check_recorder(check_recorder_t *recorder) {
  CHECK_TRUE("uninstalling again", recorder->previous != NULL);
  set_abort_callback(recorder->previous);
  recorder->previous = NULL;
}

void validator_log_callback(void *data, log_entry_t *entry) {
  log_validator_t *validator = (log_validator_t*) data;
  validator->count++;
  // Temporarily restore the previous log callback in case validation wants to
  // log (which it typically will on validation failure).
  set_log_callback(validator->previous);
  (validator->validate_callback)(validator->validate_data, entry);
  set_log_callback(&validator->callback);
}

void install_log_validator(log_validator_t *validator, log_function_t *callback,
    void *data) {
  validator->count = 0;
  validator->validate_callback = callback;
  validator->validate_data = data;
  init_log_callback(&validator->callback, validator_log_callback, validator);
  validator->previous = set_log_callback(&validator->callback);
  CHECK_TRUE("no previous log callback", validator->previous != NULL);
}

void uninstall_log_validator(log_validator_t *validator) {
  CHECK_TRUE("uninstalling again", validator->previous != NULL);
  set_log_callback(validator->previous);
  validator->previous = NULL;
}


// --- V a r i a n t s ---

// The size of an individual variant container block.
static const size_t kVariantContainerBlockSize = 1024;

// Moves the current block, which is now presumably exhausted, into the list
// of past blocks.
static void test_arena_retire_current_block(test_arena_t *arena) {
  if (arena->past_block_count == arena->past_block_capacity) {
    // If we've exhausted the past blocks array we double the capacity.
    size_t new_capacity = 2 * arena->past_block_capacity;
    if (new_capacity < 4)
      // The first time the old capacity will be 0 so we go directly to 4.
      new_capacity = 4;
    // Allocate a new past blocks array.
    memory_block_t new_memory = allocator_default_malloc(new_capacity * sizeof(memory_block_t));
    memory_block_t *new_blocks = (memory_block_t*) new_memory.memory;
    // Copy the previous values.
    for (size_t i = 0; i < arena->past_block_capacity; i++)
      new_blocks[i] = arena->past_blocks[i];
    // Free the old array before we clobber the field.
    allocator_default_free(arena->past_blocks_memory);
    // Update the state of the container.
    arena->past_blocks_memory = new_memory;
    arena->past_blocks = new_blocks;
    arena->past_block_capacity = new_capacity;
  }
  arena->past_blocks[arena->past_block_count] = arena->current_block;
  arena->past_block_count++;
  arena->current_block = allocator_default_malloc(kVariantContainerBlockSize);
  arena->current_block_cursor = 0;
}

void *test_arena_malloc(test_arena_t *arena, size_t size) {
  CHECK_REL("variant block too big", size, <, kVariantContainerBlockSize);
  if ((arena->current_block_cursor + size) > arena->current_block.size)
    test_arena_retire_current_block(arena);
  byte_t *start = (byte_t*) arena->current_block.memory;
  byte_t *result = start + arena->current_block_cursor;
  arena->current_block_cursor += size;
  return result;
}

// Copies va_args into an array, where each element has the given type.
// TODO: verify that this is really necessary and otherwise get rid of it.
#define COPY_ARRAY_ELEMENTS(TYPE) do {                                         \
  TYPE *elms = (TYPE*) mem;                                                    \
  for (size_t i = 0; i < size; i++)                                            \
    elms[i] = va_arg(ap, TYPE);                                                \
} while (false)

void *test_arena_copy_array(test_arena_t *arena, size_t size, size_t elmsize, ...) {
  void *mem = ARENA_MALLOC_ARRAY(arena, variant_t*, size);
  va_list ap;
  va_start(ap, elmsize);
  switch (elmsize) {
  case 4:
    COPY_ARRAY_ELEMENTS(int32_t);
    break;
  case 8:
    COPY_ARRAY_ELEMENTS(int64_t);
    break;
  default:
    UNREACHABLE("unexpected element size");
    break;
  }
  va_end(ap);
  return mem;
}

void test_arena_init(test_arena_t *arena) {
  arena->past_blocks_memory = memory_block_empty();
  arena->current_block = allocator_default_malloc(kVariantContainerBlockSize);
  arena->current_block_cursor = 0;
  arena->past_block_capacity = 0;
  arena->past_block_count = 0;
  arena->past_blocks = NULL;
}

void test_arena_dispose(test_arena_t *arena) {
  for (size_t i = 0; i < arena->past_block_count; i++) {
    memory_block_t past_block = arena->past_blocks[i];
    allocator_default_free(past_block);
  }
  allocator_default_free(arena->past_blocks_memory);
  allocator_default_free(arena->current_block);
}

variant_value_t var_array(test_arena_t *arena, size_t elmc, ...) {
  variant_value_t value;
  value.as_array.length = elmc;
  void *mem = ARENA_MALLOC_ARRAY(arena, variant_t*, elmc);
  value.as_array.elements = (variant_t**) mem;
  va_list ap;
  va_start(ap, elmc);
  for (size_t i = 0; i < elmc; i++)
    value.as_array.elements[i] = va_arg(ap, variant_t*);
  va_end(ap);
  return value;
}

value_t expand_variant_to_integer(runtime_t *runtime, variant_value_t *value) {
  return new_integer(value->as_int64);
}

value_t expand_variant_to_stage_offset(runtime_t *runtime, variant_value_t *value) {
  return new_stage_offset(value->as_int64);
}

value_t expand_variant_to_string(runtime_t *runtime, variant_value_t *value) {
  string_t chars = new_string(value->as_c_str);
  return new_heap_string(runtime, &chars);
}

value_t expand_variant_to_infix(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(str, expand_variant_to_string(runtime, value));
  return new_heap_operation(runtime, afFreeze, otInfix, str);
}

value_t expand_variant_to_index(runtime_t *runtime, variant_value_t *value) {
  return new_heap_operation(runtime, afFreeze, otIndex, nothing());
}

value_t expand_variant_to_bool(runtime_t *runtime, variant_value_t *value) {
  return new_boolean(value->as_bool);
}

value_t expand_variant_to_null(runtime_t *runtime, variant_value_t *value) {
  return null();
}

value_t expand_variant_to_value(runtime_t *runtime, variant_value_t *value) {
  return value->as_value;
}

value_t expand_variant_to_array(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  TRY_DEF(result, new_heap_array(runtime, length));
  for (size_t i = 0; i < length; i++) {
    TRY_DEF(element, C(value->as_array.elements[i]));
    set_array_at(result, i, element);
  }
  return result;
}

value_t expand_variant_to_map(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  TRY_DEF(result, new_heap_id_hash_map(runtime, length));
  for (size_t i = 0; i < length; i += 2) {
    TRY_DEF(key, C(value->as_array.elements[i]));
    TRY_DEF(val, C(value->as_array.elements[i + 1]));
    TRY(set_id_hash_map_at(runtime, result, key, val));
  }
  return result;
}

value_t expand_variant_to_array_buffer(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(array, expand_variant_to_array(runtime, value));
  return new_heap_array_buffer_with_contents(runtime, array);
}

value_t expand_variant_to_path(runtime_t *runtime, variant_value_t *value) {
  size_t length = value->as_array.length;
  value_t result = ROOT(runtime, empty_path);
  for (size_t i = 0; i < length; i++) {
    // The path has to be constructed backwards to the first element becomes
    // the head of the result, rather than the head of the end.
    TRY_DEF(head, C(value->as_array.elements[length - i - 1]));
    TRY_SET(result, new_heap_path(runtime, afMutable, head, result));
  }
  return result;
}

value_t expand_variant_to_identifier(runtime_t *runtime, variant_value_t *value) {
  CHECK_EQ("invalid identifier variant input", 2, value->as_array.length);
  TRY_DEF(stage, C(value->as_array.elements[0]));
  TRY_DEF(path, C(value->as_array.elements[1]));
  return new_heap_identifier(runtime, stage, path);
}

value_t variant_to_value(runtime_t *runtime, variant_t *variant) {
  return (variant->expander)(runtime, &variant->value);
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
  size_t l = 0;
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
