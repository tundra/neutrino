//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _UTILS
#define _UTILS

#include "globals.h"
#include "utils/alloc.h"
#include "utils/string.h"
#include "value.h"

#include <stdarg.h>

// --- M i s c ---

// Returns a pointer greater than or equal to the given pointer which is
// aligned to an 'alignment' boundary.
static size_t align_size(uint32_t alignment, size_t size) {
  return (size + (alignment - 1)) & ~(alignment - 1);
}


// --- B l o b ---

// A block of data with a length.
struct blob_t {
  size_t byte_length;
  byte_t *data;
};

// Initializes a blob to hold this data.
void blob_init(blob_t *blob, byte_t *data, size_t byte_length);

// The number of bytes in this blob.
size_t blob_byte_length(blob_t *blob);

// The number of shorts in this blob.
size_t blob_short_length(blob_t *blob);

// Returns the index'th byte in the given blob.
byte_t blob_byte_at(blob_t *blob, size_t index);

// Returns the index'th short in the given blob.
short_t blob_short_at(blob_t *blob, size_t index);

// Fills this blob's data with the given value.
void blob_fill(blob_t *blob, byte_t value);

// Write the contents of the source blob into the destination.
void blob_copy_to(blob_t *src, blob_t *dest);


// --- B y t e   b u f f e r ---

FORWARD(byte_buffer_t);
FORWARD(byte_buffer_cursor_t);

#define BUFFER_TYPE byte_t
#define MAKE_BUFFER_NAME(SUFFIX) byte_buffer_##SUFFIX

#include "buffer-tmpl.h"

#undef BUFFER_TYPE
#undef MAKE_BUFFER_NAME


// --- S h o r t   b u f f e r ---

FORWARD(short_buffer_t);
FORWARD(short_buffer_cursor_t);

#define BUFFER_TYPE short_t
#define MAKE_BUFFER_NAME(SUFFIX) short_buffer_##SUFFIX

#include "buffer-tmpl.h"

#undef BUFFER_TYPE
#undef MAKE_BUFFER_NAME


// --- B i t   v e c t o r ---

// Bit vectors smaller than this should be allocated inline.
#define kSmallBitVectorLimit 128

// The size in bytes of the backing store of small bit vectors.
#define kBitVectorInlineDataSize (kSmallBitVectorLimit / 8)

// Data used for bit vectors smaller than kSmallBitVectorLimit which are stored
// inline without heap allocation.
typedef struct {
  byte_t inline_data[kBitVectorInlineDataSize];
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
  byte_t *data;
  // The source of the storage, either allocated inline or on the heap.
  union {
    small_bit_vector_store_t as_small;
    large_bit_vector_store_t as_large;
  } storage;
} bit_vector_t;

// Initializes a bit vector to the given value. If anything goes wrong, for
// instance if it's a large bit vector and heap allocation fails, a condition is
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
// of the given value. If a cycle is detected returns a condition, otherwise
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
    heap_object_family_t family);

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


// --- W o r d y  ---

// The average number of bits per letter is 3.7; 64 / 3.7 is 17.3, rounded up
// is 18, add 1 because 3.7 is a slight over-estimate for finite-length strings,
// add 1 for the null terminator.
#define kMaxWordyNameSize 20

// Encodes the given 64-bit quantity as a pronounceable name. The given buffer
// must be at least wide enough to holde the name plus the null terminator; the
// kMaxWordyNameSize macro gives a size that is guaranteed to be wide enough for
// any 64-bit value.
void wordy_encode(int64_t value, char *buf, size_t bufc);


/// ## Value array

// A generic array of values.
typedef struct {
  // Beginning of the array. Can be NULL if the array is empty.
  value_t *start;
  // Length of the array in values.
  size_t length;
} value_array_t;

// Shorthand for creating a new value array struct.
static value_array_t new_value_array(value_t *start, size_t length) {
  value_array_t result = {start, length};
  return result;
}

#endif // _UTILS
