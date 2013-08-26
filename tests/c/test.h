// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "signals.h"
#include "value.h"

// Declare a unit test method. The suite name must match the file the test
// case is declared in.
#define TEST(suite, name) void test_##suite##_##name()

// Aborts exception, signalling an error.
void fail(const char *file, int line, const char *fmt, ...);

// Returns true iff the two values are structurally equal.
bool value_structural_equal(value_t a, value_t b);

// Allocates a new runtime object which will be stored in the variable "runtime".
// This pattern is used *everywhere* and packing it into a macro ensures that it
// is used consistently and allows other macros to depend on the variables
// created here.
#define CREATE_RUNTIME()                                                       \
runtime_t stack_runtime;                                                       \
runtime_t *runtime = &stack_runtime;                                           \
ASSERT_SUCCESS(runtime_init(runtime, NULL))

// Disposes a runtime created using the CREATE_RUNTIME method.
#define DISPOSE_RUNTIME()                                                      \
ASSERT_SUCCESS(runtime_dispose(runtime));


// Data recorded about check failures.
typedef struct {
  // How many check failures were triggered?
  size_t count;
  // What was the cause of the last check failure triggered?
  signal_cause_t cause;
  // The abort callback to restore when we're done recording checks.
  abort_callback_t *previous;
  // This recorder's callback.
  abort_callback_t callback;
} check_recorder_t;

// Installs a check recorder and switch to soft check failure mode. This also
// resets the recorder so it's not necessary to explicitly initialize it in
// advance. The initial cause is set to a value that is different from all
// signal causes but the concrete value should not otherwise be relied on.
void install_check_recorder(check_recorder_t *recorder);

// Uninstalls the given check recorder, which must be the currently active one,
// and restores checks to the same state as before it was installed. The state
// of the recorder is otherwise left undefined.
void uninstall_check_recorder(check_recorder_t *recorder);

// Fails unless the two values are equal.
#define ASSERT_EQ(A, B) do {                                                   \
  int64_t __a__ = (int64_t) (A);                                               \
  int64_t __b__ = (int64_t) (B);                                               \
  if (__a__ != __b__)                                                          \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.\n  Expected: %lli\n  Found: %lli", \
        #A, #B, __a__, __b__);                                                 \
} while (false)

// Bit-casts a void* to an integer.
static int64_t ptr_to_int_bit_cast(void *value) {
  int64_t result = 0;
  memcpy(&result, &value, sizeof(value));
  return result;
}

// Failus unless the two pointer values are equal.
#define ASSERT_PTREQ(A, B) ASSERT_EQ(ptr_to_int_bit_cast(A), ptr_to_int_bit_cast(B))

// Fails unless the two values are different.
#define ASSERT_NEQ(A, B) ASSERT_FALSE((A) == (B))

// Fails unless the two given strings are equal.
#define ASSERT_STREQ(A, B) do {                                                \
  string_t *__a__ = (A);                                                       \
  string_t *__b__ = (B);                                                       \
  if (!(string_equals(__a__, __b__)))                                          \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.\n  Expected: %s\n  Found: %s", \
        #A, #B, __a__->chars, __b__->chars);                                   \
} while (false)

// Check that the two values A and B are structurally equivalent. Note that this
// only handles object trees, not cyclical graphs of objects.
#define ASSERT_VALEQ(A, B) do {                                                \
  value_t __a__ = (A);                                                         \
  value_t __b__ = (B);                                                         \
  if (!(value_structural_equal(__a__, __b__))) {                               \
    fail(__FILE__, __LINE__, "Assertion failed: %s == %s.\n", #A, #B);         \
  }                                                                            \
} while (false)

// Identical to ASSERT_VALEQ except that the first argument is a variant which
// is converted to a value using the stack-allocated runtime 'runtime'.
#define ASSERT_VAREQ(A, B) ASSERT_VALEQ(variant_to_value(runtime, A), B)

// Checks that A and B are the same object or value.
#define ASSERT_SAME(A, B) ASSERT_EQ((A).encoded, (B).encoded)

// Fails unless A and B are different objects, even if they're equal.
#define ASSERT_NSAME(A, B) ASSERT_NEQ((A).encoded, (B).encoded)

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
  if (__class__ != cExpected) {                                                \
    const char *__class_name__ = get_class##_name(__class__);                  \
    fail(__FILE__, __LINE__, "Assertion failed: %s(%s) == %s.\n  Found: %s",   \
        #get_class, #EXPR, #cExpected, __class_name__);                        \
  }                                                                            \
} while (false)

// Fails if the given value is a signal.
#define ASSERT_SUCCESS(EXPR) do {                                              \
  value_t __value__ = (EXPR);                                                  \
  if (get_value_domain(__value__) == vdSignal) {                               \
    fail(__FILE__, __LINE__, "Assertion failed: is_signal(%s).\n  Was signal: %s",\
        #EXPR, get_signal_cause_name(get_signal_cause(__value__)));            \
  }                                                                            \
} while (false)

#define __ASSERT_CHECK_FAILURE_HELPER__(scCause, E) do {                       \
  check_recorder_t __recorder__;                                               \
  install_check_recorder(&__recorder__);                                       \
  ASSERT_SIGNAL(scCause, E);                                                   \
  ASSERT_EQ(1, __recorder__.count);                                            \
  ASSERT_EQ(scCause, __recorder__.cause);                                      \
  uninstall_check_recorder(&__recorder__);                                     \
} while (false)

// Fails unless the given expression returns the given failure _and_ CHECK fails
// with the same cause. Only executed when checks are enabled.
#define ASSERT_CHECK_FAILURE(scCause, E)                                       \
IF_CHECKS_ENABLED(__ASSERT_CHECK_FAILURE_HELPER__(scCause, E))

// Expands to a string_t with the given contents.
#define STR(value) ((string_t) {strlen(value), value})

// The type tag of a variant value.
typedef enum {
  vtInteger,
  vtString,
  vtBool,
  vtNull,
  vtArray
} variant_type_t;

// A variant which can hold various C data types. Used for various convenience
// functions for working with neutrino data.
typedef struct variant_t {
  variant_type_t type;
  union {
    int64_t as_integer;
    const char *as_string;
    bool as_bool;
    struct {
      size_t length;
      struct variant_t *elements;
    } as_array;
  } value;
} variant_t;

// Creates an integer variant with the given value.
#define vInt(V) ((variant_t) {vtInteger, {.as_integer=(V)}})

// Creates a variant string with the given value.
#define vStr(V) ((variant_t) {vtString, {.as_string=(V)}})

// Creates a variant bool with the given value.
#define vBool(V) ((variant_t) {vtBool, {.as_bool=(V)}})

// Creates a variant null with the given value.
#define vNull() ((variant_t) {vtNull, {.as_integer=0}})

#define vEmptyArray() ((variant_t) {vtArray, {.as_array={0, 0}}})

// Creates a variant array with the given length and elements.
#define vArray(N, ELMS) ((variant_t) {vtArray, {.as_array={N, (variant_t[N]) {ELMS}}}})

// Alias for commas to use between elements as arguments to vArray. Commas mess
// with macros, this fixes that.
#define o ,

// Given a variant, returns a value allocated in the given runtime (if necessary)
// with the corresponding value.
value_t variant_to_value(runtime_t *runtime, variant_t variant);

// Data for multiply-with-carry pseudo-random generator.
// See See http://www.ms.uky.edu/~mai/RandomNumber.
typedef struct {
  uint32_t low;
  uint32_t high;
} pseudo_random_t;

// Initializes a pseudo-random generator with the given seed.
void pseudo_random_init(pseudo_random_t *random, uint32_t seed);

// Returns the next pseudo-random uint32.
uint32_t pseudo_random_next_uint32(pseudo_random_t *random);

// Returns the next pseudo-random number greater than 0 and less than the given
// max.
uint32_t pseudo_random_next(pseudo_random_t *random, uint32_t max);

// Shuffles the given array of 'elem_count' elements, each of which is 'elem_size'
// wide.
void pseudo_random_shuffle(pseudo_random_t *random, void *data,
    size_t elem_count, size_t elem_size);
