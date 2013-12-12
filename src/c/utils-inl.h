// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _UTILS_INL
#define _UTILS_INL

#include "utils.h"

// --- S t r i n g ---

// Returns the number of characters of the string. The -1 is to get rid of the
// null terminator.
#define __STATIC_STRLEN__(S) ((sizeof(S) / sizeof(char)) - 1)

// Creates a new string hint from a literal C string by chopping off the
// beginning and the end of the string. For details of how the encoding works
// see string_hint_to_c_str.
// TODO: remove this when all uses have been replaced by STRING_HINT_INIT.
#define STRING_HINT(str) ((string_hint_t) STRING_HINT_INIT(str))

// Expands to an initializer that can be used to initialize a string_hint_t
// variable.
#define STRING_HINT_INIT(str) {{                                               \
  ((__STATIC_STRLEN__(str) == 0) ? '\0' : str[0]),                             \
  ((__STATIC_STRLEN__(str) <= 1) ? '\0' : str[1]),                             \
  ((__STATIC_STRLEN__(str) <= 3) ? '\0' : str[__STATIC_STRLEN__(str) - 2]),    \
  ((__STATIC_STRLEN__(str) <= 2) ? '\0' : str[__STATIC_STRLEN__(str) - 1]),    \
}}


// --- V a r i a d i c   m a c r o s ---

// Utility that picks out the correct count from the arguments passed by VA_ARGC.
#define __VA_ARGC_PICK_COUNT__(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, COUNT, ...) COUNT

// Expands to the number of arguments given as var args. Note that this macro
// does _not_ work with 0 arguments.
#define VA_ARGC(...) __VA_ARGC_PICK_COUNT__(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

// Expands the given function for each element in the var args. The
// implementation of this is insane, plus there is a fixed limit on how many
// arguments are allowed, but it's really useful.
//
// The way it works is that it pairs each argument with an action that expands
// for that argument. The actions are picked out in reverse order so the action
// to terminate is last, the actions to continue before it.
#define FOR_EACH_VA_ARG(F, ...)                                                \
  __E16__(__FOR_EACH_VA_ARG_HELPER__(F, __VA_ARGS__,                           \
      __C__, __C__, __C__, __C__, __C__, __C__, __C__, __C__, __C__, __C__, __T__))

// Kicks things off by invoking the first action which, if necessary, will take
// care of invoking the remaining ones.
#define __FOR_EACH_VA_ARG_HELPER__(F, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, ACTION, ...)\
  DEFER(ACTION)()(F, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, __VA_ARGS__)

// The __EN__ macro expands N times. This is because the preprocessor doesn't
// allow macros to expand themselves recursively so instead we generate an
// intermediate result with unexpanded recursive calls and then use this
// set of macros to manually expand repeatedly.
#define __E1__(...) __VA_ARGS__
#define __E2__(...) __E1__(__E1__(__VA_ARGS__))
#define __E4__(...) __E2__(__E2__(__VA_ARGS__))
#define __E16__(...) __E4__(__E4__(__VA_ARGS__))

// To be completely honest I don't understand what's going on here.
#define EMPTY
#define DEFER(...) __VA_ARGS__ EMPTY

// Trampoline for continuing the expansion.
#define __C__() __DO_C__

// Trampoline for ending the expansion.
#define __T__() __DO_T__

// Apply the callback to the first argument, shift down the arguments and
// actions, and continue recursively.
#define __DO_C__(F, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, NEXT, ...)         \
  F(_0)                                                                        \
  DEFER(NEXT)()(F, _1, _2, _3, _4, _5, _6, _7, _8, _9, NEXT, __VA_ARGS__)

// Stop expanding.
#define __DO_T__(...)


#endif // _UTILS_INL
