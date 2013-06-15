#include "value.h"

#ifndef _VALUE_INL
#define _VALUE_INL

// Evaluates the given expression; if it yields a signal returns it otherwise
// ignores the value.
#define TRY(EXPR) do {             \
  value_t __result__ = (EXPR); \
  if (HAS_TAG(Signal, __result__)) \
    return __result__;             \
} while (false)

// Evaluates the value and if it yields a signal bails out, otherwise assigns
// the result to the given target.
#define TRY_SET(TARGET, VALUE) do { \
  value_t __value__ = (VALUE);  \
  TRY(__value__);                   \
  TARGET = __value__;               \
} while (false)

// Declares a new variable to have the specified value. If the initializer
// yields a signal we bail out and return that value.
#define TRY_DEF(name, INIT)        \
value_t name;                  \
TRY_SET(name, INIT)

// Macro that evaluates to true if the given expression has the specified
// value tag.
#define HAS_TAG(Tag, EXPR) (get_value_tag(EXPR) == vt##Tag)

#endif // _VALUE_INL
