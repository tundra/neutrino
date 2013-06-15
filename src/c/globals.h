// Standard includes and definitions available everywhere.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _GLOBALS
#define _GLOBALS

// Shorthand for pointers into memory.
typedef uint8_t *address_t;

// The type to cast pointers to when you need to do advanced pointer
// arithmetic.
typedef uint32_t address_arith_t;

// Shorthands for commonly used sizes.
#define kKB 1024
#define kMB (kKB * kKB)

#endif // _GLOBALS
