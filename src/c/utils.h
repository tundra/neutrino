#ifndef _UTILS
#define _UTILS

#include "globals.h"

// A C string with a length.
typedef struct {
  size_t length;
  const char *chars;
} string_t;

// Initializes this string to hold the given characters.
void string_init(string_t *str, const char *chars);

// Returns the length of the given string.
size_t string_length(string_t *str);

// Returns the index'th character in the given string.
char string_char_at(string_t *str, size_t index);

// Write the contents of this string into the given buffer, which must hold
// at least count characters.
void string_copy_to(string_t *str, char *dest, size_t count);

// Returns true iff the two strings are equal.
bool string_equals(string_t *a, string_t *b);

#endif // _UTILS
