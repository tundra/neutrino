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

// Calculates a hash code for the given string.
size_t string_hash(string_t *str);


// An allocator function.
typedef address_t (alloc_t)(void *data, size_t size);

// A deallocator function. Note: the pointer to deallocate is the _second_ one.
typedef void (free_t)(void *data, address_t ptr);

// An allocator encapsulates a source of memory from the system.
typedef struct {
  // Function to call to do allocation.
  alloc_t *malloc;
  // Function to call to dispose memory.
  free_t *free;
  // Extra data that can be used by the alloc/dealloc functions.
  void *data;
} allocator_t;

// Initializes the given allocator to be the system allocator, using malloc and
// free.
void init_system_allocator(allocator_t *alloc);

// Allocates a block of memory using the given allocator.
address_t allocator_malloc(allocator_t *alloc, size_t size);

// Frees a block of memory using the given allocator.
void allocator_free(allocator_t *alloc, address_t ptr);

#endif // _UTILS
