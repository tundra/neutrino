//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "globals.h"
#include "utils.h"
#include "utils/check.h"
#include "value-inl.h"

#include <stdarg.h>

blob_t new_blob(void *data, size_t size) {
  blob_t result;
  result.data = data;
  result.size = size;
  return result;
}

size_t blob_byte_length(blob_t blob) {
  return blob.size;
}

size_t blob_short_length(blob_t blob) {
  CHECK_EQ("unaligned short blob", 0, blob.size & 0x1);
  return blob.size >> 1;
}

byte_t blob_byte_at(blob_t blob, size_t index) {
  CHECK_REL("blob index out of bounds", index, <, blob_byte_length(blob));
  return ((byte_t*) blob.data)[index];
}

short_t blob_short_at(blob_t blob, size_t index) {
  CHECK_REL("blob index out of bounds", index, <, blob_short_length(blob));
  return ((short_t*) blob.data)[index];
}

void blob_fill(blob_t blob, byte_t value) {
  memset(blob.data, value, blob_byte_length(blob));
}

void blob_copy_to(blob_t src, blob_t dest) {
  CHECK_REL("blob copy destination too small", blob_byte_length(dest), >=,
      blob_byte_length(src));
  memcpy(dest.data, src.data, blob_byte_length(src));
}


// --- B y t e   b u f f e r ---

#define BUFFER_TYPE uint8_t
#define MAKE_BUFFER_NAME(SUFFIX) byte_buffer_##SUFFIX

#include "buffer-tmpl.c"

#undef BUFFER_TYPE
#undef MAKE_BUFFER_NAME


// --- S h o r t   b u f f e r ---

#define BUFFER_TYPE uint16_t
#define MAKE_BUFFER_NAME(SUFFIX) short_buffer_##SUFFIX

#include "buffer-tmpl.c"

#undef BUFFER_TYPE
#undef MAKE_BUFFER_NAME


// --- B i t   v e c t o r ---

static bool bit_vector_is_small(bit_vector_t *vector) {
  return vector->length < kSmallBitVectorLimit;
}

value_t bit_vector_init(bit_vector_t *vector, size_t length, bool value) {
  vector->length = length;
  size_t byte_size = align_size(8, length) >> 3;
  if (bit_vector_is_small(vector)) {
    vector->data = vector->storage.as_small.inline_data;
  } else {
    memory_block_t memory = allocator_default_malloc(byte_size);
    vector->storage.as_large.memory = memory;
    vector->data = (byte_t*) memory.memory;
  }
  memset(vector->data, value ? 0xFF : 0x00, byte_size);
  return success();
}

void bit_vector_dispose(bit_vector_t *vector) {
  if (!bit_vector_is_small(vector))
    allocator_default_free(vector->storage.as_large.memory);
}

void bit_vector_set_at(bit_vector_t *vector, size_t index, bool value) {
  CHECK_REL("set bit vector out of bounds", index, <, vector->length);
  size_t segment = index >> 3;
  size_t offset = (index & 0x7);
  if (value) {
    vector->data[segment] |= (1 << offset);
  } else {
    vector->data[segment] &= ~(1 << offset);
  }
}

bool bit_vector_get_at(bit_vector_t *vector, size_t index) {
  CHECK_REL("get bit vector out of bounds", index, <, vector->length);
  size_t segment = index >> 3;
  size_t offset = (index & 0x7);
  return (vector->data[segment] >> offset) & 0x1;
}


// --- P s e u d o   r a n d o m ---

void pseudo_random_init(pseudo_random_t *random, uint32_t seed) {
  random->low = 362436069 + seed;
  random->high = 521288629 - seed;
}

uint32_t pseudo_random_next_uint32(pseudo_random_t *random) {
  uint32_t low = random->low;
  uint32_t high = random->high;
  uint32_t new_high = 23163 * (high & 0xFFFF) + (high >> 16);
  uint32_t new_low = 22965 * (low & 0xFFFF) + (low >> 16);
  random->low = new_low;
  random->high = new_high;
  return ((new_high & 0xFFFF) << 16) | (low & 0xFFFF);
}

uint32_t pseudo_random_next(pseudo_random_t *random, uint32_t max) {
  // NOTE: when the max is not a divisor in 2^32 this gives a small bias
  //   towards the smaller values in the range. For what this is used for that's
  //   probably not worth worrying about.
  return pseudo_random_next_uint32(random) % max;
}

void pseudo_random_shuffle(pseudo_random_t *random, void *data,
    size_t elem_count, size_t elem_size) {
  // Fisher-Yates shuffle
  CHECK_REL("element size too big", elem_size, <, 16);
  byte_t scratch[16];
  byte_t *start = (byte_t*) data;
  for (size_t i = 0; i < elem_count - 1; i++) {
    size_t target = elem_count - i - 1;
    size_t source = pseudo_random_next(random, target + 1);
    if (source == target)
      continue;
    // Move the value currently stored in target into scratch.
    memcpy(scratch, start + (elem_size * target), elem_size);
    // Move the source to target.
    memcpy(start + (elem_size * target), start + (elem_size * source), elem_size);
    // Move the value that used to be target into source.
    memcpy(start + (elem_size * source), scratch, elem_size);
  }
}


// --- C y c l e   d e t e c t o r ---

void cycle_detector_init_bottom(cycle_detector_t *detector) {
  detector->remaining_before_check = kCircularObjectDepthThreshold;
  // This should really be a condition such that it's safe to enter any value (not
  // that you'd want to enter an integer but it's one fewer special cases) but
  // it causes a valgrind error in check_for_cycles if it is so we'll use an
  // integer instead, even though I'm almost certain the valgrind problem is
  // either a valgrind bug or a compiler bug.
  detector->value = new_integer(-1);
  detector->outer = NULL;
}

// Check whether the given cycle detector chain contains a cycle the given
// value is part of.
static value_t check_for_cycles(cycle_detector_t *detector, value_t value) {
  cycle_detector_t *current = detector;
  while (current != NULL) {
    value_t level = current->value;
    if (is_same_value(level, value))
      return new_condition(ccCircular);
    current = current->outer;
  }
  return success();
}

value_t cycle_detector_enter(cycle_detector_t *outer, cycle_detector_t *inner,
    value_t value) {
  CHECK_REL("invalid outer in cycle check", outer->remaining_before_check, >, 0);
  inner->remaining_before_check = outer->remaining_before_check - 1;
  inner->value = value;
  inner->outer = outer;
  if (inner->remaining_before_check == 0) {
    inner->remaining_before_check = kCircularObjectCheckInterval;
    return check_for_cycles(outer, value);
  } else {
    return success();
  }
}


// --- H a s h   s t r e a m ---

void hash_stream_init(hash_stream_t *stream) {
  stream->hash = 0;
}

void hash_stream_write_tags(hash_stream_t *stream, value_domain_t domain,
    heap_object_family_t family) {
  hash_stream_write_int64(stream, (family << 8) | domain);
}

static inline uint64_t rotate(uint64_t value, uint8_t rotation) {
  return (value << rotation) | (value >> (64 - rotation));
}

void hash_stream_write_int64(hash_stream_t *stream, int64_t value) {
  // TODO: I bet this is actually more expensive than a proper implementation
  //   would be. But this isn't the time to look into that.
  uint64_t hash_before = stream->hash;
  uint8_t rotation = (hash_before ^ value) & 0x3F;
  uint64_t rotated = rotate(hash_before, rotation);
  stream->hash = rotated ^ value ^ 0xA90F0F60EB3D4C56LL;
;
}

void hash_stream_write_data(hash_stream_t *stream, const void *ptr, size_t size) {
  const byte_t *bytes = (const byte_t*) ptr;
  // Look away, it's hideous!
  // TODO: It should be possible to do this block-by-block, the tricky part is
  //   making sure that identical chunks of data hash the same whether they're
  //   aligned or not. Or ensuring that all blocks of data will be 64-bit
  //   aligned.
  for (size_t i = 0; i < size; i++) {
    hash_stream_write_int64(stream, bytes[i] ^ rotate(i, i & 0x3F));
  }
}

int64_t hash_stream_flush(hash_stream_t *stream) {
  hash_stream_write_int64(stream, 0x048812362BDB451ELL);
  return stream->hash;
}


// --- B a s e   6 4 ---

// Generated using python:
//
// chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
// for i in xrange(0, 256):
//   index = chars.find(chr(i))
//   if index == -1:
//     index = 255
//   print ("%s," % index),
static uint8_t kBase64CharToSextet[256] = {
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 62, 255, 255,
  255, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 255, 255, 255, 255, 255, 255,
  255, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
  21, 22, 23, 24, 25, 255, 255, 255, 255, 255, 255, 26, 27, 28, 29, 30, 31, 32,
  33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

void base64_decode(utf8_t str, byte_buffer_t *out) {
  size_t str_length = string_size(str);
  CHECK_EQ("invalid base64 string", 0, str_length & 0x3);
  for (size_t i = 0; i < str_length; i += 4) {
    // Read the next block of 4 characters.
    uint8_t a = kBase64CharToSextet[(uint8_t) string_byte_at(str, i)];
    uint8_t b = kBase64CharToSextet[(uint8_t) string_byte_at(str, i + 1)];
    byte_buffer_append(out, (a << 2) | (b >> 4));
    uint8_t c = kBase64CharToSextet[(uint8_t) string_byte_at(str, i + 2)];
    if (c != 255) {
      byte_buffer_append(out, (b << 4) | (c >> 2));
      uint8_t d = kBase64CharToSextet[(uint8_t) string_byte_at(str, i + 3)];
      if (d != 255)
        byte_buffer_append(out, (c << 6) | d);
    }
  }
}


// --- W o r d y ---

// The tables from which to grab characters.
static const utf8_t kCharTables[2] = {
  {20, "bcdfghjklmnpqrstvwxz"},
  {6, "aeiouy"}
};

void wordy_encode(int64_t signed_value, char *buf, size_t bufc) {
  size_t cursor = 0;
  // Use the top bit to determine whether to start with a vowel or a
  // consonant.
  size_t table_index;
  uint64_t value;
  if (signed_value < 0) {
    // If the top bit is set we flip the whole word; that way small negative
    // values become short words. The +1 is such that -1 maps to 0 rather than
    // 1 which would cause the 0'th negative wordy string to be unused. Also the
    // largest negative value wouldn't fit as positive.
    table_index = 1;
    value = -(signed_value + 1);
  } else {
    table_index = 0;
    value = signed_value;
  }
  // Even if value is 0 we have to run at least once.
  do {
    utf8_t table = kCharTables[table_index];
    size_t char_index = value % string_size(table);
    char c = table.chars[char_index];
    CHECK_REL("wordy_encode buf too small", cursor, <, bufc);
    buf[cursor] = c;
    table_index = 1 - table_index;
    cursor++;
    value /= string_size(table);
  } while (value != 0);
  CHECK_REL("wordy_encode buf too small", cursor, <, bufc);
  buf[cursor] = 0;
  // It might seem to make sense to reverse the result such that the least
  // significant bits affect the rightmost characters but in practice, since
  // these are typically read left-to-right and differences are most likely
  // to be in the least significant bits, it's easier to read if values with
  // different low bits result in words with differences to the left.
}


// --- V a l u e   a r r a y ---

void value_array_copy_to(value_array_t *src, value_array_t *dest) {
  CHECK_REL("array copy destination too small", dest->length, >=,
      src->length);
  memcpy(dest->start, src->start, src->length * kValueSize);
}

void value_array_fill(value_array_t dest, value_t value) {
  for (size_t i = 0; i < dest.length; i++)
    dest.start[i] = value;
}
