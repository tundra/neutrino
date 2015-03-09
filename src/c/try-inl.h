//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Try-finally macros.


#ifndef _TRY_INL
#define _TRY_INL

#include "value-inl.h"

#define P_FLAVOR(F) F(value_t, is_condition, is_heap_exhausted_condition)
#define S_FLAVOR(F) F(safe_value_t, safe_value_is_condition, 0)

#define __TRY_GET_TYPE__(TYPE, IS_COND, IS_EXHAUSTED) TYPE
#define __TRY_GET_IS_COND__(TYPE, IS_COND, IS_EXHAUSTED) IS_COND
#define __TRY_GET_IS_EXHAUSTED__(TYPE, IS_COND, IS_EXHAUSTED) IS_EXHAUSTED

#define __GENERIC_TRY__(FLAVOR, EXPR, RETURN) do {                             \
  FLAVOR(__TRY_GET_TYPE__) __result__ = (EXPR);                                \
  if (FLAVOR(__TRY_GET_IS_COND__)(__result__))                                 \
    RETURN(__result__);                                                        \
} while (false)

#define __GENERIC_TRY_SET__(FLAVOR, TARGET, EXPR, RETURN) do {                 \
  FLAVOR(__TRY_GET_TYPE__) __value__ = (EXPR);                                 \
  __GENERIC_TRY__(FLAVOR, __value__, RETURN);                                  \
  TARGET = __value__;                                                          \
} while (false)

#define __GENERIC_TRY_DEF__(FLAVOR, TARGET, EXPR, RETURN)                      \
FLAVOR(__TRY_GET_TYPE__) TARGET;                                               \
__GENERIC_TRY_SET__(FLAVOR, TARGET, EXPR, RETURN)

#define __GENERIC_RETRY_DEF__(FLAVOR, RUNTIME, TARGET, EXPR, RETURN)           \
  FLAVOR(__TRY_GET_TYPE__) TARGET;                                             \
  do {                                                                         \
    FLAVOR(__TRY_GET_TYPE__) __value__ = (EXPR);                               \
    if (FLAVOR(__TRY_GET_IS_EXHAUSTED__)(__value__)) {                         \
      runtime_garbage_collect(RUNTIME);                                        \
      runtime_toggle_fuzzing(RUNTIME, false);                                  \
      __value__ = (EXPR);                                                      \
      runtime_toggle_fuzzing(RUNTIME, true);                                   \
      if (FLAVOR(__TRY_GET_IS_EXHAUSTED__)(__value__))                         \
        RETURN(new_out_of_memory_condition());                                 \
    }                                                                          \
    __GENERIC_TRY__(FLAVOR, __value__, RETURN);                                \
    TARGET = __value__;                                                        \
  } while (false)


// --- P l a i n ---

// Evaluates the given expression; if it yields a condition returns it otherwise
// ignores the value.
#define P_RETURN(V) return (V)
#define TRY(EXPR) __GENERIC_TRY__(P_FLAVOR, EXPR, P_RETURN)

// Evaluates the value and if it yields a condition bails out, otherwise assigns
// the result to the given target.
#define TRY_SET(TARGET, EXPR) __GENERIC_TRY_SET__(P_FLAVOR, TARGET, EXPR, P_RETURN)

// Declares a new variable to have the specified value. If the initializer
// yields a condition we bail out and return that value.
#define TRY_DEF(NAME, EXPR) __GENERIC_TRY_DEF__(P_FLAVOR, NAME, EXPR, P_RETURN)


// --- E n s u r e ---

// Crude exception handling. The E_ macros work like the plain ones but can
// ensure cleanup on conditions.

// Declares that the following code uses the E_ macros to perform cleanup on
// leaving the method. This is super crude and need refinement but it works okay.
#define E_BEGIN_TRY_FINALLY() {                                                \
  value_t __ensure_result__;

// Does the same as a TRY except makes sure that the function's FINALLY block
// is executed before bailing out on a condition.
#define E_TRY(EXPR) __GENERIC_TRY__(P_FLAVOR, EXPR, E_RETURN)

// Does the same as TRY_SET except makes sure the function's FINALLY block is
// executed before bailing on a condition.
#define E_TRY_SET(TARGET, EXPR) __GENERIC_TRY_SET__(P_FLAVOR, TARGET, EXPR, E_RETURN)

// Does the same as TRY_DEF except makes sure the function's FINALLY block is
// executed before bailing on a condition.
#define E_TRY_DEF(NAME, EXPR) __GENERIC_TRY_DEF__(P_FLAVOR, NAME, EXPR, E_RETURN)

#define E_S_RETURN(V) E_RETURN(deref_immediate(V))

// Does the same as a S_TRY except makes sure that the function's FINALLY block
// is executed before bailing out on a condition.
#define E_S_TRY(EXPR) __GENERIC_TRY__(S_FLAVOR, EXPR, E_S_RETURN)

// Does the same as S_TRY_SET except makes sure the function's FINALLY block is
// executed before bailing on a condition.
#define E_S_TRY_SET(TARGET, EXPR) __GENERIC_TRY_SET__(S_FLAVOR, TARGET, EXPR, E_S_RETURN)

// Does the same as S_TRY_DEF except makes sure the function's FINALLY block is
// executed before bailing on a condition.
#define E_S_TRY_DEF(NAME, EXPR) __GENERIC_TRY_DEF__(S_FLAVOR, NAME, EXPR, E_S_RETURN)

// Does the same as a normal return of the specified value but ensures that the
// containing function's FINALLY clause is executed.
#define E_RETURN(V) do {                                                       \
  __ensure_result__ = V;                                                       \
  goto finally;                                                                \
} while (false)

// Marks the end of a try/finally block. This must come at the and of a function,
// any code following this macro will not be executed.
#define E_END_TRY_FINALLY()                                                    \
    }                                                                          \
    return __ensure_result__;                                                  \
  }                                                                            \
  SWALLOW_SEMI(etf)

// Marks the end of the try-part of a try/finally block, the following code is
// the finally part. The implementation is a bit fiddly in order for a semi to
// be allowed after this macro, that's what the extra { is for.
#define E_FINALLY()                                                            \
  finally: {                                                                   \
    SWALLOW_SEMI(ef)

#endif // _TRY_INL
