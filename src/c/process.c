// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "process.h"
#include "value-inl.h"


// --- S t a c k   p i e c e ---

OBJECT_IDENTITY_IMPL(stack_piece);
CANT_SET_CONTENTS(stack_piece);
FIXED_SIZE_PURE_VALUE_IMPL(stack_piece, StackPiece);

CHECKED_GETTER_SETTER_IMPL(StackPiece, stack_piece, Array, Storage, storage);
UNCHECKED_GETTER_SETTER_IMPL(StackPiece, stack_piece, Previous, previous);
INTEGER_GETTER_SETTER_IMPL(StackPiece, stack_piece, TopFramePointer, top_frame_pointer);
INTEGER_GETTER_SETTER_IMPL(StackPiece, stack_piece, TopStackPointer, top_stack_pointer);

value_t stack_piece_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofStackPiece, value);
  VALIDATE_VALUE_FAMILY(ofArray, get_stack_piece_storage(value));
  return success();
}

void stack_piece_print_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofStackPiece, value);
  string_buffer_printf(buf, "#<stack piece: st@%i>",
      get_blob_length(get_stack_piece_storage(value)));
}

void stack_piece_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofStackPiece, value);
  string_buffer_printf(buf, "#<stack piece>");
}


// --- S t a c k   ---

OBJECT_IDENTITY_IMPL(stack);
CANT_SET_CONTENTS(stack);
FIXED_SIZE_PURE_VALUE_IMPL(stack, Stack);

CHECKED_GETTER_SETTER_IMPL(Stack, stack, StackPiece, Top, top);

value_t stack_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofStack, value);
  VALIDATE_VALUE_FAMILY(ofStackPiece, get_stack_top(value));
  return success();
}

void stack_print_on(value_t value, string_buffer_t *buf) {
  stack_print_atomic_on(value, buf);
}

void stack_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofStack, value);
  string_buffer_printf(buf, "#<stack>");
}


// --- F r a m e ---

bool try_push_frame(frame_t *frame, value_t stack_piece, size_t frame_size) {
  // The amount of space required in this frame.
  size_t size = frame_size + kFrameHeaderSize;
  // Determine how much room is left in the stack piece.
  value_t storage = get_stack_piece_storage(stack_piece);
  size_t capacity = get_array_length(storage);
  size_t old_stack_pointer = get_stack_piece_top_stack_pointer(stack_piece);
  size_t old_frame_pointer = get_stack_piece_top_frame_pointer(stack_piece);
  size_t available = capacity - old_stack_pointer;
  if (available < size)
    return false;
  // Points to the first field in this frame's header.
  size_t new_frame_pointer = old_stack_pointer + kFrameHeaderSize;
  frame->stack_pointer = frame->frame_pointer = new_frame_pointer;
  set_stack_piece_top_stack_pointer(stack_piece, new_frame_pointer);
  set_stack_piece_top_frame_pointer(stack_piece, new_frame_pointer);
  frame->capacity = frame_size;
  frame->stack_piece = stack_piece;
  set_frame_previous_frame_pointer(frame, old_frame_pointer);
  return true;
}

bool try_pop_frame(frame_t *frame) {
  size_t next_frame_pointer = get_frame_previous_frame_pointer(frame);
  size_t next_stack_pointer = frame->frame_pointer - kFrameHeaderSize;
  frame->frame_pointer = next_frame_pointer;
  frame->stack_pointer = next_stack_pointer;
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  set_stack_piece_top_frame_pointer(frame->stack_piece, frame->frame_pointer);
  return true;
}

// Accesses a frame header field, that is, a bookkeeping field below the frame
// pointer.
static value_t *access_frame_header_field(frame_t *frame, size_t offset) {
  CHECK_TRUE("frame header field out of bounds", offset <= kFrameHeaderSize);
  value_t storage = get_stack_piece_storage(frame->stack_piece);
  size_t index = frame->frame_pointer - offset - 1;
  CHECK_TRUE("frame header out of bounds", index < get_array_length(storage));
  return get_array_elements(storage) + index;
}

// Accesses a frame field, that is, a local to the current call. The offset is
// relative to the whole stack piece, not the frame.
static value_t *access_frame_field(frame_t *frame, size_t offset) {
  CHECK_TRUE("frame field out of bounds", (offset - frame->frame_pointer) < frame->capacity);
  value_t storage = get_stack_piece_storage(frame->stack_piece);
  CHECK_TRUE("frame field out of bounds", offset < get_array_length(storage));
  return get_array_elements(storage) + offset;
}

void set_frame_previous_frame_pointer(frame_t *frame, size_t value) {
  *access_frame_header_field(frame, kFrameHeaderPreviousFramePointerOffset) = new_integer(value);
}

size_t get_frame_previous_frame_pointer(frame_t *frame) {
  return get_integer_value(*access_frame_header_field(frame, kFrameHeaderPreviousFramePointerOffset));
}

void frame_push_value(frame_t *frame, value_t value) {
  // Check that the stack is in sync with this frame.
  CHECK_EQ("disconnected frame", frame->stack_pointer,
      get_stack_piece_top_stack_pointer(frame->stack_piece));
  *access_frame_field(frame, frame->stack_pointer) = value;
  frame->stack_pointer++;
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
}

value_t frame_pop_value(frame_t *frame) {
  value_t result = *access_frame_field(frame, frame->stack_pointer - 1);
  frame->stack_pointer--;
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  return result;
}
