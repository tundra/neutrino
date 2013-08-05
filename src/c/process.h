// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Objects and functionality related to processes and execution.

#include "value.h"

#ifndef _PROCESS
#define _PROCESS


// --- S t a c k   p i e c e ---

static const size_t kStackPieceSize = OBJECT_SIZE(4);
static const size_t kStackPieceStorageOffset = 1;
static const size_t kStackPiecePreviousOffset = 2;
static const size_t kStackPieceTopFramePointerOffset = 3;
static const size_t kStackPieceTopStackPointerOffset = 4;

// Returns the storage array for this stack segment.
value_t get_stack_piece_storage(value_t value);

// Sets the storage array for this stack segment.
void set_stack_piece_storage(value_t value, value_t storage);

// Returns the previous piece for this stack.
value_t get_stack_piece_previous(value_t value);

// Sets the previous piece for this stack.
void set_stack_piece_previous(value_t value, value_t previous);

// Returns the offset of the top stack segment element (0 if the stack segment
// is empty).
size_t get_stack_piece_top_frame_pointer(value_t value);

// Sets the offset of the top stack segment element.
void set_stack_piece_top_frame_pointer(value_t value, size_t top);

// Returns the offset of the top stack segment element (0 if the stack segment
// is empty).
size_t get_stack_piece_top_stack_pointer(value_t value);

// Sets the offset of the top stack segment element.
void set_stack_piece_top_stack_pointer(value_t value, size_t top);


// --- F r a m e ---

// A transient stack frame. The current structure isn't clever but it's not
// intended to be, it's intended to work and be fully general.
typedef struct {
  // Pointer to the next available field on the stack.
  size_t stack_pointer;
  // Pointer to the bottom of the stack fields.
  size_t frame_pointer;
  // The frame capacity available.
  size_t capacity;
  // The stack piece that contains this frame.
  value_t stack_piece;
} frame_t;

// The number of words in a stack frame header.
static const size_t kFrameHeaderSize = 1;

// Offsets _down_ from the frame pointer to the header fields.
static const size_t kFrameHeaderPreviousFramePointerOffset = 0;

// Tries to allocate a new frame on the given stack piece of the given capacity.
// Returns true iff allocation succeeds.
bool try_push_frame(frame_t *frame, value_t stack_piece, size_t capacity);

bool try_pop_frame(frame_t *frame);

// Record the frame pointer for the previous stack frame, the one below this one.
void set_frame_previous_frame_pointer(frame_t *frame, size_t value);

// Returns the frame pointer for the previous stack frame, the one below this
// one.
size_t get_frame_previous_frame_pointer(frame_t *frame);

// Pushes a value onto this stack frame. The returned value will always be
// success except on bounds check failures in soft check failure mode where it
// will be OutOfBounds.
value_t frame_push_value(frame_t *frame, value_t value);

// Pops a value off this stack frame. Bounds checks whether there is a value to
// pop and in soft check failure mode returns an OutOfBounds signal if not.
value_t frame_pop_value(frame_t *frame);


// --- S t a c k ---

static const size_t kStackSize = OBJECT_SIZE(1);
static const size_t kStackTopOffset = 1;

// Returns the top piece of this stack.
value_t get_stack_top(value_t value);

// Sets the top piece of this stack.
void set_stack_top(value_t value, value_t piece);

#endif // _PROCESS
