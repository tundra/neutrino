// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "process.h"
#include "value-inl.h"


// --- S t a c k   p i e c e ---

CHECKED_ACCESSORS_IMPL(StackPiece, stack_piece, Array, Storage, storage);
UNCHECKED_ACCESSORS_IMPL(StackPiece, stack_piece, Previous, previous);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopFramePointer, top_frame_pointer);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopStackPointer, top_stack_pointer);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopCapacity, top_capacity);

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

CHECKED_ACCESSORS_IMPL(Stack, stack, StackPiece, TopPiece, top_piece);
INTEGER_ACCESSORS_IMPL(Stack, stack, DefaultPieceCapacity, default_piece_capacity);

value_t stack_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofStack, value);
  VALIDATE_VALUE_FAMILY(ofStackPiece, get_stack_top_piece(value));
  return success();
}

void stack_print_on(value_t value, string_buffer_t *buf) {
  stack_print_atomic_on(value, buf);
}

void stack_print_atomic_on(value_t value, string_buffer_t *buf) {
  CHECK_FAMILY(ofStack, value);
  string_buffer_printf(buf, "#<stack>");
}

value_t push_stack_frame(runtime_t *runtime, value_t stack, frame_t *frame,
    size_t frame_capacity) {
  CHECK_FAMILY(ofStack, stack);
  value_t top_piece = get_stack_top_piece(stack);
  if (!try_push_stack_piece_frame(top_piece, frame, frame_capacity)) {
    // There wasn't room to push this frame onto the top stack piece so
    // allocate a new top piece that definitely has room.
    size_t default_capacity = get_stack_default_piece_capacity(stack);
    size_t required_capacity = frame_capacity + kFrameHeaderSize;
    size_t new_capacity = max_size(default_capacity, required_capacity);
    TRY_DEF(new_piece, new_heap_stack_piece(runtime, new_capacity, top_piece));
    set_stack_top_piece(stack, new_piece);
    bool pushed_stack_piece = try_push_stack_piece_frame(new_piece,
        frame, frame_capacity);
    CHECK_TRUE("pushing on new piece failed", pushed_stack_piece);
  }
  return success();
}

bool pop_stack_frame(value_t stack, frame_t *frame) {
  value_t top_piece = get_stack_top_piece(stack);
  if (pop_stack_piece_frame(top_piece, frame)) {
    // There was a frame to pop on the top piece so we're good.
    return true;
  }
  // The piece was empty so we have to pop down to the previous piece.
  value_t next_piece = get_stack_piece_previous(top_piece);
  if (is_null(next_piece)) {
    // There are no more pieces. Leave the empty one in place and report that
    // popping failed.
    return false;
  } else {
    // There are more pieces. Update the stack and fetch the top frame (which
    // we know will be there).
    set_stack_top_piece(stack, next_piece);
    get_stack_top_frame(stack, frame);
    return true;
  }
}

// Sets the given frame to be the top frame of the given stack piece. If the
// stack piece has no frames the frame is set to all zeroes.
static void get_top_stack_piece_frame(value_t stack_piece, frame_t *frame) {
  frame->stack_piece = stack_piece;
  frame->frame_pointer = get_stack_piece_top_frame_pointer(stack_piece);
  frame->stack_pointer = get_stack_piece_top_stack_pointer(stack_piece);
  frame->capacity = get_stack_piece_top_capacity(stack_piece);
}

void get_stack_top_frame(value_t stack, frame_t *frame) {
  CHECK_FAMILY(ofStack, stack);
  value_t top_piece = get_stack_top_piece(stack);
  get_top_stack_piece_frame(top_piece, frame);
}


// --- F r a m e ---

bool try_push_stack_piece_frame(value_t stack_piece, frame_t *frame, size_t frame_capacity) {
  // First record the current state of the old top frame so we can store it in
  // the header if the new frame.
  frame_t old_frame;
  get_top_stack_piece_frame(stack_piece, &old_frame);
  // The amount of space required to make this frame.
  size_t size = frame_capacity + kFrameHeaderSize;
  // Determine how much room is left in the stack piece.
  value_t storage = get_stack_piece_storage(stack_piece);
  size_t stack_piece_capacity = get_array_length(storage);
  size_t available = stack_piece_capacity - old_frame.stack_pointer;
  if (available < size)
    return false;
  // Points to the first field in this frame's header.
  size_t new_frame_pointer = old_frame.stack_pointer + kFrameHeaderSize;
  // Store the new frame's info in the frame struct.
  frame->stack_pointer = frame->frame_pointer = new_frame_pointer;
  frame->capacity = frame_capacity;
  frame->stack_piece = stack_piece;
  // Record the relevant information about the previous frame in the new frame's
  // header.
  set_frame_previous_frame_pointer(frame, old_frame.frame_pointer);
  set_frame_previous_capacity(frame, old_frame.capacity);
  // Update the stack piece's top frame data to reflect the new top frame.
  set_stack_piece_top_stack_pointer(stack_piece, frame->stack_pointer);
  set_stack_piece_top_frame_pointer(stack_piece, frame->frame_pointer);
  set_stack_piece_top_capacity(stack_piece, frame->capacity);
  return true;
}

bool pop_stack_piece_frame(value_t stack_piece, frame_t *frame) {
  // Grab the current top frame.
  frame_t top_frame;
  get_top_stack_piece_frame(stack_piece, &top_frame);
  CHECK_TRUE("empty stack piece", top_frame.frame_pointer > 0);
  // Get the frame pointer and capacity from the top frame's header.
  frame->frame_pointer = get_frame_previous_frame_pointer(&top_frame);
  frame->capacity = get_frame_previous_capacity(&top_frame);
  // The stack pointer will be the first field of the top frame's header.
  frame->stack_pointer = top_frame.frame_pointer - kFrameHeaderSize;
  set_stack_piece_top_stack_pointer(stack_piece, frame->stack_pointer);
  set_stack_piece_top_frame_pointer(stack_piece, frame->frame_pointer);
  set_stack_piece_top_capacity(stack_piece, frame->capacity);
  return (frame->frame_pointer != 0);
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

// Returns true if the given absolute offset is within the fields available to
// the given frame.
static bool is_offset_within_frame(frame_t *frame, size_t offset) {
  return (offset - frame->frame_pointer) < frame->capacity;
}

// Returns true iff the given frame is the top stack frame on its stack piece.
static bool is_top_frame(frame_t *frame) {
  return frame->stack_pointer == get_stack_piece_top_stack_pointer(frame->stack_piece);
}

// Accesses a frame field, that is, a local to the current call. The offset is
// relative to the whole stack piece, not the frame.
static value_t *access_frame_field(frame_t *frame, size_t offset) {
  CHECK_TRUE("frame field out of bounds", is_offset_within_frame(frame, offset));
  value_t storage = get_stack_piece_storage(frame->stack_piece);
  CHECK_TRUE("frame field out of bounds", offset < get_array_length(storage));
  return get_array_elements(storage) + offset;
}

void set_frame_previous_frame_pointer(frame_t *frame, size_t value) {
  *access_frame_header_field(frame, kFrameHeaderPreviousFramePointerOffset) =
      new_integer(value);
}

size_t get_frame_previous_frame_pointer(frame_t *frame) {
  return get_integer_value(*access_frame_header_field(frame,
      kFrameHeaderPreviousFramePointerOffset));
}

void set_frame_previous_capacity(frame_t *frame, size_t value) {
  *access_frame_header_field(frame, kFrameHeaderPreviousCapacityOffset) =
      new_integer(value);
}

size_t get_frame_previous_capacity(frame_t *frame) {
  return get_integer_value(*access_frame_header_field(frame,
      kFrameHeaderPreviousCapacityOffset));
}

void set_frame_code_block(frame_t *frame, value_t code_block) {
  *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset) =
      code_block;
}

value_t get_frame_code_block(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset);
}

void set_frame_pc(frame_t *frame, size_t pc) {
  *access_frame_header_field(frame, kFrameHeaderPcOffset) =
      new_integer(pc);
}

size_t get_frame_pc(frame_t *frame) {
  return get_integer_value(*access_frame_header_field(frame,
      kFrameHeaderPcOffset));
}


value_t frame_push_value(frame_t *frame, value_t value) {
  // Check that the stack is in sync with this frame.
  SIG_CHECK_TRUE("push on lower frame", scWat, is_top_frame(frame));
  SIG_CHECK_TRUE("push out of frame bounds", scOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer));
  *access_frame_field(frame, frame->stack_pointer) = value;
  frame->stack_pointer++;
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  return success();
}

value_t frame_pop_value(frame_t *frame) {
  SIG_CHECK_TRUE("pop off lower frame", scWat, is_top_frame(frame));
  SIG_CHECK_TRUE("pop out of frame bounds", scOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer - 1));
  frame->stack_pointer--;
  value_t result = *access_frame_field(frame, frame->stack_pointer);
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  return result;
}

value_t frame_peek_value(frame_t *frame, size_t index) {
  return *access_frame_field(frame, frame->stack_pointer - index - 1);
}
