// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _UTILS_INL
#define _UTILS_INL

#include "utils.h"

// --- S t r i n g ---

#define __STATIC_STRLEN__(S) (sizeof(S) / sizeof(char))

// Creates a new string hint from a literal C string.
#define STRING_HINT(str) ((string_hint_t) {{                                   \
  ((__STATIC_STRLEN__(str) == 0) ? '\0' : str[0]),                             \
  ((__STATIC_STRLEN__(str) <= 1) ? '\0' : str[1]),                             \
  ((__STATIC_STRLEN__(str) <= 2) ? '\0' : str[2]),                             \
  ((__STATIC_STRLEN__(str) <= 3) ? '\0' : str[3]),                             \
}})


// --- V a r i a d i c   m a c r o s ---

// Expands the given function for each element in the var args. The
// implementation of this is insane, plus there is a fixed limit on how many
// arguments are allowed, but it's really useful.
//
// The way it works is that it pairs each argument with an action that expands
// for that argument. The actions are picked out in reverse order so the action
// to terminate is last, the actions to continue before it.
#define FOR_EACH_VA_ARG(F, ...)                                                \
  __E8__(__FOR_EACH_VA_ARG_HELPER__(F, __VA_ARGS__,                            \
      __C__, __C__, __C__, __C__, __C__, __C__, __C__, __C__, __T__))

// Kicks things off by invoking the first action which, if necessary, will take
// care of invoking the remaining ones.
#define __FOR_EACH_VA_ARG_HELPER__(F, _0, _1, _2, _3, _4, _5, _6, _7, ACTION, ...)\
  DEFER(ACTION)()(F, _0, _1, _2, _3, _4, _5, _6, _7, __VA_ARGS__)

// The __EN__ macro expands N times. This is because the preprocessor doesn't
// allow macros to expand themselves recursively so instead we generate an
// intermediate result with unexpanded recursive calls and then use this
// set of macros to manually expand repeatedly.
#define __E1__(...) __VA_ARGS__
#define __E2__(...) __E1__(__E1__(__VA_ARGS__))
#define __E4__(...) __E2__(__E2__(__VA_ARGS__))
#define __E8__(...) __E2__(__E4__(__VA_ARGS__))

// To be completely honest I don't understand what's going on here.
#define EMPTY
#define DEFER(...) __VA_ARGS__ EMPTY

// Trampoline for continuing the expansion.
#define __C__() __DO_C__

// Trampoline for ending the expansion.
#define __T__() __DO_T__

// Apply the callback to the first argument, shift down the arguments and
// actions, and continue recursively.
#define __DO_C__(F, _0, _1, _2, _3, _4, _5, _6, _7, NEXT, ...)                 \
  F(_0)                                                                        \
  DEFER(NEXT)()(F, _1, _2, _3, _4, _5, _6, _7, NEXT, __VA_ARGS__)

// Stop expanding.
#define __DO_T__(...)


#endif // _UTILS_INL
