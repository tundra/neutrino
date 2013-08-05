// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "test.h"
#include "process.h"
#include "runtime.h"

TEST(process, frame_bounds) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack_piece = new_heap_stack_piece(&runtime, 1024, runtime_null(&runtime));

  // Check that push/pop outside the frame boundaries causes a check failure.
  frame_t frame;
  ASSERT_TRUE(try_push_frame(&frame, stack_piece, 4));
  ASSERT_CHECK_FAILURE(scOutOfBounds, frame_pop_value(&frame));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(6)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(5)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(4)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(3)));
  ASSERT_CHECK_FAILURE(scOutOfBounds, frame_push_value(&frame, new_integer(2)));
  ASSERT_VALEQ(new_integer(3), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(4), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(5), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(6), frame_pop_value(&frame));
  ASSERT_CHECK_FAILURE(scOutOfBounds, frame_pop_value(&frame));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(0)));

  // Mutating a frame that's below the top causes a check failure.
  frame_t inner;
  ASSERT_TRUE(try_push_frame(&inner, stack_piece, 4));
  ASSERT_CHECK_FAILURE(scWat, frame_push_value(&frame, new_integer(1)));
  ASSERT_CHECK_FAILURE(scWat, frame_pop_value(&frame));
  ASSERT_TRUE(try_pop_frame(&inner));
  ASSERT_VALEQ(new_integer(0), frame_pop_value(&frame));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(process, simple_frames) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack_piece = new_heap_stack_piece(&runtime, 1024, runtime_null(&runtime));
  frame_t frame;
  for (int i = 0; i < 256; i++) {
    if (i % 16 == 0)
      ASSERT_TRUE(try_push_frame(&frame, stack_piece, 16));
    frame_push_value(&frame, new_integer(i));
  }
  for (int i = 255; i >= 0; i--) {
    value_t expected = new_integer(i);
    value_t found = frame_pop_value(&frame);
    ASSERT_VALEQ(expected, found);
    if (i % 16 == 0)
      ASSERT_TRUE(try_pop_frame(&frame));
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
