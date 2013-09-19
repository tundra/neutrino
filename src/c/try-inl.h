// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Try-finally macros.

#ifndef _TRY_INL
#define _TRY_INL


// --- P l a i n ---

// Evaluates the given expression; if it yields a signal returns it otherwise
// ignores the value.
#define TRY(EXPR) do {                                                         \
  value_t __result__ = (EXPR);                                                 \
  if (in_domain(vdSignal, __result__))                                         \
    return __result__;                                                         \
} while (false)

// Evaluates the value and if it yields a signal bails out, otherwise assigns
// the result to the given target.
#define TRY_SET(TARGET, VALUE) do {                                            \
  value_t __value__ = (VALUE);                                                 \
  TRY(__value__);                                                              \
  TARGET = __value__;                                                          \
} while (false)

// Declares a new variable to have the specified value. If the initializer
// yields a signal we bail out and return that value.
#define TRY_DEF(name, INIT)                                                    \
value_t name;                                                                  \
TRY_SET(name, INIT)


// --- S a f e ---

// Same as TRY except works on safe values. Beware that unless you know that
// the safe is immediate or owned by someone else this may lead to safe object
// references being leaked.
#define S_TRY(EXPR) do {                                                       \
  safe_value_t __result__ = (EXPR);                                            \
  if (safe_value_in_domain(vdSignal, __result__))                              \
    return __result__;                                                         \
} while (false)

// Same as TRY_SET excepts works on safe values.
#define S_TRY_SET(TARGET, VALUE) do {                                          \
  safe_value_t __value__ = (VALUE);                                            \
  S_TRY(__value__);                                                            \
  TARGET = __value__;                                                          \
} while (false)

// Same as TRY_DEF except works on safe values.
#define S_TRY_DEF(name, INIT)                                                  \
safe_value_t name;                                                             \
S_TRY_SET(name, INIT)

// --- E n s u r e ---

// Crude exception handling. The E_ macros work like the plain ones but can
// ensure cleanup on signals.

// Declares that the following code uses the E_ macros to perform cleanup on
// leaving the method. This is super crude and need refinement but it works okay.
#define E_BEGIN_TRY_FINALLY() {                                                \
  value_t __ensure_result__;

// Does the same as a try except makes sure that the function's FINALLY block
// is executed before bailing out on a signal.
#define E_TRY(EXPR) do {                                                       \
  value_t __result__ = (EXPR);                                                 \
  if (in_domain(vdSignal, __result__))                                         \
    E_RETURN(__result__);                                                      \
} while (false)

// Does the same as TRY_SET except makes sure the function's FINALLY block is
// executed before bailing on a signal.
#define E_TRY_SET(TARGET, VALUE) do {                                          \
  value_t __value__ = (VALUE);                                                 \
  E_TRY(__value__);                                                            \
  TARGET = __value__;                                                          \
} while (false)

// Does the same as TRY_DEF except makes sure the function's FINALLY block is
// executed before bailing on a signal.
#define E_TRY_DEF(name, INIT)                                                  \
value_t name;                                                                  \
E_TRY_SET(name, INIT)

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
