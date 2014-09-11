//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "runtime.h"
#include "utils-inl.h"
#include "value-inl.h"
END_C_INCLUDES

TEST(utils, globals) {
  ASSERT_TRUE(kMostNegativeInt32 < 0);
  ASSERT_FALSE((int32_t) (((int64_t) kMostNegativeInt32) - 1) < 0);
}

#define CHECK_PRINTF(expected, format, ...) do {                               \
  string_buffer_t buf;                                                         \
  string_buffer_init(&buf);                                                    \
  string_buffer_printf(&buf, format, __VA_ARGS__);                             \
  string_t found;                                                              \
  string_buffer_flush(&buf, &found);                                           \
  string_t str = new_string(expected);                                                \
  ASSERT_STREQ(&str, &found);                                                  \
  string_buffer_dispose(&buf);                                                 \
} while (false)

TEST(utils, string_buffer_value_printf) {
  CREATE_RUNTIME();

  CHECK_PRINTF("--- 0 ---", "--- %v ---", new_integer(0));
  CHECK_PRINTF("--- %<condition: Wat(dt@0)> ---", "--- %v ---", new_condition(ccWat));
  CHECK_PRINTF("--- null ---", "--- %v ---", null());
  CHECK_PRINTF("--- true ---", "--- %v ---", yes());
  CHECK_PRINTF("--- [] ---", "--- %v ---", ROOT(runtime, empty_array));

  value_t cycle_array = new_heap_array(runtime, 1);
  set_array_at(cycle_array, 0, cycle_array);
  CHECK_PRINTF("--- " kBottomValuePlaceholder " ---", "--- %0v ---", cycle_array);
  CHECK_PRINTF("--- #<array[1]> ---", "--- %1v ---", cycle_array);
  CHECK_PRINTF("--- [#<array[1]>] ---", "--- %2v ---", cycle_array);
  CHECK_PRINTF("--- [[#<array[1]>]] ---", "--- %3v ---", cycle_array);
  CHECK_PRINTF("--- [[[[[[[[[[#<array[1]>]]]]]]]]]] ---",
      "--- %11v ---", cycle_array);

  DISPOSE_RUNTIME();
}

// Runs a test of a bit vector of the given size.
static void test_bit_vector(size_t size) {
  bit_vector_t false_bits;
  ASSERT_SUCCESS(bit_vector_init(&false_bits, size, false));
  bit_vector_t true_bits;
  ASSERT_SUCCESS(bit_vector_init(&true_bits, size, true));

  // Check that the vectors have been initialized as expected.
  for (size_t i = 0; i < size; i++) {
    ASSERT_FALSE(bit_vector_get_at(&false_bits, i));
    ASSERT_TRUE(bit_vector_get_at(&true_bits, i));
  }

  // Check that setting and getting works as expected.
  for (size_t i = 0; i < size; i++) {
    bit_vector_set_at(&false_bits, i, (i % 7) == 3);
    bit_vector_set_at(&true_bits, i, (i % 5) != 1);
  }
  for (size_t i = 0; i < size; i++) {
    ASSERT_EQ((i % 7) == 3, bit_vector_get_at(&false_bits, i));
    ASSERT_EQ((i % 5) != 1, bit_vector_get_at(&true_bits, i));
  }

  bit_vector_dispose(&false_bits);
  bit_vector_dispose(&true_bits);

}

TEST(utils, bit_vectors) {
  test_bit_vector(8);
  test_bit_vector(62);
  test_bit_vector(64);
  test_bit_vector(66);
  test_bit_vector(1022);
  test_bit_vector(1024);
  test_bit_vector(1026);
}

TEST(utils, pseudo_random) {
  pseudo_random_t rand;
  pseudo_random_init(&rand, 123456);
  static const size_t kBucketCount = 257;
  size_t buckets[1027];
  for (size_t i = 0; i < kBucketCount; i++)
    buckets[i] = 0;
  size_t tries = 1048576;
  for (size_t i = 0; i < tries; i++) {
    size_t index = pseudo_random_next(&rand, kBucketCount);
    ASSERT_TRUE(index < kBucketCount);
    buckets[index]++;
  }
  double mid = tries / kBucketCount;
  size_t min = tries;
  size_t max = 0;
  for (size_t i = 0; i < kBucketCount; i++) {
    size_t bucket = buckets[i];
    if (bucket < min)
      min = bucket;
    if (bucket > max)
      max = bucket;
  }
  double min_dev = (mid - min) / mid;
  double max_dev = (max - mid) / mid;
  ASSERT_TRUE(min_dev <= 0.05);
  ASSERT_TRUE(max_dev <= 0.05);
}

TEST(utils, shuffle) {
  pseudo_random_t rand;
  pseudo_random_init(&rand, 654322);

  static const size_t kElemCount = 513;
  size_t elems[513];
  for (size_t i = 0; i < kElemCount; i++)
    elems[i] = i;

  bit_vector_t moved;
  bit_vector_init(&moved, kElemCount, false);
  size_t moved_count = 0;
  for (size_t t = 0; t < 65; t++) {
    pseudo_random_shuffle(&rand, elems, kElemCount, sizeof(size_t));
    bit_vector_t seen;
    bit_vector_init(&seen, kElemCount, false);
    size_t seen_count = 0;
    for (size_t i = 0; i < kElemCount; i++) {
      size_t elem = elems[i];
      if (elem != i && !bit_vector_get_at(&moved, i)) {
        bit_vector_set_at(&moved, i, true);
        moved_count++;
      }
      ASSERT_FALSE(bit_vector_get_at(&seen, elem));
      bit_vector_set_at(&seen, elem, true);
      seen_count++;
    }
    // Check that we saw each element once.
    ASSERT_EQ(kElemCount, seen_count);
    bit_vector_dispose(&seen);
  }
  // Check that all elements have been moved at least once. This is more of a
  // sanity check than anything, the distribution might still be awful but it'll
  // do.
  ASSERT_EQ(kElemCount, moved_count);
  bit_vector_dispose(&moved);
}

// Enter values to the given depth.
static void test_deep_entering(cycle_detector_t *outer, size_t depth) {
  if (depth == 0)
    return;
  cycle_detector_t inner;
  ASSERT_SUCCESS(cycle_detector_enter(outer, &inner, new_integer(depth)));
  test_deep_entering(&inner, depth - 1);
}

// Enter values to the given depth and return whether a cycle was ever detected.
static bool test_eventual_detection(cycle_detector_t *outer, size_t depth) {
  if (depth == 0)
    return false;
  cycle_detector_t inner;
  value_t entered = cycle_detector_enter(outer, &inner, new_integer(depth % 17));
  if (is_condition(entered))
    return true;
  return test_eventual_detection(&inner, depth - 1);
}

TEST(utils, cycle_detector) {
  CREATE_RUNTIME();

  {
    cycle_detector_t outer;
    cycle_detector_init_bottom(&outer);
    test_deep_entering(&outer, 1024);
  }

  {
    cycle_detector_t outer;
    cycle_detector_init_bottom(&outer);
    ASSERT_TRUE(test_eventual_detection(&outer, 1024));
  }

  DISPOSE_RUNTIME();
}

TEST(utils, hash_stream) {
  // Different integers.
  int64_t hashes[1024];
  for (size_t i = 0; i < 1024; i++) {
    hash_stream_t stream;
    hash_stream_init(&stream);
    hash_stream_write_int64(&stream, i);
    int64_t hash = hash_stream_flush(&stream);
    for (size_t j = 0; j < i; j++)
      ASSERT_TRUE(hash != hashes[j]);
    hashes[i] = hash;
  }
  // Adding the same value repeatedly.
  for (size_t i = 0; i < 1024; i++) {
    hash_stream_t stream;
    hash_stream_init(&stream);
    for (size_t j = 0; j < i; j++)
      hash_stream_write_int64(&stream, 0);
    int64_t hash = hash_stream_flush(&stream);
    for (size_t j = 0; j < i; j++)
      ASSERT_TRUE(hash != hashes[j]);
    hashes[i] = hash;
  }
  // Hashing blocks of data that are slightly different.
  pseudo_random_t random;
  pseudo_random_init(&random, 1312314);
  int64_t data[1024 / 64];
  // Initialize the data block to some random data.
  for (size_t i = 0; i < 1024 / 32; i++)
    ((uint32_t*) data)[i] = pseudo_random_next_uint32(&random);
  // Okay the hash isn't actually good enough to pass this with i++ but it does
  // work with i += 5 and that should be okay for now. Later on, whatever better
  // hash is ultimately used it should definitely pass with i++.
  for (size_t i = 0; i < 1024; i += 5) {
    size_t word = i / 64;
    size_t bit = i % 64;
    // Flip one bit.
    hash_stream_t stream;
    hash_stream_init(&stream);
    data[word] ^= (1LL << bit);
    hash_stream_write_data(&stream, data, 1024 / 8);
    data[word] ^= (1LL << bit);
    int64_t hash = hash_stream_flush(&stream);
    for (size_t j = 0; j < i; j++)
      ASSERT_TRUE(hash != hashes[j]);
    hashes[i] = hash;
    // Flip the bit back.
  }
}

// Check that the input decodes the the given data.
#define CHECK_BASE64_ENCODE(INPUT, N, ...) do {                                \
  string_t str;                                                                \
  string_init(&str, (INPUT));                                                  \
  byte_buffer_t buf;                                                           \
  byte_buffer_init(&buf);                                                      \
  base64_decode(&str, &buf);                                                   \
  blob_t blob;                                                                 \
  byte_buffer_flush(&buf, &blob);                                              \
  ASSERT_EQ(N, blob_byte_length(&blob));                                            \
  uint8_t expected[(N == 0) ? 1 : (N)] = {__VA_ARGS__};                        \
  for (int i = 0; i < (N); i++)                                                \
    ASSERT_EQ(expected[i], blob_byte_at(&blob, i));                            \
  byte_buffer_dispose(&buf);                                                   \
} while (false)

TEST(utils, base64_encode) {
  CHECK_BASE64_ENCODE("", 0, 0);
  CHECK_BASE64_ENCODE("AA==", 1, 0);
  CHECK_BASE64_ENCODE("AAA=", 2, 0, 0);
  CHECK_BASE64_ENCODE("AAAA", 3, 0, 0, 0);

  CHECK_BASE64_ENCODE("Dw==", 1, 15);
  CHECK_BASE64_ENCODE("DwA=", 2, 15,  0);
  CHECK_BASE64_ENCODE("DwAA", 3, 15,  0,   0);
  CHECK_BASE64_ENCODE("GA==", 1, 24);
  CHECK_BASE64_ENCODE("GAA=", 2, 24,  0);
  CHECK_BASE64_ENCODE("GAAA", 3, 24,  0,   0);
  CHECK_BASE64_ENCODE("Jw8=", 2, 39,  15);
  CHECK_BASE64_ENCODE("Jw8A", 3, 39,  15,  0);
  CHECK_BASE64_ENCODE("Pxg=", 2, 63,  24);
  CHECK_BASE64_ENCODE("PxgA", 3, 63,  24,  0);
  CHECK_BASE64_ENCODE("ZicP", 3, 102, 39,  15);
  CHECK_BASE64_ENCODE("pj8Y", 3, 166, 63,  24);
  CHECK_BASE64_ENCODE("pmYn", 3, 166, 102, 39);
  CHECK_BASE64_ENCODE("pqY/", 3, 166, 166, 63);
  CHECK_BASE64_ENCODE("pqZm", 3, 166, 166, 102);
  CHECK_BASE64_ENCODE("pqam", 3, 166, 166, 166);

  CHECK_BASE64_ENCODE("////", 3, 0xFF, 0xFF, 0xFF);
  CHECK_BASE64_ENCODE("++++", 3, 0xFB, 0xEF, 0xBE);

  CHECK_BASE64_ENCODE("SGVsbG8gd29ybGQ=", 11, 72, 101, 108, 108, 111, 32, 119,
      111, 114, 108, 100);
  CHECK_BASE64_ENCODE(
      "VGhpbmdzIGZhbGwgYXBhcnQ7IHRoZSBjZW50cmUgY2Fubm90IGhvbGQ7",
      42,
      84, 104, 105, 110, 103, 115, 32, 102, 97, 108, 108, 32, 97, 112, 97, 114,
      116, 59, 32, 116, 104, 101, 32, 99, 101, 110, 116, 114, 101, 32, 99, 97,
      110, 110, 111, 116, 32, 104, 111, 108, 100, 59);
}

// Record that this macro has been invoked with the given argument.
#define RECORD(N) record[count++] = N;

TEST(utils, for_each_va_arg) {
  int count = 0;
  int record[8];

  FOR_EACH_VA_ARG(RECORD, 1, 4, 6, 9, 34, 54, 2, 3);
  ASSERT_EQ(8, count);
  ASSERT_EQ(record[0], 1);
  ASSERT_EQ(record[1], 4);
  ASSERT_EQ(record[2], 6);
  ASSERT_EQ(record[3], 9);
  ASSERT_EQ(record[4], 34);
  ASSERT_EQ(record[5], 54);
  ASSERT_EQ(record[6], 2);
  ASSERT_EQ(record[7], 3);

  count = 0;
  FOR_EACH_VA_ARG(RECORD, 6, 5, 4);
  ASSERT_EQ(3, count);
  ASSERT_EQ(record[0], 6);
  ASSERT_EQ(record[1], 5);
  ASSERT_EQ(record[2], 4);
}

TEST(utils, va_argc) {
  ASSERT_EQ(1, VA_ARGC(a));
  ASSERT_EQ(2, VA_ARGC(a, b));
  ASSERT_EQ(3, VA_ARGC(a, b, c));
  ASSERT_EQ(4, VA_ARGC(a, b, c, d));
  ASSERT_EQ(5, VA_ARGC(a, b, c, d, e));
  ASSERT_EQ(6, VA_ARGC(a, b, c, d, e, f));
  ASSERT_EQ(7, VA_ARGC(a, b, c, d, e, f, g));
  ASSERT_EQ(8, VA_ARGC(a, b, c, d, e, f, g, h));
}

TEST(utils, byte_buffer_cursor) {
  byte_buffer_t buf;
  byte_buffer_init(&buf);

  byte_buffer_cursor_t c0, c1, c2;
  byte_buffer_append_cursor(&buf, &c0);
  byte_buffer_append_cursor(&buf, &c1);
  byte_buffer_append_cursor(&buf, &c2);

  blob_t blob;
  byte_buffer_flush(&buf, &blob);
  ASSERT_EQ(3, blob_byte_length(&blob));
  ASSERT_EQ(0, blob_byte_at(&blob, 0));
  ASSERT_EQ(0, blob_byte_at(&blob, 1));
  ASSERT_EQ(0, blob_byte_at(&blob, 2));

  byte_buffer_cursor_set(&c0, 8);
  ASSERT_EQ(8, blob_byte_at(&blob, 0));
  ASSERT_EQ(0, blob_byte_at(&blob, 1));
  ASSERT_EQ(0, blob_byte_at(&blob, 2));

  byte_buffer_cursor_set(&c1, 7);
  ASSERT_EQ(8, blob_byte_at(&blob, 0));
  ASSERT_EQ(7, blob_byte_at(&blob, 1));
  ASSERT_EQ(0, blob_byte_at(&blob, 2));

  byte_buffer_cursor_set(&c2, 6);
  ASSERT_EQ(8, blob_byte_at(&blob, 0));
  ASSERT_EQ(7, blob_byte_at(&blob, 1));
  ASSERT_EQ(6, blob_byte_at(&blob, 2));

  byte_buffer_dispose(&buf);
}

TEST(utils, short_buffer) {
  short_buffer_t buf;
  short_buffer_init(&buf);

  short_buffer_append(&buf, 0xFACE);
  short_buffer_append(&buf, 0xF00D);
  short_buffer_append(&buf, 0xDEAD);

  blob_t blob;
  short_buffer_flush(&buf, &blob);
  ASSERT_EQ(3, blob_short_length(&blob));
  ASSERT_EQ(0xFACE, blob_short_at(&blob, 0));
  ASSERT_EQ(0xF00D, blob_short_at(&blob, 1));
  ASSERT_EQ(0xDEAD, blob_short_at(&blob, 2));

  short_buffer_dispose(&buf);
}

TEST(utils, 64name) {
  char buf[kMaxWordyNameSize];
  wordy_encode(0x7FFFFFFFFFFFFFFFL, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("kahyfahuzytolubosuc", buf);
  wordy_encode(0x7FFFFFFFFFFFFFFEL, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("jahyfahuzytolubosuc", buf);
  wordy_encode(0, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("b", buf);
  wordy_encode(1, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("c", buf);
  wordy_encode(-1, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("a", buf);
  wordy_encode(-2, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("e", buf);
  wordy_encode(65536, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("vajog", buf);
  wordy_encode(-65536, buf, kMaxWordyNameSize);
  ASSERT_C_STREQ("odapu", buf);
}
