// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _STDC
#define _STDC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Convert the compiler-defined macros into simpler ones that express the
// differences we're interested in.
#ifdef _MSC_VER
#define IS_MSVC 1
#else
#define IS_GCC 1
#endif

// Include custom headers for each toolchain.
#ifdef IS_MSVC
#include "stdc-msvc.h"
#else // !IS_MSVC
#include "stdc-posix.h"
#endif

// Define some expression macros for small pieces of platform-dependent code.
#ifdef IS_MSVC
#define IF_MSVC(T, E) T
#define IF_GCC(T, E) E
#else
#define IF_MSVC(T, E) E
#define IF_GCC(T, E) T
#endif

#endif // _STDC
