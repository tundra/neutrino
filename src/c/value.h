#include "check.h"
#include "globals.h"

#ifndef _VALUE
#define _VALUE

// Tags used to distinguish between values.
typedef enum {
  tObject = 0,
  tInteger = 1
} value_tag_t;

// A tagged value pointer.
typedef uint64_t value_ptr_t;

// How many bits are used for tags?
static const int kTagSize = 3;
static const int kTagMask = 0x7;

// Alignment that ensures that object pointers have tag 0.
static const int kValuePtrAlignment = 8;

// Returns the tag from a tagged value.
static value_tag_t get_value_tag(value_ptr_t value) {
  return (value_tag_t) (value & kTagMask);
}

// Creates a new tagged integer with the given value.
static value_ptr_t new_integer(int64_t value) {
  return (value << kTagSize) | tInteger;
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_ptr_t value) {
  CHECK_EQ(tInteger, get_value_tag(value));
  return (((int64_t) value) >> kTagSize);
}

#endif // _VALUE
