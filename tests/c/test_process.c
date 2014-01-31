// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "test.h"
#include "process.h"
#include "runtime.h"

TEST(process, frame_bounds) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing());

  // Check that push/pop outside the frame boundaries causes a check failure.
  frame_t frame;
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 4, false));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_pop_value(&frame));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(6)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(5)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(4)));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(3)));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_push_value(&frame, new_integer(2)));
  ASSERT_VALEQ(new_integer(3), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(4), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(5), frame_pop_value(&frame));
  ASSERT_VALEQ(new_integer(6), frame_pop_value(&frame));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_pop_value(&frame));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(0)));

  // Mutating a frame that's below the top causes a check failure.
  frame_t inner;
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &inner, 4, false));
  ASSERT_CHECK_FAILURE(ccWat, frame_push_value(&frame, new_integer(1)));
  ASSERT_CHECK_FAILURE(ccWat, frame_pop_value(&frame));

  // Popping down to the frame makes value popping work again.
  ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &inner));
  ASSERT_VALEQ(new_integer(0), frame_pop_value(&frame));

  DISPOSE_RUNTIME();
}

TEST(process, simple_frames) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing());
  frame_t frame;
  for (int i = 0; i < 256; i++) {
    if (i % 16 == 0)
      ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 16, false));
    frame_push_value(&frame, new_integer(i));
  }
  for (int i = 255; i >= 0; i--) {
    value_t expected = new_integer(i);
    value_t found = frame_pop_value(&frame);
    ASSERT_VALEQ(expected, found);
    if (i % 16 == 0)
      ASSERT_EQ(i != 0, pop_stack_piece_frame(stack_piece, &frame));
  }

  DISPOSE_RUNTIME();
}

TEST(process, frame_capacity) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing());
  for (int i = 0; i < 16; i++) {
    frame_t frame;
    ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, i, false));
    ASSERT_EQ((size_t) i, frame.capacity);
  }

  for (int i = 14; i >= 0; i--) {
    frame_t frame;
    ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &frame));
    ASSERT_EQ((size_t) i, frame.capacity);
  }
  frame_t frame;
  ASSERT_FALSE(pop_stack_piece_frame(stack_piece, &frame));

  DISPOSE_RUNTIME();
}

TEST(process, bottom_frame) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing());
  frame_t frame;
  // Push two frames onto the stack piece.
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 10, false));
  ASSERT_TRUE(try_push_stack_piece_frame(stack_piece, &frame, 10, false));
  // Popping the first one succeeds since there's a second one below to pop to.
  ASSERT_TRUE(pop_stack_piece_frame(stack_piece, &frame));
  // Popping the second fails since there is no frame below to pop to.
  ASSERT_FALSE(pop_stack_piece_frame(stack_piece, &frame));

  DISPOSE_RUNTIME();
}

TEST(process, stack_frames) {
  CREATE_RUNTIME();

  value_t stack = new_heap_stack(runtime, 16);
  for (size_t i = 0; i < 256; i++) {
    frame_t frame;
    ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, i + 1, ROOT(runtime, empty_array)));
    frame_push_value(&frame, new_integer(i * 3));
  }

  for (int i = 255; i > 0; i--) {
    frame_t frame;
    get_stack_top_frame(stack, &frame);
    ASSERT_EQ((size_t) i + 1, frame.capacity);
    value_t value = frame_pop_value(&frame);
    ASSERT_EQ(i * 3, get_integer_value(value));
    ASSERT_TRUE(pop_organic_stack_frame(stack, &frame));
  }
  frame_t frame;
  // Popping the synthetic stack bottom frame should succeed.
  ASSERT_TRUE(pop_stack_frame(stack, &frame));
  // Now the stack is empty so it should not be possible to pop.
  ASSERT_FALSE(pop_stack_frame(stack, &frame));

  DISPOSE_RUNTIME();
}

TEST(process, get_argument_one_piece) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t stack = new_heap_stack(runtime, 3 + 3 * kFrameHeaderSize);
  frame_t frame;
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  frame_push_value(&frame, new_integer(6));
  frame_push_value(&frame, new_integer(5));
  frame_push_value(&frame, new_integer(4));
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 0, null()));
  set_frame_argument_map(&frame,
      C(vArray(vInt(0), vInt(1), vInt(2))));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 2));
  set_frame_argument_map(&frame,
      C(vArray(vInt(2), vInt(1), vInt(0))));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 2));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(process, get_argument_multi_pieces) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t stack = new_heap_stack(runtime, 16);
  frame_t frame;
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  frame_push_value(&frame, new_integer(6));
  frame_push_value(&frame, new_integer(5));
  frame_push_value(&frame, new_integer(4));
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 13,
      C(vArray(vInt(0), vInt(1), vInt(2)))));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 2));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(process, get_local) {
  CREATE_RUNTIME();

  value_t stack = new_heap_stack(runtime, 16);
  frame_t frame;
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(6)));
  ASSERT_VALEQ(new_integer(6), frame_get_local(&frame, 0));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_get_local(&frame, 1));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_get_local(&frame, 2));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(5)));
  ASSERT_VALEQ(new_integer(6), frame_get_local(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_local(&frame, 1));
  ASSERT_CHECK_FAILURE(ccOutOfBounds, frame_get_local(&frame, 2));
  ASSERT_SUCCESS(frame_push_value(&frame, new_integer(4)));
  ASSERT_VALEQ(new_integer(6), frame_get_local(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_local(&frame, 1));
  ASSERT_VALEQ(new_integer(4), frame_get_local(&frame, 2));

  DISPOSE_RUNTIME();
}
