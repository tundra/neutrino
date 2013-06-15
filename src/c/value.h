#include "check.h"
#include "globals.h"
#include "utils.h"

#ifndef _VALUE
#define _VALUE

// Tags used to distinguish between values.
typedef enum {
  // Pointer to a heap object.
  vtObject = 0,
  // Tagged integer.
  vtInteger,
  // A VM-internal signal.
  vtSignal
} value_tag_t;

// A tagged value pointer.
typedef uint64_t value_ptr_t;

// How many bits are used for tags?
static const int kTagSize = 3;
static const int kTagMask = 0x7;

// Alignment that ensures that object pointers have tag 0.
#define kValueSize 8

// Returns the tag from a tagged value.
static value_tag_t get_value_tag(value_ptr_t value) {
  return (value_tag_t) (value & kTagMask);
}

// Macro that evaluates to true if the given expression has the specified
// value tag.
#define HAS_TAG(Tag, EXPR) (get_value_tag(EXPR) == vt##Tag)


// Creates a new tagged integer with the given value.
static value_ptr_t new_integer(int64_t value) {
  return (value << kTagSize) | vtInteger;
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_ptr_t value) {
  CHECK_EQ(vtInteger, get_value_tag(value));
  return (((int64_t) value) >> kTagSize);
}

// Returns a value that is _not_ a signal. This can be used to indicate
// unspecific success.
static value_ptr_t non_signal() {
  return new_integer(0);
}


// Enum identifying the type of a signal.
typedef enum {
  // The heap has run out of space.
  scHeapExhausted = 0,
  scSystemError
} signal_cause_t;

// How many bits do we need to hold the cause of a signal?
static const int kSignalCauseSize = 5;
static const int kSignalCauseMask = 0x1f;

// Creates a new signal with the specified cause.
static value_ptr_t new_signal(signal_cause_t cause) {
  return (value_ptr_t) (cause << kTagSize) | vtSignal;
}

// Returns the cause of a signal.
static signal_cause_t get_signal_cause(value_ptr_t value) {
  CHECK_EQ(vtSignal, get_value_tag(value));
  return  (((int64_t) value) >> kTagSize) & kSignalCauseMask;
}

// Evaluates the given expression; if it yields a signal returns it otherwise
// ignores the value.
#define TRY(EXPR) do {             \
  value_ptr_t __result__ = (EXPR); \
  if (HAS_TAG(Signal, __result__)) \
    return __result__;             \
} while (false)


// Enum identifying the different types of heap objects.
typedef enum {
  // A species object.
  otSpecies = 0,
  // A dumb string.
  otString,
} object_type_t;

// Converts a pointer to an object into an tagged object value pointer.
static value_ptr_t new_object(address_t addr) {
  CHECK_EQ(0, ((address_arith_t) addr) & kTagMask);
  return (value_ptr_t) ((address_arith_t) addr);
}

// Returns the address of the object pointed to by a tagged object value
// pointer.
static address_t get_object_address(value_ptr_t value) {
  CHECK_EQ(vtObject, get_value_tag(value));
  return (address_t) ((address_arith_t) value);
}

// Returns a pointer to the index'th field in the given heap object. Check
// fails if the value is not a heap object.
static value_ptr_t *access_object_field(value_ptr_t value, size_t index) {
  CHECK_EQ(vtObject, get_value_tag(value));
  address_t addr = get_object_address(value);
  return ((value_ptr_t*) addr) + index;
}

// Returns the object type of the object the given value points to.
static object_type_t get_object_type(value_ptr_t value) {
  return (object_type_t) *access_object_field(value, 0);
}

// Sets the type of the given object.
static void set_object_type(value_ptr_t value, object_type_t type) {
  *access_object_field(value, 0) = (value_ptr_t) type;
}

// Number of bytes in an object header.
#define kObjectHeaderSize kValueSize

// Returns the size of a heap string with the given number of characters.
size_t calc_string_size(size_t char_count);

// Sets the length of a heap string.
void set_string_length(value_ptr_t value, size_t length);

// Returns the length of a heap string.
size_t get_string_length(value_ptr_t value);

// Returns a pointer to the array that holds the contents of this array.
char *get_string_chars(value_ptr_t value);

// Stores the contents of this string in the given output.
void get_string_contents(value_ptr_t value, string_t *out);

#endif // _VALUE
