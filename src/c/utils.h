#ifndef _UTILS
#define _UTILS

#include "globals.h"

#include <stdarg.h>


// --- S t r i n g ---

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


// --- B l o b ---

// A block of data with a length.
typedef struct {
  size_t length;
  byte_t *data;
} blob_t;

// Initializes a blob to hold this data.
void blob_init(blob_t *blob, byte_t *data, size_t length);

// The number of bytes in this blob.
size_t blob_length(blob_t *blob);

// Returns the index'th byte in the given blob.
byte_t blob_byte_at(blob_t *blob, size_t index);

// Fills this blob's data with the given value.
void blob_fill(blob_t *blob, byte_t value);

// Write the contents of the source blob into the destination.
void blob_copy_to(blob_t *src, blob_t *dest);


// --- A l l o c a t o r ---

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


// --- S t r i n g   b u f f e r ---

// Buffer for building a string incrementally.
typedef struct {
  // Size of string currently in the buffer.
  size_t length;
  // Length of the total buffer (including null terminator).
  size_t capacity;
  // The actual data.
  char *chars;
  // The allocator to use to grab memory.
  allocator_t allocator;
} string_buffer_t;

// Initialize a string buffer. If an allocator is passed it will be used for
// all allocation, otherwise the default system allocator will be used.
void string_buffer_init(string_buffer_t *buf, allocator_t *allocator);

// Disposes the given string buffer.
void string_buffer_dispose(string_buffer_t *buf);

// Append the given text to the given buffer.
void string_buffer_printf(string_buffer_t *buf, const char *format, ...);

// Append the given text to the given buffer.
void string_buffer_vprintf(string_buffer_t *buf, const char *format, va_list argp);

// Null-terminates the buffer and stores the result in the given out parameter.
// The string is still backed by the buffer and so becomes invalid when the
// buffer is disposed.
void string_buffer_flush(string_buffer_t *buf, string_t *str_out);

#endif // _UTILS
