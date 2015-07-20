//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "c/stdc.h"
#include "test/asserts.hh"
#include "test/unittest.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "condition.h"
#include "globals.h"
#include "runtime.h"
#include "utils/log.h"
#include "utils/ook.h"
#include "value-inl.h"
END_C_INCLUDES

// Returns true iff the two values are structurally equal.
bool value_structural_equal(value_t a, value_t b);

// Allocates a new runtime object which will be stored in the variable "runtime".
// This pattern is used *everywhere* and packing it into a macro ensures that it
// is used consistently and allows other macros to depend on the variables
// created here.
#define CREATE_RUNTIME() CREATE_RUNTIME_WITH_CONFIG(NULL)

// Works the same as CREATE_RUNTIME but takes an explicit runtime config.
#define CREATE_RUNTIME_WITH_CONFIG(CONFIG)                                     \
runtime_t *runtime = NULL;                                                     \
value_t ambience = nothing();                                                  \
ASSERT_SUCCESS(new_runtime((CONFIG), &runtime));                               \
ASSERT_SUCCESS(ambience = new_heap_ambience(runtime));

// Disposes a runtime created using the CREATE_RUNTIME method.
#define DISPOSE_RUNTIME()                                                      \
ASSERT_SUCCESS(delete_runtime(runtime, dfDefault));

IMPLEMENTATION(check_recorder_o, abort_o);

// Data recorded about check failures.
struct check_recorder_o {
  IMPLEMENTATION_HEADER(check_recorder_o, abort_o);
  // How many check failures were triggered?
  size_t count;
  // What was the cause of the last check failure triggered?
  condition_cause_t last_cause;
  // The abort callback to restore when we're done recording checks.
  abort_o *previous;
};

// Installs a check recorder and switch to soft check failure mode. This also
// resets the recorder so it's not necessary to explicitly initialize it in
// advance. The initial cause is set to a value that is different from all
// condition causes but the concrete value should not otherwise be relied on.
void install_check_recorder(check_recorder_o *recorder);

// Uninstalls the given check recorder, which must be the currently active one,
// and restores checks to the same state as before it was installed. The state
// of the recorder is otherwise left undefined.
void uninstall_check_recorder(check_recorder_o *recorder);


IMPLEMENTATION(log_validator_o, log_o);

// Data associated with validating log messages. Unlike the check recorder we
// don't record log messages since there are some complicated issues around
// ownership and installing/uninstalling -- you want to uninstall the recorder
// before checking the log entries so that assertion failures are logged
// correctly but on the other hand you want the data you're going to check to
// stay alive so uninstalling can't dispose data. Anyway, this seems simpler,
// do the validation immediately.
struct log_validator_o {
  IMPLEMENTATION_HEADER(log_validator_o, log_o);
  // The number of entries that were logged.
  size_t count;
  // The abort callback to restore when we're done validating log messages.
  log_o *previous;
  // The pointers used to trampoline to the validate function.
  log_m validate_callback;
  void *validate_data;
};

// Installs a log validator. The struct stores data that can be used to
// uninstall it again, the callback will be invoked for each log entry issued.
void install_log_validator(log_validator_o *validator,
    log_m validate_callback, void *data);

// Uninstalls the given log validator, which must be the currently active one,
// and restores logging to the same state as before it was installed.
void uninstall_log_validator(log_validator_o *validator);

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

// Fails unless the given value is within the given domain.
#define ASSERT_DOMAIN(vdDomain, EXPR) \
ASSERT_CLASS(value_domain_t, vdDomain, EXPR, get_value_domain)

// Fails unless the given value is within the given genus.
#define ASSERT_GENUS(dgGenus, EXPR) \
ASSERT_CLASS(derived_object_genus_t, dgGenus, EXPR, get_derived_object_genus)

// Fails unless the given value is within the given family.
#define ASSERT_FAMILY(ofFamily, EXPR) \
ASSERT_CLASS(heap_object_family_t, ofFamily, EXPR, get_heap_object_family)

// Fails unless the given value is a condition of the given type.
#define ASSERT_CONDITION(scCause, EXPR) \
ASSERT_CLASS(condition_cause_t, scCause, EXPR, get_condition_cause)

#define ASSERT_CLASS(class_t, cExpected, EXPR, get_class) do {                 \
  class_t __class__ = get_class(EXPR);                                         \
  if (__class__ != cExpected) {                                                \
    const char *__class_name__ = get_class##_name(__class__);                  \
    fail(__FILE__, __LINE__, "Assertion failed: %s(%s) == %s.\n  Found: %s",   \
        #get_class, #EXPR, #cExpected, __class_name__);                        \
  }                                                                            \
} while (false)

// Fails if the given value is a condition.
#define ASSERT_SUCCESS(EXPR) do {                                              \
  value_t __value__ = (EXPR);                                                  \
  if (is_condition(__value__)) {                                               \
    fail(__FILE__, __LINE__, "Assertion failed: is_condition(%s).\n  Was condition: %s",\
        #EXPR, get_condition_cause_name(get_condition_cause(__value__)));      \
  }                                                                            \
} while (false)

#define __ASSERT_CHECK_FAILURE_NO_VALUE_HELPER__(scCause, E) do {              \
  check_recorder_o __recorder__;                                               \
  install_check_recorder(&__recorder__);                                       \
  do { E; } while (false);                                                     \
  ASSERT_EQ(1, __recorder__.count);                                            \
  ASSERT_EQ(scCause, __recorder__.last_cause);                                 \
  uninstall_check_recorder(&__recorder__);                                     \
} while (false)

// Fails unless the given expression returns the given failure _and_ CHECK fails
// with the same cause. Only executed when checks are enabled.
#define ASSERT_CHECK_FAILURE(scCause, E)                                       \
IF_CHECKS_ENABLED(                                                             \
    __ASSERT_CHECK_FAILURE_NO_VALUE_HELPER__(scCause, ASSERT_CONDITION(scCause, (E))))

// Fails unless the given expression CHECK fails. Unlike ASSERT_CHECK_FAILURE
// this makes no assumption about the returned value. Only executed when checks
// are enabled.
#define ASSERT_CHECK_FAILURE_NO_VALUE(scCause, E)                              \
IF_CHECKS_ENABLED(__ASSERT_CHECK_FAILURE_NO_VALUE_HELPER__(scCause, E))

FORWARD(variant_t);

// The payload of a variant value.
typedef union {
  int64_t as_int64;
  const char *as_c_str;
  bool as_bool;
  struct {
    size_t length;
    variant_t **elements;
  } as_array;
  value_t as_value;
} variant_value_t;

// Expanders for the basic types.
value_t expand_variant_to_integer(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_stage_offset(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_string(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_infix(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_index(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_bool(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_null(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_value(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_array(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_map(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_array_buffer(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_path(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_identifier(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_signature(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_parameter(runtime_t *runtime, variant_value_t *value);
value_t expand_variant_to_guard(runtime_t *runtime, variant_value_t *value);

// Type of expander functions that turn variants into values.
typedef value_t (variant_expander_t)(runtime_t *runtime, variant_value_t *variant);

// A generic variant type which allows heap data structures to be described
// conveniently inlined (using the macros below) as expressions and then
// passed around and/or converted together. New variant types can be defined
// by creating new expander functions.
struct variant_t {
  // The function to call to expand this variant to a heap value.
  variant_expander_t *expander;
  variant_value_t value;
};

// A stack-allocated arena that holds the memory used by test cases.
typedef struct {
  // The memory where the past blocks array is stored.
  blob_t past_blocks_memory;
  // Array of past blocks that have been exhausted and are waiting to be
  // disposed.
  blob_t *past_blocks;
  // The size of the past blocks array.
  size_t past_block_capacity;
  // The number of entries in the past blocks array that are in use.
  size_t past_block_count;
  // The block from which we're currently grabbing memory.
  blob_t current_block;
  // Pointer into the current block where the free memory starts.
  size_t current_block_cursor;
} test_arena_t;

// Initializes a test arena.
void test_arena_init(test_arena_t *arena);

// Disposes a variant container.
void test_arena_dispose(test_arena_t *arena);

// Allocates a new empty variant value in the given container.
void *test_arena_malloc(test_arena_t *arena, size_t size);

// Allocates a new array in the given arena and fills it with the pointer values
// given as arguments.
void *test_arena_copy_array(test_arena_t *arena, size_t size, size_t elmsize, ...);

// Allocates an instance of the given type in the given test arena.
#define ARENA_MALLOC(ARENA, TYPE)                                              \
  ((TYPE*) test_arena_malloc((ARENA), sizeof(TYPE)))

// Allocates an array of the given number of elements of the given in the given
// arena.
#define ARENA_MALLOC_ARRAY(ARENA, TYPE, SIZE)                                  \
  ((TYPE*) test_arena_malloc((ARENA), (SIZE) * sizeof(TYPE)))

// Creates an array within an arena with the given elements. The elements must
// be pointers.
#define ARENA_ARRAY(ARENA, TYPE, SIZE, ...)                                    \
  ((TYPE*) test_arena_copy_array((ARENA), (SIZE), sizeof(TYPE), __VA_ARGS__))

// Creates a new variant container that can be used implicitly by the variant
// macros.
#define CREATE_TEST_ARENA()                                                    \
  test_arena_t __test_arena__;                                                 \
  test_arena_init(&__test_arena__)

// Disposes a test arena created by CREATE_TEST_ARENA.
#define DISPOSE_TEST_ARENA()                                                   \
  test_arena_dispose(&__test_arena__)

// Returns true if the given variant value expands to null.
static bool variant_is_marker(variant_t *variant) {
  return variant->expander == NULL;
}

// Creates a new variant in the given arena with the given expander and
// payload.
static variant_t *new_variant(test_arena_t *arena, variant_expander_t expander,
    variant_value_t value) {
  variant_t *result = ARENA_MALLOC(arena, variant_t);
  result->expander = expander;
  result->value = value;
  return result;
}

// Creates a boolean variant value.
static variant_value_t var_bool(bool as_bool) {
  variant_value_t value;
  value.as_bool = as_bool;
  return value;
}

// Creates a variant value representing the empty array.
static variant_value_t var_empty_array() {
  variant_value_t contents;
  contents.as_array.length = 0;
  return contents;
}

// Creates an int64 variant value.
static variant_value_t var_int64(int64_t as_int64) {
  variant_value_t value;
  value.as_int64 = as_int64;
  return value;
}

// Creates a value_t variant value.
static variant_value_t var_value(value_t as_value) {
  variant_value_t value;
  value.as_value = as_value;
  return value;
}

// Creates a C-string variant value.
static variant_value_t var_c_str(const char *as_c_str) {
  variant_value_t value;
  value.as_c_str = as_c_str;
  return value;
}

// Creates an array-type variant value with the given elements.
variant_value_t var_array(test_arena_t *arena, size_t elmc, ...);

// Creates a new variant in the given arena. This macro and all the macros
// below must be somewhere between CREATE_TEST_ARENA and DISPOSE_TEST_ARENA
// declarations.
#define vVariant(expander, value) new_variant(&__test_arena__, expander, value)

// Returns a recognizable marker that can be detected using variant_is_marker.
#define vMarker vVariant(NULL, var_bool(0))

// Creates an integer variant with the given value.
#define vInt(V) vVariant(expand_variant_to_integer, var_int64(V))

// Creates a stage offset variant with the given offset.
#define vStageOffset(V) vVariant(expand_variant_to_stage_offset, var_int64(V))

// Creates a variant string with the given value.
#define vStr(V) vVariant(expand_variant_to_string, var_c_str(V))

// Creates a variant infix operation with the given string value.
#define vInfix(V) vVariant(expand_variant_to_infix, var_c_str(V))

// Creates a variant index operation.
#define vIndex() vVariant(expand_variant_to_index, var_bool(0))

// Creates a variant bool with the given value.
#define vBool(V) vVariant(expand_variant_to_bool, var_bool(V))

// Creates a variant null.
#define vNull() vVariant(expand_variant_to_null, var_bool(0))

// Creates a variant which represents the given value_t.
#define vValue(V) vVariant(expand_variant_to_value, var_value(V))

// Expands to the empty array. Because of the way the vArray macro works it has
// to be non-empty.
#define vEmptyArray() vVariant(expand_variant_to_array, var_empty_array())

// Expands to a payload value for the vVariant macro that stores all the argument
// in the as_array field in the value union.
#define vArrayPayload(...) var_array(&__test_arena__, VA_ARGC(__VA_ARGS__), __VA_ARGS__)

// Creates a variant array with the given length and elements.
#define vArray(...) vVariant(expand_variant_to_array, vArrayPayload(__VA_ARGS__))

// Creates a variant map with the given elements, passed as alternating keys and
// values.
#define vMap(...) vVariant(expand_variant_to_map, vArrayPayload(__VA_ARGS__))

// Creates a variant array with the given length and elements.
#define vArrayBuffer(...) vVariant(expand_variant_to_array_buffer, vArrayPayload(__VA_ARGS__))

// Expands to the empty array. Because of the way the vArray macro works it has
// to be non-empty.
#define vEmptyArrayBuffer() vVariant(expand_variant_to_array_buffer, var_empty_array())

// Expands to a variant representing a path with the given segments. This can
// not be used to construct the empty path.
#define vPath(...) vVariant(expand_variant_to_path, vArrayPayload(__VA_ARGS__))

// Expands to a variant representing an identifier with the given path and
// stage.
#define vIdentifier(S, P) vVariant(expand_variant_to_identifier, vArrayPayload(S, P))

// Expands to a variant representing a signature with the given allow_extra and
// parameters.
#define vSignature(AE, ...) vVariant(expand_variant_to_signature, vArrayPayload(vBool(AE), __VA_ARGS__))

// Expands to a variant representing a parameter with the given guard, is_optional,
// and tags.
#define vParameter(G, O, ...) vVariant(expand_variant_to_parameter, vArrayPayload(G, vBool(O), vArray(__VA_ARGS__)))

// Expands to a variant representing a guard with the given type and value.
#define vGuard(T, V) vVariant(expand_variant_to_guard, vArrayPayload(vInt(T), V))

// Instantiates a variant value in the runtime stored in the variable 'runtime'.
#define C(V) variant_to_value(runtime, (V))

// Given a variant, returns a value allocated in the given runtime (if necessary)
// with the corresponding value.
value_t variant_to_value(runtime_t *runtime, variant_t *variant);

// Given an array of length N that contains a permutation of the numbers from 0
// to N, advances the numbers to the next permutation such that calling this 2^N
// times, starting from 0..N, generates all possible permutations. Returns false
// iff the given array if in descending order, which is the end point.
bool advance_lexical_permutation(int64_t *elms, size_t elmc);
