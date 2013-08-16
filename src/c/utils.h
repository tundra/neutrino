// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _UTILS
#define _UTILS

#include "globals.h"
#include "value.h"

#include <stdarg.h>


// --- S t r i n g ---

// A C string with a length.
struct string_t {
  size_t length;
  const char *chars;
};

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

// Returns an integer indicating how a and b relate in lexical ordering. It
// holds that (string_compare(a, b) REL 0) when (a REL b) for a relational
// operator REL.
int string_compare(string_t *a, string_t *b);

// Returns true iff the given string is equal to the given c-string.
bool string_equals_cstr(string_t *a, const char *b);

// Calculates a hash code for the given string.
size_t string_hash(string_t *str);


// --- B l o b ---

// A block of data with a length.
struct blob_t {
  size_t length;
  byte_t *data;
};

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

// An allocator encapsulates a source of memory from the system.
typedef struct {
  // Function to call to do allocation.
  void *(*malloc)(void *data, size_t size);
  // Function to call to dispose memory. Note: the memory to deallocate is
  // the second arguments.
  void (*free)(void *data, void *ptr);
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
struct string_buffer_t {
  // Size of string currently in the buffer.
  size_t length;
  // Length of the total buffer (including null terminator).
  size_t capacity;
  // The actual data.
  char *chars;
  // The allocator to use to grab memory.
  allocator_t allocator;
};

// Initialize a string buffer. If an allocator is passed it will be used for
// all allocation, otherwise the default system allocator will be used.
void string_buffer_init(string_buffer_t *buf, allocator_t *allocator);

// Disposes the given string buffer.
void string_buffer_dispose(string_buffer_t *buf);

// Add a single character to this buffer.
void string_buffer_putc(string_buffer_t *buf, char c);

// Append the given text to the given buffer.
void string_buffer_printf(string_buffer_t *buf, const char *format, ...);

// Append the given text to the given buffer.
void string_buffer_vprintf(string_buffer_t *buf, const char *format, va_list argp);

// Null-terminates the buffer and stores the result in the given out parameter.
// The string is still backed by the buffer and so becomes invalid when the
// buffer is disposed.
void string_buffer_flush(string_buffer_t *buf, string_t *str_out);


// --- B y t e   b u f f e r ---

// Buffer for building a block of bytes incrementally.
typedef struct {
  // Size of data currently in the buffer.
  size_t length;
  // Length of the total buffer.
  size_t capacity;
  // The actual data.
  uint8_t *data;
  // The allocator to use to grab memory.
  allocator_t allocator;
} byte_buffer_t;

// Initialize a byte buffer. If an allocator is passed it will be used for
// all allocation, otherwise the default system allocator will be used.
void byte_buffer_init(byte_buffer_t *buf, allocator_t *alloc_or_null);

// Disposes the given byte buffer.
void byte_buffer_dispose(byte_buffer_t *buf);

// Add a byte to the given buffer.
void byte_buffer_append(byte_buffer_t *buf, uint8_t value);

// Write the current contents to the given blob. The data in the blob will
// still be backed by this buffer so disposing this will make the blob invalid.
void byte_buffer_flush(byte_buffer_t *buf, blob_t *blob_out);


// --- B i t   v e c t o r ---

// Bit vectors smaller than this should be allocated inline.
#define kSmallBitVectorLimit 128

// The size in bytes of the backing store of small bit vectors.
#define kBitVectorInlineDataSize (kSmallBitVectorLimit / 8)

// Data used for bit vectors smaller than kSmallBitVectorLimit which are stored
// inline without heap allocation.
typedef struct {
  uint8_t inline_data[kBitVectorInlineDataSize];
} small_bit_vector_store_t;

// Data used for bit vectors of size kSmallBitVectorLimit or larger which get
// their storage on the heap.
typedef struct {
  uint8_t *alloced_data;
} large_bit_vector_store_t;

// A compact vector of bits.
typedef struct {
  // How many bits?
  size_t length;
  // The storage array.
  uint8_t *data;
  // The source of the storage, either allocated inline or on the heap.
  union {
    small_bit_vector_store_t as_small;
    large_bit_vector_store_t as_large;
  };
} bit_vector_t;

// Initializes a bit vector to the given value. If anything goes wrong, for
// instance if it's a large bit vector and heap allocation fails, a signal is
// returned.
value_t bit_vector_init(bit_vector_t *vector, size_t length, bool value);

// Returns any resources used by the give bit vector.
void bit_vector_dispose(bit_vector_t *vector);

// Sets the index'th bit in the give bit vector to the given value.
void bit_vector_set_at(bit_vector_t *vector, size_t index, bool value);

// Returns the value of the index'th element in this bit vector.
bool bit_vector_get_at(bit_vector_t *vector, size_t index);


#endif // _UTILS
