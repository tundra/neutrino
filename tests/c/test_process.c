// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "test.h"
#include "process.h"
#include "runtime.h"

TEST(process, frame_bounds) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing(), nothing());

  // Check that push/pop outside the frame boundaries causes a check failure.
  frame_t frame = frame_empty();
  open_stack_piece(stack_piece, &frame);
  ASSERT_TRUE(try_push_new_frame(&frame, 4, ffOrganic, false));
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
  close_frame(&frame);

  DISPOSE_RUNTIME();
}

TEST(process, simple_frames) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing(), nothing());
  frame_t frame = frame_empty();
  open_stack_piece(stack_piece, &frame);

  for (int i = 0; i < 256; i++) {
    if (i % 16 == 0)
      ASSERT_TRUE(try_push_new_frame(&frame, 16, ffOrganic, false));
    frame_push_value(&frame, new_integer(i));
  }
  for (int i = 255; i >= 0; i--) {
    value_t expected = new_integer(i);
    value_t found = frame_pop_value(&frame);
    ASSERT_VALEQ(expected, found);
    if (i % 16 == 0) {
      frame_pop_within_stack_piece(&frame);
      ASSERT_EQ(i == 0, frame_has_flag(&frame, ffStackPieceEmpty));
    }
  }

  close_frame(&frame);

  DISPOSE_RUNTIME();
}

TEST(process, frame_capacity) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing(), nothing());
  frame_t frame = frame_empty();
  open_stack_piece(stack_piece, &frame);
  for (int i = 0; i < 16; i++) {
    ASSERT_TRUE(try_push_new_frame(&frame, i, ffOrganic, false));
    ASSERT_PTREQ(frame.frame_pointer + i, frame.limit_pointer);
  }

  for (int i = 14; i >= 0; i--) {
    frame_pop_within_stack_piece(&frame);
    ASSERT_FALSE(frame_has_flag(&frame, ffStackPieceEmpty));
    ASSERT_PTREQ(frame.frame_pointer + i, frame.limit_pointer);
  }
  frame_pop_within_stack_piece(&frame);
  ASSERT_TRUE(frame_has_flag(&frame, ffStackPieceEmpty));
  close_frame(&frame);

  DISPOSE_RUNTIME();
}

TEST(process, bottom_frame) {
  CREATE_RUNTIME();

  value_t stack_piece = new_heap_stack_piece(runtime, 1024, nothing(), nothing());
  frame_t frame = frame_empty();
  // Push two frames onto the stack piece.
  open_stack_piece(stack_piece, &frame);
  ASSERT_TRUE(try_push_new_frame(&frame, 10, ffOrganic, false));
  ASSERT_TRUE(try_push_new_frame(&frame, 10, ffOrganic, false));
  frame_pop_within_stack_piece(&frame);
  ASSERT_FALSE(frame_has_flag(&frame, ffStackPieceEmpty));
  frame_pop_within_stack_piece(&frame);
  ASSERT_TRUE(frame_has_flag(&frame, ffStackPieceEmpty));

  DISPOSE_RUNTIME();
}

// Pops frames off the given stack until one is reached that has one of the
// given flags set. There must be such a frame on the stack. Note that this
// ignores barriers so the resulting stack may be some form of invalid wrt.
// barriers.
static void drop_to_stack_frame(value_t stack, frame_t *frame, frame_flag_t flags) {
  value_t piece = get_stack_top_piece(stack);
  frame_loop: while (true) {
    CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
    frame_walk_down_stack(frame);
    if (frame_has_flag(frame, ffStackPieceEmpty)) {
      // If we're at the bottom of a stack piece walk down another frame to
      // get to the next one.
      piece = get_stack_piece_previous(piece);
      CHECK_FALSE("bottom of stack", is_nothing(piece));
      set_stack_top_piece(stack, piece);
      open_stack_piece(piece, frame);
      CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
    }
    if (!frame_has_flag(frame, flags)) {
      goto frame_loop;
    } else {
      return;
    }
  }
}

TEST(process, stack_frames) {
  CREATE_RUNTIME();

  value_t stack = new_heap_stack(runtime, 24);
  frame_t frame = open_stack(stack);
  for (size_t i = 0; i < 256; i++) {
    ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, i + 1, ROOT(runtime, empty_array)));
    frame_push_value(&frame, new_integer(i * 3));
  }

  for (int i = 255; i > 0; i--) {
    ASSERT_PTREQ(frame.frame_pointer + i + 1, frame.limit_pointer);
    value_t value = frame_pop_value(&frame);
    ASSERT_EQ(i * 3, get_integer_value(value));
    drop_to_stack_frame(stack, &frame, ffOrganic);
  }
  // Popping the synthetic stack bottom frame should succeed.
  drop_to_stack_frame(stack, &frame, ffSynthetic);
  // Finally we should be at the very bottom.
  ASSERT_TRUE(frame_has_flag(&frame, ffStackBottom));
  close_frame(&frame);

  DISPOSE_RUNTIME();
}

TEST(process, walk_stack_frames) {
  CREATE_RUNTIME();

  value_t stack = new_heap_stack(runtime, 16);
  frame_t frame = open_stack(stack);

  for (size_t i = 0; i < 64; i++) {
    ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 1,
        ROOT(runtime, empty_array)));
    ASSERT_SUCCESS(frame_push_value(&frame, new_integer(i + 5)));
    frame_iter_t iter;
    frame_iter_init_from_frame(&iter, &frame);
    for (size_t j = 0; j <= i; j++) {
      size_t frame_i = i - j;
      frame_t *current = frame_iter_get_current(&iter);
      ASSERT_VALEQ(new_integer(frame_i + 5), frame_peek_value(current, 0));
      if (j < i)
        ASSERT_TRUE(frame_iter_advance(&iter));
    }
    ASSERT_FALSE(frame_iter_advance(&iter));
  }

  close_frame(&frame);

  DISPOSE_RUNTIME();
}

TEST(process, get_argument_one_piece) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t stack = new_heap_stack(runtime, 3 + 3 * kFrameHeaderSize + kStackBarrierSize);
  frame_t frame = open_stack(stack);

  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  frame_push_value(&frame, new_integer(6));
  frame_push_value(&frame, new_integer(5));
  frame_push_value(&frame, new_integer(4));
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 0, null()));
  frame_set_argument_map(&frame,
      C(vArray(vInt(0), vInt(1), vInt(2))));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 2));
  frame_set_argument_map(&frame,
      C(vArray(vInt(2), vInt(1), vInt(0))));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 2));

  close_frame(&frame);

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(process, get_argument_multi_pieces) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t stack = new_heap_stack(runtime, 20);
  frame_t frame = open_stack(stack);

  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  frame_push_value(&frame, new_integer(6));
  frame_push_value(&frame, new_integer(5));
  frame_push_value(&frame, new_integer(4));
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 13,
      C(vArray(vInt(0), vInt(1), vInt(2)))));
  ASSERT_VALEQ(new_integer(4), frame_get_argument(&frame, 0));
  ASSERT_VALEQ(new_integer(5), frame_get_argument(&frame, 1));
  ASSERT_VALEQ(new_integer(6), frame_get_argument(&frame, 2));

  close_frame(&frame);

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(process, get_local) {
  CREATE_RUNTIME();

  value_t stack = new_heap_stack(runtime, 20);
  frame_t frame = open_stack(stack);

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

  close_frame(&frame);

  DISPOSE_RUNTIME();
}

static void assert_invocation_format(const char *expected_c_str, value_t invocation) {
  // Print the invocation on a temporary buffer.
  print_on_context_t context;
  string_buffer_t buffer;
  string_buffer_init(&buffer);
  print_on_context_init(&context, &buffer, pfNone, 99);
  backtrace_entry_invocation_print_on(invocation, ocInvoke, &context);
  // Flush the output and the expected values into string_ts.
  string_t found;
  string_buffer_flush(&buffer, &found);
  string_t expected_str;
  string_init(&expected_str, expected_c_str);
  ASSERT_STREQ(&expected_str, &found);
  string_buffer_dispose(&buffer);
}

TEST(process, backtrace_entry_printing) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *subject = vValue(ROOT(runtime, subject_key));
  variant_t *selector = vValue(ROOT(runtime, selector_key));

  assert_invocation_format("10", C(vMap(
      subject, vInt(10))));
  assert_invocation_format("11.foo()", C(vMap(
      subject, vInt(11),
      selector, vInfix("foo"))));
  assert_invocation_format(".fxx()", C(vMap(
      selector, vInfix("fxx"))));
  assert_invocation_format("12.bar(\"blah\")", C(vMap(
      subject, vInt(12),
      selector, vInfix("bar"),
      vInt(0), vStr("blah"))));
  assert_invocation_format("13.baz(\"blah\", \"blob\")", C(vMap(
      subject, vInt(13),
      selector, vInfix("baz"),
      vInt(0), vStr("blah"),
      vInt(1), vStr("blob"))));
  assert_invocation_format("13[0]", C(vMap(
      subject, vInt(13),
      selector, vIndex(),
      vInt(0), vInt(0))));
  assert_invocation_format("14.quux(\"blah\", 2: \"blob\")", C(vMap(
      subject, vInt(14),
      selector, vInfix("quux"),
      vInt(0), vStr("blah"),
      vInt(2), vStr("blob"))));
  assert_invocation_format("16.quux(a: \"blob\")", C(vMap(
      subject, vInt(16),
      selector, vInfix("quux"),
      vStr("a"), vStr("blob"))));
  assert_invocation_format("17[row: 8]", C(vMap(
      subject, vInt(17),
      selector, vIndex(),
      vStr("row"), vInt(8))));
  assert_invocation_format("18.quux(-1: \"blob\")", C(vMap(
      subject, vInt(18),
      selector, vInfix("quux"),
      vInt(-1), vStr("blob"))));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

