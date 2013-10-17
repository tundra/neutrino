// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "check.h"
#include "globals.h"
#include "utils.h"
#include "value-inl.h"

#include <stdarg.h>


void string_init(string_t *str, const char *chars) {
  str->chars = chars;
  str->length = strlen(chars);
}

size_t string_length(string_t *str) {
  return str->length;
}

char string_char_at(string_t *str, size_t index) {
  CHECK_REL("string index out of bounds", index, <, string_length(str));
  return str->chars[index];
}

void string_copy_to(string_t *str, char *dest, size_t count) {
  // The count must be strictly greater than the number of chars because we
  // also need to fit the terminating null character.
  CHECK_REL("string copy destination too small", string_length(str), <, count);
  strncpy(dest, str->chars, string_length(str) + 1);
}

bool string_equals(string_t *a, string_t *b) {
  size_t length = string_length(a);
  if (length != string_length(b))
    return false;
  for (size_t i = 0; i < length; i++) {
    if (string_char_at(a, i) != string_char_at(b, i))
      return false;
  }
  return true;
}

int string_compare(string_t *a, string_t *b) {
  size_t a_length = string_length(a);
  size_t b_length = string_length(b);
  if (a_length != b_length)
    return a_length - b_length;
  for (size_t i = 0; i < a_length; i++) {
    char a_char = string_char_at(a, i);
    char b_char = string_char_at(b, i);
    if (a_char != b_char)
      return a_char - b_char;
  }
  return 0;
}

bool string_equals_cstr(string_t *a, const char *str) {
  string_t b;
  string_init(&b, str);
  return string_equals(a, &b);
}

size_t string_hash(string_t *str) {
  // This is a dreadful hash but it has the right properties. Improve later.
  size_t length = string_length(str);
  size_t result = length;
  for (size_t i = 0; i < length; i++)
    result = (result << 1) ^ (string_char_at(str, i));
  return result;
}


void blob_init(blob_t *blob, byte_t *data, size_t length) {
  blob->length = length;
  blob->data = data;
}

size_t blob_length(blob_t *blob) {
  return blob->length;
}

byte_t blob_byte_at(blob_t *blob, size_t index) {
  CHECK_REL("blob index out of bounds", index, <, blob_length(blob));
  return blob->data[index];
}

void blob_fill(blob_t *blob, byte_t value) {
  memset(blob->data, value, blob_length(blob));
}

void blob_copy_to(blob_t *src, blob_t *dest) {
  CHECK_REL("blob copy destination too small", blob_length(dest), >=, blob_length(src));
  memcpy(dest->data, src->data, blob_length(src));
}

static const byte_t kMallocHeapMarker = 0xB0;

memory_block_t memory_block_empty() {
  return new_memory_block(NULL, 0);
}

memory_block_t new_memory_block(void *memory, size_t size) {
  return (memory_block_t) {memory, size};
}

bool memory_block_is_empty(memory_block_t block) {
  return block.memory == NULL;
}

// Throws away the data argument and just calls malloc.
static memory_block_t system_malloc_trampoline(void *data, size_t size) {
  CHECK_EQ("invalid system allocator", NULL, data);
  void *result = malloc(size);
  if (result == NULL) {
    return memory_block_empty();
  } else {
    memset(result, kMallocHeapMarker, size);
    return (memory_block_t) {result, size};
  }
}

// Throws away the data argument and just calls free.
static void system_free_trampoline(void *data, memory_block_t memory) {
  CHECK_EQ("invalid system allocator", NULL, data);
  free(memory.memory);
}

void init_system_allocator(allocator_t *alloc) {
  alloc->malloc = system_malloc_trampoline;
  alloc->free = system_free_trampoline;
  alloc->data = NULL;
}

memory_block_t allocator_malloc(allocator_t *alloc, size_t size) {
  return (alloc->malloc)(alloc->data, size);
}

void allocator_free(allocator_t *alloc, memory_block_t memory) {
  (alloc->free)(alloc->data, memory);
}

memory_block_t allocator_default_malloc(size_t size) {
  return allocator_malloc(allocator_get_default(), size);
}

void allocator_default_free(memory_block_t block) {
  allocator_free(allocator_get_default(), block);
}

allocator_t kSystemAllocator;
allocator_t *allocator_default = NULL;

allocator_t *allocator_get_default() {
  if (allocator_default == NULL) {
    init_system_allocator(&kSystemAllocator);
    allocator_default = &kSystemAllocator;
  }
  return allocator_default;
}

allocator_t *allocator_set_default(allocator_t *value) {
  allocator_t *previous = allocator_get_default();
  allocator_default = value;
  return previous;
}

void string_buffer_init(string_buffer_t *buf) {
  buf->length = 0;
  buf->memory = allocator_default_malloc(128);
}

void string_buffer_dispose(string_buffer_t *buf) {
  allocator_default_free(buf->memory);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void string_buffer_ensure_capacity(string_buffer_t *buf,
    size_t length) {
  if (length < buf->memory.size)
    return;
  size_t new_capacity = (length * 2);
  memory_block_t new_memory = allocator_default_malloc(new_capacity);
  memcpy(new_memory.memory, buf->memory.memory, buf->length);
  allocator_default_free(buf->memory);
  buf->memory = new_memory;
}

void string_buffer_append(string_buffer_t *buf, string_t *str) {
  string_buffer_ensure_capacity(buf, buf->length + string_length(str));
  char *chars = buf->memory.memory;
  string_copy_to(str, chars + buf->length, buf->memory.size - buf->length);
  buf->length += string_length(str);
}

void string_buffer_putc(string_buffer_t *buf, char c) {
  string_buffer_ensure_capacity(buf, buf->length + 1);
  char *chars = buf->memory.memory;
  chars[buf->length] = c;
  buf->length++;
}

void string_buffer_printf(string_buffer_t *buf, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(buf, fmt, argp);
  va_end(argp);
}

void string_buffer_vprintf(string_buffer_t *buf, const char *fmt, va_list argp) {
  // Write the formatted string into a temporary buffer.
  static const size_t kMaxSize = 1024;
  char buffer[kMaxSize + 1];
  // Null terminate explicitly just to be on the safe side.
  buffer[kMaxSize] = '\0';
  size_t written = vsnprintf(buffer, kMaxSize, fmt, argp);
  // TODO: fix this if we ever hit it.
  CHECK_REL("temp buffer too small", written, <, kMaxSize);
  // Then write the temp string into the string buffer.
  string_t data = {written, buffer};
  string_buffer_append(buf, &data);
}

void string_buffer_flush(string_buffer_t *buf, string_t *str_out) {
  CHECK_REL("no room for null terminator", buf->length, <, buf->memory.size);
  char *chars = buf->memory.memory;
  chars[buf->length] = '\0';
  str_out->length = buf->length;
  str_out->chars = chars;
}


// --- B y t e   b u f f e r ---

void byte_buffer_init(byte_buffer_t *buf) {
  buf->length = 0;
  buf->memory = allocator_default_malloc(128);
}

void byte_buffer_dispose(byte_buffer_t *buf) {
  allocator_default_free(buf->memory);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void byte_buffer_ensure_capacity(byte_buffer_t *buf, size_t length) {
  if (length < buf->memory.size)
    return;
  size_t new_capacity = (length * 2);
  memory_block_t new_memory = allocator_default_malloc(new_capacity);
  memcpy(new_memory.memory, buf->memory.memory, buf->length);
  allocator_default_free(buf->memory);
  buf->memory = new_memory;
}

void byte_buffer_append(byte_buffer_t *buf, uint8_t value) {
  byte_buffer_ensure_capacity(buf, buf->length + 1);
  ((byte_t*) buf->memory.memory)[buf->length] = value;
  buf->length++;
}

void byte_buffer_flush(byte_buffer_t *buf, blob_t *blob_out) {
  blob_init(blob_out, buf->memory.memory, buf->length);
}


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
    vector->data = memory.memory;
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
  // Fisher–Yates shuffle
  CHECK_REL("element size too big", elem_size, <, 16);
  byte_t scratch[16];
  byte_t *start = data;
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
  // This should really be a signal such that it's safe to enter any value (not
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
    if (level.encoded == value.encoded)
      return new_signal(scCircular);
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
    object_family_t family) {
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

void hash_stream_write_data(hash_stream_t *stream, void *ptr, size_t size) {
  uint8_t *bytes = ptr;
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
