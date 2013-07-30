// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Standard includes and definitions available everywhere.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _GLOBALS
#define _GLOBALS

// A single byte.
typedef uint8_t byte_t;

// Shorthand for pointers into memory.
typedef byte_t *address_t;

// The type to cast pointers to when you need to do advanced pointer
// arithmetic.
#if M64
typedef uint64_t address_arith_t;
#else
typedef uint32_t address_arith_t;
#endif

// Shorthands for commonly used sizes.
#define kKB 1024
#define kMB (kKB * kKB)

// Expands to a declaration that is missing a semicolon at the end. If used
// at the end of a macro that doesn't allow a final semi this allows the semi
// to be written.
#define SWALLOW_SEMI() typedef int __CONCAT_WITH_EVAL__(__ignore__, __LINE__)

// Concatenates the values A and B without evaluating them if they're macros.
#define __CONCAT_NO_EVAL__(A, B) A##B

// Concatenates the value A and B, evaluating A and B first if they are macros.
#define __CONCAT_WITH_EVAL__(A, B) __CONCAT_NO_EVAL__(A, B)

#endif // _GLOBALS
