// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Standard includes and definitions available everywhere.


#ifndef _GLOBALS
#define _GLOBALS

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define SWALLOW_SEMI(U) typedef int __CONCAT_WITH_EVAL__(__ignore_##U##__, __LINE__)

// Concatenates the values A and B without evaluating them if they're macros.
#define __CONCAT_NO_EVAL__(A, B) A##B

// Concatenates the value A and B, evaluating A and B first if they are macros.
#define __CONCAT_WITH_EVAL__(A, B) __CONCAT_NO_EVAL__(A, B)

// Forward declares a struct type with the given name such that it can be
// referred to by that naked name. This can only appear once but if there's a
// conflict it should mean that one of the occurrences can safely be removed.
#define FORWARD(name_t) typedef struct name_t name_t

// Returns the greatest of a and b.
static size_t max_size(size_t a, size_t b) {
  return (a < b) ? b : a;
}

// Returns the smallest of a and b.
static size_t min_size(size_t a, size_t b) {
  return (a < b) ? a : b;
}

// Ensures that the compiler knows that the expression is used but doesn't cause
// it to be executed.
#define USE(E) do { if (false) { E; } } while (false)

// The negative int32 with the largest possible magnitude. Beware of implicit
// conversions to unsigned/wider int types which eagerly mess with this value.
#define kMostNegativeInt32 ((int32_t) 0x80000000)


#endif // _GLOBALS
