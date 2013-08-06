// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Objects and functionality related to processes and execution.

#include "value-inl.h"

#ifndef _PROCESS
#define _PROCESS


// --- S t a c k   p i e c e ---

static const size_t kStackPieceSize = OBJECT_SIZE(5);
static const size_t kStackPieceStorageOffset = 1;
static const size_t kStackPiecePreviousOffset = 2;
static const size_t kStackPieceTopFramePointerOffset = 3;
static const size_t kStackPieceTopStackPointerOffset = 4;
static const size_t kStackPieceTopCapacityOffset = 5;

// The plain array used for storage for this stack piece.
ACCESSORS_DECL(stack_piece, storage);

// The previous, lower, stack piece.
ACCESSORS_DECL(stack_piece, previous);

// The frame pointer for the top frame.
INTEGER_ACCESSORS_DECL(stack_piece, top_frame_pointer);

// The current stack pointer for the top frame.
INTEGER_ACCESSORS_DECL(stack_piece, top_stack_pointer);

// The capacity for the top frame.
INTEGER_ACCESSORS_DECL(stack_piece, top_capacity);


// --- F r a m e ---

// A transient stack frame. The current structure isn't clever but it's not
// intended to be, it's intended to work and be fully general.
typedef struct {
  // Pointer to the next available field on the stack.
  size_t stack_pointer;
  // Pointer to the bottom of the stack fields.
  size_t frame_pointer;
  // The total frame capacity available.
  size_t capacity;
  // The stack piece that contains this frame.
  value_t stack_piece;
} frame_t;

// The number of words in a stack frame header.
static const size_t kFrameHeaderSize = 2;

// Offsets _down_ from the frame pointer to the header fields.
static const size_t kFrameHeaderPreviousFramePointerOffset = 0;
static const size_t kFrameHeaderPreviousCapacityOffset = 1;

// Tries to allocate a new frame on the given stack piece of the given capacity.
// Returns true iff allocation succeeds.
bool try_push_stack_piece_frame(value_t stack_piece, frame_t *frame, size_t capacity);

// Pops the top frame off the given stack piece, storing the next frame in the
// given frame struct. Returns true if there are more frames to pop off the stack,
// false if the one popped off was the last one. If false is returned the frame
// is invalid.
bool pop_stack_piece_frame(value_t stack_piece, frame_t *frame);

// Record the frame pointer for the previous stack frame, the one below this one.
void set_frame_previous_frame_pointer(frame_t *frame, size_t value);

// Returns the frame pointer for the previous stack frame, the one below this
// one.
size_t get_frame_previous_frame_pointer(frame_t *frame);

// Record the capacity of the previous stack frame.
void set_frame_previous_capacity(frame_t *frame, size_t capacity);

// Returns the capacity of the previous stack frame.
size_t get_frame_previous_capacity(frame_t *frame);

// Pushes a value onto this stack frame. The returned value will always be
// success except on bounds check failures in soft check failure mode where it
// will be OutOfBounds.
value_t frame_push_value(frame_t *frame, value_t value);

// Pops a value off this stack frame. Bounds checks whether there is a value to
// pop and in soft check failure mode returns an OutOfBounds signal if not.
value_t frame_pop_value(frame_t *frame);


// --- S t a c k ---

static const size_t kStackSize = OBJECT_SIZE(2);
static const size_t kStackTopPieceOffset = 1;
static const size_t kStackDefaultPieceCapacityOffset = 2;

// The top stack piece of this stack.
ACCESSORS_DECL(stack, top_piece);

// The default capacity of the stack pieces that make up this stack.
INTEGER_ACCESSORS_DECL(stack, default_piece_capacity);

// Allocates a new frame on this stack. If allocating fails, for instance if a
// new stack piece is required and we're out of memory, a signal is returned.
value_t push_stack_frame(runtime_t *runtime, value_t stack, frame_t *frame,
    size_t frame_capacity);

// Pops the top frame off the given stack and stores the next frame in the given
// frame struct.
bool pop_stack_frame(value_t stack, frame_t *frame);

// Reads the top frame off the given stack into the given frame.
void get_stack_top_frame(value_t stack, frame_t *frame);

#endif // _PROCESS
