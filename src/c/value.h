#include "check.h"
#include "globals.h"

#ifndef _VALUE
#define _VALUE

// Tags used to distinguish between values.
typedef enum {
  // Pointer to a heap object.
  vtObject = 0,
  // Tagged integer.
  vtInteger = 1,
  // A VM-internal signal.
  vtSignal = 2
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
  return (value << kTagSize) | vtInteger;
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_ptr_t value) {
  CHECK_EQ(vtInteger, get_value_tag(value));
  return (((int64_t) value) >> kTagSize);
}


// Enum identifying the type of a signal.
typedef enum {
  // The heap has run out of space.
  scHeapExhausted = 0,
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

#endif // _VALUE
