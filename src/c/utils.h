// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _UTILS
#define _UTILS

#include "globals.h"
#include "value.h"

#include <stdarg.h>


// --- M i s c ---

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
static size_t align_size(uint32_t alignment, size_t size) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}


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


// A small snippet of a string that can be encoded as a 32-bit integer. Create
// a hint cheaply using the STRING_HINT macro.
typedef struct {
  // The characters of this hint.
  const char value[4];
} string_hint_t;

// Reads the characters from a string hint, storing them in a plain c-string.
void string_hint_to_c_str(string_hint_t hint, char c_str_out[5]);


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

// A block of memory as returned from an allocator. Bundling the length with the
// memory allows us to check how much memory is live at any given time.
typedef struct {
  // The actual memory.
  void *memory;
  // The number of bytes of memory (minus any overhead added by the allocator).
  size_t size;
} memory_block_t;

// Returns true iff the given block is empty, say because allocation failed.
bool memory_block_is_empty(memory_block_t block);

// Resets the given block to the empty state.
memory_block_t memory_block_empty();

// Creates a new memory block with the given contents. Be sure to note that this
// doesn't allocate anything, just bundles previously allocated memory into a
// struct.
memory_block_t new_memory_block(void *memory, size_t size);

// An allocator encapsulates a source of memory from the system.
typedef struct {
  // Function to call to do allocation.
  memory_block_t (*malloc)(void *data, size_t size);
  // Function to call to dispose memory. Note: the memory to deallocate is
  // the second arguments.
  void (*free)(void *data, memory_block_t memory);
  // Extra data that can be used by the alloc/dealloc functions.
  void *data;
} allocator_t;

// Initializes the given allocator to be the system allocator, using malloc and
// free.
void init_system_allocator(allocator_t *alloc);

// Allocates a block of memory using the given allocator.
memory_block_t allocator_malloc(allocator_t *alloc, size_t size);

// Allocates the specified amount of memory using the default allocator.
memory_block_t allocator_default_malloc(size_t size);

// Frees the given block of memory using the default allocator.
void allocator_default_free(memory_block_t block);

// Frees a block of memory using the given allocator.
void allocator_free(allocator_t *alloc, memory_block_t memory);

// Returns the current default allocator. If none has been explicitly set this
// will be the system allocator.
allocator_t *allocator_get_default();

// Sets the default allocator, returning the previous value.
allocator_t *allocator_set_default(allocator_t *value);


// --- S t r i n g   b u f f e r ---

// Buffer for building a string incrementally.
struct string_buffer_t {
  // Size of string currently in the buffer.
  size_t length;
  // The data buffer.
  memory_block_t memory;
};

// Initialize a string buffer.
void string_buffer_init(string_buffer_t *buf);

// Disposes the given string buffer.
void string_buffer_dispose(string_buffer_t *buf);

// Add a single character to this buffer.
void string_buffer_putc(string_buffer_t *buf, char c);

// Append the given text to the given buffer.
void string_buffer_printf(string_buffer_t *buf, const char *format, ...);

// Append the given text to the given buffer.
void string_buffer_vprintf(string_buffer_t *buf, const char *format, va_list argp);

// Append the contents of the string to this buffer.
void string_buffer_append(string_buffer_t *buf, string_t *str);

// Null-terminates the buffer and stores the result in the given out parameter.
// The string is still backed by the buffer and so becomes invalid when the
// buffer is disposed.
void string_buffer_flush(string_buffer_t *buf, string_t *str_out);


// --- B y t e   b u f f e r ---

// Buffer for building a block of bytes incrementally.
typedef struct {
  // Size of data currently in the buffer.
  size_t length;
  // The data block.
  memory_block_t memory;
} byte_buffer_t;

// Initialize a byte buffer.
void byte_buffer_init(byte_buffer_t *buf);

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
  // The heap allocated memory block holding the vector.
  memory_block_t memory;
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
  } storage;
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


// --- P s e u d o   r a n d o m ---

// Data for multiply-with-carry pseudo-random generator.
// See See http://www.ms.uky.edu/~mai/RandomNumber.
typedef struct {
  uint32_t low;
  uint32_t high;
} pseudo_random_t;

// Initializes a pseudo-random generator with the given seed.
void pseudo_random_init(pseudo_random_t *random, uint32_t seed);

// Returns the next pseudo-random uint32.
uint32_t pseudo_random_next_uint32(pseudo_random_t *random);

// Returns the next pseudo-random number greater than 0 and less than the given
// max.
uint32_t pseudo_random_next(pseudo_random_t *random, uint32_t max);

// Shuffles the given array of 'elem_count' elements, each of which is 'elem_size'
// wide.
void pseudo_random_shuffle(pseudo_random_t *random, void *data,
    size_t elem_count, size_t elem_size);


// --- C y c l e   d e t e c t o r ---

// Data used for cycle detection in recursive operations that act on possibly
// circular data structures. The way circle detection works is to keep the
// path from the root to the current object on a stack-allocated chain and then
// at certain depths check whether any object occur earlier in the chain. This
// is expensive in the case of very large object structures but the uses there
// are for this (printing, hashing, etc.) aren't places where you'd generally
// see those anyway. For shallow objects it should be pretty low overhead.
struct cycle_detector_t {
  // How many levels of recursion do we have left before we'll do another cycle
  // check?
  size_t remaining_before_check;
  // The entered object.
  value_t value;
  // The enclosing cycle detector.
  cycle_detector_t *outer;
};

// Initializes the "bottom" cycle detector that has no parents.
void cycle_detector_init_bottom(cycle_detector_t *detector);

// Checks for cycles using the given outer cycle detector and initialies the
// given inner cycle detector such that it can be passed along to the children
// of the given value. If a cycle is detected returns a signal, otherwise
// success.
value_t cycle_detector_enter(cycle_detector_t *outer, cycle_detector_t *inner,
    value_t value);

// This is how deep we'll recurse into an object before we assume that we're
// maybe dealing with a circular object.
static const size_t kCircularObjectDepthThreshold = 16;

// At which depths will we check for circles when looking at a possibly circular
// object.
static const size_t kCircularObjectCheckInterval = 8;


// --- H a s h   S t r e a m ---

// An accumulator that you can write data to and extract a hash value from. The
// actual implementation is pretty awful but it's hard to tune before the
// implementation is further along.
struct hash_stream_t {
  // The current accumulated hash value.
  int64_t hash;
};

// Initialize a hash stream.
void hash_stream_init(hash_stream_t *stream);

// Writes a domain/family tag pair. By including this in the hash you'll get
// different hash values for different types of objects even when their contents
// are the same.
void hash_stream_write_tags(hash_stream_t *stream, value_domain_t domain,
    object_family_t family);

// Writes a 64-bit integer into the hash.
void hash_stream_write_int64(hash_stream_t *stream, int64_t value);

// Writes a block of data of the given size (in bytes) to the hash.
void hash_stream_write_data(hash_stream_t *stream, const void *ptr, size_t size);

// Completes the hash computation and returns the hash value. This can only
// be called once since it clobbers the internal state of the stream.
int64_t hash_stream_flush(hash_stream_t *stream);


// --- B a s e   6 4 ---

// Decodes a base-64 encoded string as raw bytes which will be written to the
// given out buffer.
void base64_decode(string_t *str, byte_buffer_t *out);


#endif // _UTILS
