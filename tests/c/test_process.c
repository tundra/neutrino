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
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 4));
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
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &inner, 4));
  ASSERT_CHECK_FAILURE(scWat, frame_push_value(&frame, new_integer(1)));
  ASSERT_CHECK_FAILURE(scWat, frame_pop_value(&frame));

  // Popping down to the frame makes value popping work again.
  ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &inner));
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
      ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 16));
    frame_push_value(&frame, new_integer(i));
  }
  for (int i = 255; i >= 0; i--) {
    value_t expected = new_integer(i);
    value_t found = frame_pop_value(&frame);
    ASSERT_VALEQ(expected, found);
    if (i % 16 == 0)
      ASSERT_EQ(i != 0, pop_stack_piece_frame(stack_piece, &frame));
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(process, frame_capacity) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack_piece = new_heap_stack_piece(&runtime, 1024, runtime_null(&runtime));
  for (int i = 0; i < 16; i++) {
    frame_t frame;
    ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, i));
    ASSERT_EQ((size_t) i, frame.capacity);
  }

  for (int i = 14; i >= 0; i--) {
    frame_t frame;
    ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &frame));
    ASSERT_EQ((size_t) i, frame.capacity);
  }
  frame_t frame;
  ASSERT_FALSE(pop_stack_piece_frame(stack_piece, &frame));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(process, bottom_frame) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack_piece = new_heap_stack_piece(&runtime, 1024, runtime_null(&runtime));
  frame_t frame;
  // Push two frames onto the stack piece.
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 10));
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 10));
  // Popping the first one succeeds since there's a second one below to pop to.
  ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &frame));
  // Popping the second fails since there is no frame below to pop to.
  ASSERT_FALSE(pop_stack_piece_frame(stack_piece, &frame));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(process, stack_frames) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack = new_heap_stack(&runtime, 16);
  for (size_t i = 0; i < 256; i++) {
    frame_t frame;
    ASSERT_SUCCESS(push_stack_frame(&runtime, stack, &frame, i + 1));
    frame_push_value(&frame, new_integer(i * 3));
  }

  for (int i = 255; i > 0; i--) {
    frame_t frame;
    get_stack_top_frame(stack, &frame);
    ASSERT_EQ((size_t) i + 1, frame.capacity);
    value_t value = frame_pop_value(&frame);
    ASSERT_EQ(i * 3, get_integer_value(value));
    ASSERT_TRUE(pop_stack_frame(stack, &frame));
  }
  frame_t frame;
  ASSERT_FALSE(pop_stack_frame(stack, &frame));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
