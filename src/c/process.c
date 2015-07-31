//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "derived-inl.h"
#include "freeze.h"
#include "io.h"
#include "process.h"
#include "sync.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "utils/log.h"
#include "value-inl.h"

/// ## Stack piece

FIXED_GET_MODE_IMPL(stack_piece, vmMutable);

ACCESSORS_IMPL(StackPiece, stack_piece, snNoCheck, Capacity, capacity);
ACCESSORS_IMPL(StackPiece, stack_piece, snInFamilyOpt(ofStackPiece), Previous,
    previous);
ACCESSORS_IMPL(StackPiece, stack_piece, snInFamilyOpt(ofStack), Stack, stack);
ACCESSORS_IMPL(StackPiece, stack_piece, snNoCheck, LidFramePointer,
    lid_frame_pointer);

size_t calc_stack_piece_size(size_t capacity) {
  return kStackPieceHeaderSize + (capacity * kValueSize);
}

value_t *get_stack_piece_storage(value_t value) {
  CHECK_FAMILY(ofStackPiece, value);
  return access_heap_object_field(value, kStackPieceStorageOffset);
}

int set_stack_piece_storage(value_t self, value_t value) {
  UNREACHABLE("set_stack_piece_storage");
  return 0;
}

void get_stack_piece_layout(value_t value, heap_object_layout_t *layout) {
  size_t capacity = (size_t) get_integer_value(get_stack_piece_capacity(value));
  size_t size = calc_stack_piece_size(capacity);
  heap_object_layout_set(layout, size, kHeapObjectHeaderSize);
}

value_t stack_piece_validate(value_t value) {
  VALIDATE_FAMILY(ofStackPiece, value);
  VALIDATE_FAMILY_OPT(ofStackPiece, get_stack_piece_previous(value));
  VALIDATE_FAMILY_OPT(ofStack, get_stack_piece_stack(value));
  VALIDATE_DOMAIN(vdInteger, get_stack_piece_capacity(value));
  return success();
}

void stack_piece_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofStackPiece, value);
  string_buffer_printf(context->buf, "#<stack piece ~%w: st@%i>",
      canonicalize_value_for_print(value, context),
      get_stack_piece_capacity(value));
}

bool is_stack_piece_closed(value_t self) {
  return is_integer(get_stack_piece_lid_frame_pointer(self));
}

// --- S t a c k   ---

FIXED_GET_MODE_IMPL(stack, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Stack, stack);

ACCESSORS_IMPL(Stack, stack, snInFamily(ofStackPiece), TopPiece, top_piece);
INTEGER_ACCESSORS_IMPL(Stack, stack, DefaultPieceCapacity,
    default_piece_capacity);
ACCESSORS_IMPL(Stack, stack, snNoCheck, TopBarrier, top_barrier);

value_t stack_validate(value_t self) {
  VALIDATE_FAMILY(ofStack, self);
  VALIDATE_FAMILY(ofStackPiece, get_stack_top_piece(self));
  value_t current = get_stack_top_piece(self);
  while (!is_nothing(current)) {
    value_t stack = get_stack_piece_stack(current);
    VALIDATE(is_same_value(stack, self));
    current = get_stack_piece_previous(current);
  }
  return success();
}

// Transfers the arguments from the top of the previous piece (which the frame
// points to) to the bottom of the new stack segment.
static void transfer_top_arguments(value_t new_piece, frame_t *frame,
    size_t arg_count) {
  frame_t new_frame = frame_empty();
  open_stack_piece(new_piece, &new_frame);
  for (size_t i = 0; i < arg_count; i++) {
    value_t value = frame_peek_value(frame, arg_count - i - 1);
    frame_push_value(&new_frame, value);
  }
  close_frame(&new_frame);
}

static void push_stack_piece_bottom_frame(runtime_t *runtime,
    value_t stack_piece, value_t arg_map) {
  frame_t bottom = frame_empty();
  value_t code_block = ROOT(runtime, stack_piece_bottom_code_block);
  // The transferred arguments are going to appear as if they were arguments
  // passed from this frame so we have to "allocate" enough room for them on
  // the stack.
  open_stack_piece(stack_piece, &bottom);
  size_t arg_count = (size_t) get_array_length(arg_map);
  bool pushed = try_push_new_frame(&bottom,
      (size_t) (get_code_block_high_water_mark(code_block) + arg_count),
      ffSynthetic | ffStackPieceBottom, false);
  CHECK_TRUE("pushing bottom frame", pushed);
  frame_set_code_block(&bottom, code_block);
  frame_set_argument_map(&bottom, arg_map);
  close_frame(&bottom);
}

// Reads the state of the stack piece lid into the given frame; doesn't modify
// the piece in any way though.
static void read_stack_piece_lid(value_t piece, frame_t *frame) {
  CHECK_TRUE("stack piece not closed", is_stack_piece_closed(piece));
  frame->stack_piece = piece;
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  frame->frame_pointer = stack_start
      + get_integer_value(get_stack_piece_lid_frame_pointer(piece));
  frame_walk_down_stack(frame);
}

void open_stack_piece(value_t piece, frame_t *frame) {
  CHECK_FAMILY(ofStackPiece, piece);
  read_stack_piece_lid(piece, frame);
  set_stack_piece_lid_frame_pointer(piece, nothing());
}

void close_frame(frame_t *frame) {
  value_t piece = frame->stack_piece;
  CHECK_FALSE("stack piece already closed", is_stack_piece_closed(piece));
  bool pushed = try_push_new_frame(frame, 0, ffLid | ffSynthetic, true);
  CHECK_TRUE("Failed to close frame", pushed);
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  set_stack_piece_lid_frame_pointer(piece,
      new_integer(frame->frame_pointer - stack_start));
  frame->stack_piece = nothing();
  frame->frame_pointer = frame->limit_pointer = frame->stack_pointer = 0;
  frame->pc = 0;
}

value_t push_stack_frame(runtime_t *runtime, value_t stack, frame_t *frame,
    size_t frame_capacity, value_t arg_map) {
  CHECK_FAMILY(ofStack, stack);
  value_t top_piece = get_stack_top_piece(stack);
  CHECK_FALSE("stack piece closed", is_stack_piece_closed(top_piece));
  if (!try_push_new_frame(frame, frame_capacity, ffOrganic, false)) {
    // There wasn't room to push this frame onto the top stack piece so
    // allocate a new top piece that definitely has room.
    size_t default_capacity = (size_t) get_stack_default_piece_capacity(stack);
    size_t transfer_arg_count = (size_t) get_array_length(arg_map);
    size_t required_capacity =
          frame_capacity      // the new frame's locals
        + kFrameHeaderSize    // the new frame's header
        + 1                   // the synthetic bottom frame's one local
        + kFrameHeaderSize    // the synthetic bottom frame's header
        + transfer_arg_count; // any arguments to be copied onto the piece
    size_t new_capacity = max_size(default_capacity, required_capacity);

    // Create and initialize the new stack segment. The frame struct is still
    // pointing to the old frame.
    TRY_DEF(new_piece, new_heap_stack_piece(runtime, new_capacity, top_piece,
        stack));
    push_stack_piece_bottom_frame(runtime, new_piece, arg_map);
    transfer_top_arguments(new_piece, frame, transfer_arg_count);
    set_stack_top_piece(stack, new_piece);

    // Close the previous stack piece, recording the frame state.
    close_frame(frame);

    // Finally, create a new frame on the new stack which includes updating the
    // struct. The required_capacity calculation ensures that this call will
    // succeed.
    open_stack_piece(new_piece, frame);
    bool pushed_stack_piece = try_push_new_frame(frame, frame_capacity,
        ffOrganic, false);
    if (!pushed_stack_piece)
      try_push_new_frame(frame, frame_capacity, ffOrganic, false);
    CHECK_TRUE("pushing on new piece failed", pushed_stack_piece);
  }
  frame_set_argument_map(frame, arg_map);
  return success();
}

void frame_walk_down_stack(frame_t *frame) {
  frame_t snapshot = *frame;
  // Get the frame pointer and capacity from the frame's header.
  value_t *stack_start = frame_get_stack_piece_bottom(frame);
  frame->frame_pointer = stack_start + frame_get_previous_frame_pointer(&snapshot);
  frame->limit_pointer = stack_start + frame_get_previous_limit_pointer(&snapshot);
  frame->flags = frame_get_previous_flags(&snapshot);
  frame->pc = frame_get_previous_pc(&snapshot);
  // The stack pointer will be the first field of the top frame's header.
  frame->stack_pointer = snapshot.frame_pointer - kFrameHeaderSize;
}

bool frame_has_flag(frame_t *frame, frame_flag_t flag) {
  return get_flag_set_at(frame->flags, flag);
}

value_t *frame_get_stack_piece_bottom(frame_t *frame) {
  return get_stack_piece_storage(frame->stack_piece);
}

value_t *frame_get_stack_piece_top(frame_t *frame) {
  value_t *storage = get_stack_piece_storage(frame->stack_piece);
  return storage + get_integer_value(get_stack_piece_capacity(frame->stack_piece));
}

frame_t open_stack(value_t stack) {
  CHECK_FAMILY(ofStack, stack);
  frame_t result = frame_empty();
  open_stack_piece(get_stack_top_piece(stack), &result);
  return result;
}

/// ### Barrier iter

value_t barrier_iter_init(barrier_iter_t *iter, frame_t *frame) {
  value_t stack = get_stack_piece_stack(frame->stack_piece);
  return iter->current = get_stack_top_barrier(stack);
}

value_t barrier_iter_advance(barrier_iter_t *iter) {
  iter->current = get_barrier_state_previous(iter->current);
  return iter->current;
}

// --- F r a m e ---

bool try_push_new_frame(frame_t *frame, size_t frame_capacity, uint32_t flags,
    bool is_lid) {
  value_t stack_piece = frame->stack_piece;
  CHECK_FALSE("pushing closed stack piece", is_stack_piece_closed(stack_piece));
  // First record the current state of the old top frame so we can store it in
  // the header of the new frame.
  frame_t old_frame = *frame;
  // Determine how much room is left in the stack piece.
  value_t *stack_piece_start = get_stack_piece_storage(stack_piece);
  size_t capacity = (size_t) get_integer_value(get_stack_piece_capacity(stack_piece));
  value_t *stack_piece_limit = stack_piece_start + capacity;
  // There must always be room on a stack piece for the lid frame because it
  // must always be possible to close a stack if a condition occurs, which we
  // assume it can at any time. So we hold back a frame header's worth of stack
  // except when allocating the lid.
  if (!is_lid)
    stack_piece_limit -= kFrameHeaderSize;
  value_t *new_frame_pointer = old_frame.stack_pointer + kFrameHeaderSize;
  value_t *new_frame_limit = new_frame_pointer + frame_capacity;
  if (new_frame_limit > stack_piece_limit)
    return false;
  // Store the new frame's info in the frame struct.
  frame->stack_pointer = frame->frame_pointer = new_frame_pointer;
  frame->limit_pointer = new_frame_limit;
  frame->flags = new_flag_set(flags);
  frame->pc = 0;
  // Record the relevant information about the previous frame in the new frame's
  // header.
  frame_set_previous_frame_pointer(frame, old_frame.frame_pointer - stack_piece_start);
  frame_set_previous_limit_pointer(frame, old_frame.limit_pointer - stack_piece_start);
  frame_set_previous_flags(frame, old_frame.flags);
  frame_set_previous_pc(frame, old_frame.pc);
  frame_set_code_block(frame, nothing());
  frame_set_argument_map(frame, nothing());
  return true;
}

void frame_pop_within_stack_piece(frame_t *frame) {
  CHECK_FALSE("popping closed stack piece",
      is_stack_piece_closed(frame->stack_piece));
  CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
  frame_walk_down_stack(frame);
}

// Accesses a frame header field, that is, a bookkeeping field below the frame
// pointer.
static value_t *access_frame_header_field(frame_t *frame, size_t offset) {
  CHECK_REL("frame header field out of bounds", offset, <=, kFrameHeaderSize);
  value_t *location = frame->frame_pointer - offset - 1;
  CHECK_TRUE("frame header out of bounds",
      frame_get_stack_piece_bottom(frame) <= location);
  return location;
}

// Returns true if the given absolute offset is within the fields available to
// the given frame.
static bool is_offset_within_frame(frame_t *frame, value_t *offset) {
  return (frame->frame_pointer <= offset) && (offset < frame->limit_pointer);
}

void frame_set_previous_frame_pointer(frame_t *frame, size_t value) {
  *access_frame_header_field(frame, kFrameHeaderPreviousFramePointerOffset) =
      new_integer(value);
}

size_t frame_get_previous_frame_pointer(frame_t *frame) {
  return (size_t) get_integer_value(
      *access_frame_header_field(frame, kFrameHeaderPreviousFramePointerOffset));
}

void frame_set_previous_limit_pointer(frame_t *frame, size_t value) {
  *access_frame_header_field(frame, kFrameHeaderPreviousLimitPointerOffset) =
      new_integer(value);
}

size_t frame_get_previous_limit_pointer(frame_t *frame) {
  return (size_t) get_integer_value(*access_frame_header_field(frame,
      kFrameHeaderPreviousLimitPointerOffset));
}

void frame_set_previous_flags(frame_t *frame, value_t flags) {
  *access_frame_header_field(frame, kFrameHeaderPreviousFlagsOffset) = flags;
}

value_t frame_get_previous_flags(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderPreviousFlagsOffset);
}

void frame_set_code_block(frame_t *frame, value_t code_block) {
  *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset) = code_block;
}

value_t frame_get_code_block(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset);
}

void frame_set_argument_map(frame_t *frame, value_t argument_map) {
  *access_frame_header_field(frame, kFrameHeaderArgumentMapOffset) =
      argument_map;
}

value_t frame_get_argument_map(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderArgumentMapOffset);
}

void frame_set_previous_pc(frame_t *frame, size_t pc) {
  *access_frame_header_field(frame, kFrameHeaderPreviousPcOffset) = new_integer(pc);
}

size_t frame_get_previous_pc(frame_t *frame) {
  return (size_t) get_integer_value(*access_frame_header_field(frame,
      kFrameHeaderPreviousPcOffset));
}

value_t frame_push_value(frame_t *frame, value_t value) {
  // Check that the stack is in sync with this frame.
  COND_CHECK_TRUE("push out of frame bounds", ccOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer));
  CHECK_FALSE("pushing condition", is_condition(value));
  *(frame->stack_pointer++) = value;
  return success();
}

value_array_t frame_alloc_array(frame_t *frame, size_t size) {
  CHECK_TRUE("push out of frame bounds",
      is_offset_within_frame(frame, frame->stack_pointer + (size - 1)));
  value_t *start = frame->stack_pointer;
  frame->stack_pointer += size;
  return new_value_array(start, size);
}

value_t frame_alloc_derived_object(frame_t *frame, genus_descriptor_t *desc) {
  value_array_t memory = frame_alloc_array(frame, desc->field_count);
  value_t result = alloc_derived_object(memory, desc, frame->stack_piece);
  if (desc->on_scope_exit) {
    value_t stack = get_stack_piece_stack(frame->stack_piece);
    set_barrier_state_previous(result, get_stack_top_barrier(stack));
    set_stack_top_barrier(stack, result);
  }
  return result;
}

void frame_destroy_derived_object(frame_t *frame, genus_descriptor_t *desc) {
#ifdef EXPENSIVE_CHECKS
  value_t *anchor_ptr = frame->stack_pointer - desc->after_field_count - 1;
  value_t derived = new_derived_object((address_t) anchor_ptr);
  value_validate(derived);
#endif
  for (size_t i = 0; i < desc->field_count; i++)
    *(--frame->stack_pointer) = nothing();
}

value_t frame_pop_value(frame_t *frame) {
  COND_CHECK_TRUE("pop out of frame bounds", ccOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer - 1));
  return *(--frame->stack_pointer);
}

value_t frame_peek_value(frame_t *frame, size_t index) {
  return frame->stack_pointer[-(index + 1)];
}

value_t frame_get_argument(frame_t *frame, size_t param_index) {
  value_t *stack_pointer = frame->frame_pointer - kFrameHeaderSize;
  value_t arg_map = frame_get_argument_map(frame);
  size_t offset = (size_t) get_integer_value(get_array_at(arg_map, param_index));
  return stack_pointer[-(offset + 1)];
}

value_t frame_get_raw_argument(frame_t *frame, size_t eval_index) {
  value_t *stack_pointer = frame->frame_pointer - kFrameHeaderSize;
  return stack_pointer[-(eval_index + 1)];
}

value_t frame_get_pending_argument_at(frame_t *frame, value_t tags,
    int64_t index) {
  CHECK_FAMILY(ofCallTags, tags);
  size_t offset = (size_t) get_call_tags_offset_at(tags, index);
  return frame_peek_value(frame, offset);
}

void frame_set_argument(frame_t *frame, size_t param_index, value_t value) {
  value_t *stack_pointer = frame->frame_pointer - kFrameHeaderSize;
  value_t arg_map = frame_get_argument_map(frame);
  size_t offset = (size_t) get_integer_value(get_array_at(arg_map, param_index));
  stack_pointer[-(offset + 1)] = value;
}

value_t frame_get_local(frame_t *frame, size_t index) {
  value_t *location = frame->frame_pointer + index;
  COND_CHECK_TRUE("local not defined yet", ccOutOfBounds,
      location < frame->stack_pointer);
  return *location;
}

// --- F r a m e   i t e r a t o r ---

frame_iter_t frame_iter_from_frame(frame_t *frame) {
  frame_iter_t result = {*frame};
  return result;
}

frame_t *frame_iter_get_current(frame_iter_t *iter) {
  return &iter->current;
}

bool frame_iter_at_bottom(frame_iter_t *iter) {
  return frame_has_flag(&iter->current, ffStackBottom);
}

bool frame_iter_advance(frame_iter_t *iter) {
  frame_t *current = &iter->current;
  do {
    // Advance the current frame to the next one.
    frame_walk_down_stack(current);
    if (frame_has_flag(current, ffStackPieceBottom)) {
      // If this is the bottom frame of a stack piece jump to the previous
      // piece.
      current->stack_piece = get_stack_piece_previous(current->stack_piece);
      read_stack_piece_lid(current->stack_piece, current);
    } else if (frame_has_flag(current, ffStackBottom)) {
      // If we're at the bottom of the stack there are no more frames.
      return false;
    }
  } while (!frame_has_flag(current, ffOrganic));
  // We must have reached an organic frame so return true.
  return true;
}

// ## Escape

FIXED_GET_MODE_IMPL(escape, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Escape, escape);
GET_FAMILY_PRIMARY_TYPE_IMPL(escape);

ACCESSORS_IMPL(Escape, escape, snInGenusOpt(dgEscapeSection), Section, section);

value_t escape_validate(value_t value) {
  VALIDATE_FAMILY(ofEscape, value);
  VALIDATE_GENUS_OPT(dgEscapeSection, get_escape_section(value));
  return success();
}

static value_t emit_fire_escape(assembler_t *assm) {
  TRY(assembler_emit_fire_escape_or_barrier(assm));
  return success();
}

static value_t escape_is_live(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofEscape, self);
  return new_boolean(!is_nothing(get_escape_section(self)));
}

value_t add_escape_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  TRY(add_custom_method_impl(runtime, deref(s_map), "escape()", 1,
      new_flag_set(kFlagSetAllOff), emit_fire_escape));
  ADD_BUILTIN_IMPL("escape.is_live?", 0, escape_is_live);
  return success();
}

// ## Lambda

GET_FAMILY_PRIMARY_TYPE_IMPL(lambda);

ACCESSORS_IMPL(Lambda, lambda, snInFamilyOpt(ofMethodspace), Methods, methods);
ACCESSORS_IMPL(Lambda, lambda, snInFamilyOpt(ofArray), Captures, captures);

value_t lambda_validate(value_t self) {
  VALIDATE_FAMILY(ofLambda, self);
  VALIDATE_FAMILY_OPT(ofMethodspace, get_lambda_methods(self));
  VALIDATE_FAMILY_OPT(ofArray, get_lambda_captures(self));
  return success();
}

void lambda_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofLambda, value);
  string_buffer_printf(context->buf, "lambda~%w",
      canonicalize_value_for_print(value, context));
}

value_t emit_lambda_call_trampoline(assembler_t *assm) {
  TRY(assembler_emit_delegate_lambda_call(assm));
  return success();
}

value_t add_lambda_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  TRY(add_custom_method_impl(runtime, deref(s_map), "lambda()", 0,
      new_flag_set(mfLambdaDelegate), emit_lambda_call_trampoline));
  return success();
}

value_t get_lambda_capture(value_t self, size_t index) {
  CHECK_FAMILY(ofLambda, self);
  value_t captures = get_lambda_captures(self);
  return get_array_at(captures, index);
}

value_t ensure_lambda_owned_values_frozen(runtime_t *runtime, value_t self) {
  TRY(ensure_frozen(runtime, get_lambda_captures(self)));
  return success();
}

/// ## Block

GET_FAMILY_PRIMARY_TYPE_IMPL(block);

ACCESSORS_IMPL(Block, block, snNoCheck, Section, section);

value_t block_validate(value_t self) {
  VALIDATE_FAMILY(ofBlock, self);
  VALIDATE_GENUS_OPT(dgBlockSection, get_block_section(self));
  return success();
}

void block_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBlock, value);
  string_buffer_printf(context->buf, "block~%w",
      canonicalize_value_for_print(value, context));
}

static value_t emit_block_call_trampoline(assembler_t *assm) {
  TRY(assembler_emit_delegate_block_call(assm));
  return success();
}

static value_t block_is_live(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofBlock, self);
  return new_boolean(!is_nothing(get_block_section(self)));
}

value_t add_block_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  TRY(add_custom_method_impl(runtime, deref(s_map), "block()", 0,
      new_flag_set(mfBlockDelegate), emit_block_call_trampoline));
  ADD_BUILTIN_IMPL("block.is_live?", 0, block_is_live);
  return success();
}

// Returns the refraction point data (that is, a derived object which holds
// refraction point state) for the given value.
static value_t get_refraction_point(value_t self) {
  if (is_heap_object(self)) {
    CHECK_FAMILY(ofBlock, self);
    return get_block_section(self);
  } else {
    CHECK_DOMAIN(vdDerivedObject, self);
    return self;
  }
}

void get_refractor_refracted_frame(value_t self, size_t block_depth,
    frame_t *frame) {
  CHECK_REL("refractor not nested", block_depth, >, 0);
  value_t current = self;
  for (size_t i = block_depth; i > 0; i--) {
    // Locate the next refraction point.
    value_t refraction_point = get_refraction_point(current);
    value_t fp_val = get_refraction_point_frame_pointer(refraction_point);
    size_t fp = (size_t) get_integer_value(fp_val);
    // Update the frame state to point to it.
    frame->stack_piece = get_derived_object_host(refraction_point);
    frame->frame_pointer = get_stack_piece_storage(frame->stack_piece) + fp;
    if (i > 1)
      current = frame_get_argument(frame, 0);
  }
  // We don't know the limit or stack pointers so the best estimate is that they
  // definitely don't go past the stack piece.
  frame->limit_pointer = frame_get_stack_piece_top(frame);
  frame->stack_pointer = frame_get_stack_piece_top(frame);
  // We also don't know what the flags should be so set this to nothing such
  // that trying to access them as flags fails.
  frame->flags = nothing();
}

// --- B a c k t r a c e ---

FIXED_GET_MODE_IMPL(backtrace, vmMutable);
GET_FAMILY_PRIMARY_TYPE_IMPL(backtrace);
NO_BUILTIN_METHODS(backtrace);

ACCESSORS_IMPL(Backtrace, backtrace, snInFamily(ofArrayBuffer), Entries,
    entries);

value_t backtrace_validate(value_t value) {
  VALIDATE_FAMILY(ofBacktrace, value);
  return success();
}

void backtrace_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBacktrace, value);
  string_buffer_printf(context->buf, "--- backtrace ---");
  value_t entries = get_backtrace_entries(value);
  for (int64_t i = 0; i < get_array_buffer_length(entries); i++) {
    string_buffer_putc(context->buf, '\n');
    string_buffer_printf(context->buf, "- ");
    value_print_inner_on(get_array_buffer_at(entries, i), context, -1);
  }
}

value_t capture_backtrace(runtime_t *runtime, frame_t *top, size_t skip_count) {
  TRY_DEF(frames, new_heap_array_buffer(runtime, 16));
  frame_iter_t iter = frame_iter_from_frame(top);
  size_t remaining_skips = skip_count;
  do {
    frame_t *frame = frame_iter_get_current(&iter);
    // We won't know if a frame represents an entry before we've tried to
    // capture it so do that first before deciding whether to skip. It's a
    // little wasteful but shouldn't be a big deal.
    TRY_DEF(entry, capture_backtrace_entry(runtime, frame));
    if (!is_nothing(entry)) {
      if (remaining_skips == 0) {
        TRY(add_to_array_buffer(runtime, frames, entry));
      } else {
        remaining_skips--;
      }
    }
  } while (frame_iter_advance(&iter));
  return new_heap_backtrace(runtime, frames);
}

// --- B a c k t r a c e   e n t r y ---

FIXED_GET_MODE_IMPL(backtrace_entry, vmMutable);

ACCESSORS_IMPL(BacktraceEntry, backtrace_entry, snNoCheck, Invocation,
    invocation);
ACCESSORS_IMPL(BacktraceEntry, backtrace_entry, snNoCheck, Opcode, opcode);

value_t backtrace_entry_validate(value_t value) {
  VALIDATE_FAMILY(ofBacktraceEntry, value);
  return success();
}

void generic_invocation_print_on(invocation_entry_t *invocation,
    size_t invocation_size, int32_t opcode, print_on_context_t *context) {
  // Ensure entries don't have arguments etc. so get them out of the way first.
  if (opcode == ocCallEnsurer) {
    string_buffer_printf(context->buf, "ensure");
    return;
  }
  value_t subject = new_not_found_condition(0xc8e7fc1b);
  value_t selector = new_not_found_condition(0x39063f3a);
  value_t transport = new_not_found_condition(0x255364c3);
  for (size_t i = 0; i < invocation_size; i++) {
    value_t tag = invocation[i].tag;
    value_t value = invocation[i].value;
    if (in_family(ofKey, tag)) {
      int64_t id = get_key_id(tag);
      if (id == kSubjectKeyId) {
        subject = value;
      } else if (id == kSelectorKeyId) {
        selector = value;
      } else if (id == kTransportKeyId) {
        transport = value;
      }
    }
  }
  // Print the subject as the first thing. For aborts we ignore the subject
  // (which is not supposed to be there anyway) and just print abort.
  if (opcode == ocSignalEscape || opcode == ocBuiltinMaybeEscape) {
    string_buffer_printf(context->buf, "leave");
  } else if (opcode == ocSignalContinue) {
    string_buffer_printf(context->buf, "signal");
  } else if (!in_condition_cause(ccNotFound, subject)) {
    value_print_inner_on(subject, context, -1);
  }
  // Begin the selector.
  if (in_family(ofOperation, selector)) {
    operation_print_open_on(selector, transport, context);
  } else if (!in_condition_cause(ccNotFound, selector)) {
    value_print_inner_on(selector, context, -1);
  }
  // Number of positional arguments.
  size_t posc;
  // Number of arguments in total discounting the subject and selector.
  size_t argc;
  // Print the positional arguments.
  for (posc = argc = 0; true; posc++, argc++) {
    value_t tag = new_integer(posc);
    value_t value = nothing();
    for (size_t i = 0; i < invocation_size; i++) {
      if (is_same_value(invocation[i].tag, tag)) {
        value = invocation[i].value;
        break;
      }
    }
    if (is_nothing(value))
      break;
    if (argc > 0)
      string_buffer_printf(context->buf, ", ");
    value_print_inner_on(value, context, -1);
  }
  // Print any remaining arguments. Note that this will print them in
  // nondeterministic order since the order depends on the iteration order of
  // the map. This is bad.
  for (size_t i = 0; i < invocation_size; i++) {
    value_t tag = invocation[i].tag;
    value_t value = invocation[i].value;
    if (in_family(ofKey, tag)) {
      int64_t id = get_key_id(tag);
      if (id == kSubjectKeyId || id == kSelectorKeyId || id == kTransportKeyId)
        // Don't print the subject/selector again.
        continue;
    } else if (is_integer(tag) && ((size_t) get_integer_value(tag)) < posc) {
      // Don't print any of the positional arguments again. The size_t cast of
      // the integer value means that negative values will become very large
      // positive ones and hence compare greater than posc.
      continue;
    }
    if (argc > 0)
      string_buffer_printf(context->buf, ", ");
    // Unquote the value such that string tags are unquoted as you would expect.
    print_on_context_t new_context = *context;
    new_context.flags = pfUnquote;
    value_print_inner_on(tag, &new_context, -1);
    string_buffer_printf(context->buf, ": ");
    value_print_inner_on(value, context, -1);
    argc++;
  }
  // End the selector.
  if (in_family(ofOperation, selector))
    operation_print_close_on(selector, context);
}

void backtrace_entry_invocation_print_on(value_t invocation, int32_t opcode,
    print_on_context_t *context) {
  size_t size = get_id_hash_map_size(invocation);
  invocation_entry_t *entries = allocator_default_malloc_structs(invocation_entry_t, size);
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, invocation);
  for (size_t i = 0; id_hash_map_iter_advance(&iter); i++)
    id_hash_map_iter_get_current(&iter, &entries[i].tag, &entries[i].value);
  generic_invocation_print_on(entries, size, opcode, context);
  allocator_default_free_structs(invocation_entry_t, size, entries);
}

void backtrace_entry_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBacktraceEntry, value);
  value_t invocation = get_backtrace_entry_invocation(value);
  int32_t opcode = (int32_t) get_integer_value(get_backtrace_entry_opcode(value));
  backtrace_entry_invocation_print_on(invocation, opcode, context);
}

static bool is_invocation_opcode(opcode_t op) {
  switch (op) {
  case ocInvoke:
  case ocSignalEscape:
  case ocSignalContinue:
  case ocBuiltinMaybeEscape:
  case ocCallEnsurer:
    return true;
  default:
    return false;
  }
}

value_t capture_backtrace_entry(runtime_t *runtime, frame_t *frame) {
  // Check whether the program counter stored for this frame points immediately
  // after an invoke instruction. If it does we'll use that instruction to
  // construct the entry.
  value_t code_block = frame_get_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  size_t pc = frame->pc;
  if (pc < kInvokeOperationSize)
    return nothing();
  blob_t data = get_blob_data(bytecode);
  opcode_t op = (opcode_t) blob_short_at(data, pc - kInvokeOperationSize);
  if (!is_invocation_opcode(op))
    return nothing();
  value_t tags = whatever();
  if (op == ocCallEnsurer) {
    return new_heap_backtrace_entry(runtime, nothing(), new_integer(op));
  } else if (op == ocBuiltinMaybeEscape) {
    // A builtin escape leaves the record on the stack. Pop it off. This doesn't
    // actually modify the stack only the frame.
    tags = frame_pop_value(frame);
  } else {
    // Okay so we have an invoke we can use. Grab the invocation record which
    // is baked into the instruction.
    size_t record_index = blob_short_at(data, pc - kInvokeOperationSize + 1);
    value_t value_pool = get_code_block_value_pool(code_block);
    tags = get_array_at(value_pool, record_index);
  }
  // Scan through the record to build the invocation map.
  TRY_DEF(invocation, new_heap_id_hash_map(runtime, 16));
  int64_t arg_count = get_call_tags_entry_count(tags);
  for (int64_t i = 0; i < arg_count; i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    value_t arg = frame_get_pending_argument_at(frame, tags, i);
    TRY(set_id_hash_map_at(runtime, invocation, tag, arg));
  }
  // Wrap the result in a backtrace entry.
  return new_heap_backtrace_entry(runtime, invocation, new_integer(op));
}

/// ## Task

FIXED_GET_MODE_IMPL(task, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Task, task);

ACCESSORS_IMPL(Task, task, snInFamilyOpt(ofProcess), Process, process);
ACCESSORS_IMPL(Task, task, snInFamily(ofStack), Stack, stack);

value_t task_validate(value_t self) {
  VALIDATE_FAMILY(ofTask, self);
  VALIDATE_FAMILY_OPT(ofProcess, get_task_process(self));
  VALIDATE_FAMILY(ofStack, get_task_stack(self));
  return success();
}

/// ## Process

FIXED_GET_MODE_IMPL(process, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Process, process);

ACCESSORS_IMPL(Process, process, snInFamily(ofFifoBuffer), WorkQueue,
    work_queue);
ACCESSORS_IMPL(Process, process, snInFamily(ofTask), RootTask, root_task);
ACCESSORS_IMPL(Process, process, snInFamily(ofHashSource), HashSource,
    hash_source);
ACCESSORS_IMPL(Process, process, snInFamily(ofVoidP), AirlockPtr, airlock_ptr);

value_t process_validate(value_t self) {
  VALIDATE_FAMILY(ofProcess, self);
  VALIDATE_FAMILY(ofFifoBuffer, get_process_work_queue(self));
  VALIDATE_FAMILY(ofTask, get_process_root_task(self));
  VALIDATE_FAMILY(ofHashSource, get_process_hash_source(self));
  VALIDATE_FAMILY(ofVoidP, get_process_airlock_ptr(self));
  return success();
}

void job_init(job_t *job, value_t code, value_t data, value_t promise,
    value_t guard) {
  CHECK_FAMILY(ofCodeBlock, code);
  CHECK_FAMILY_OPT(ofPromise, promise);
  CHECK_FAMILY_OPT(ofPromise, guard);
  job->code = code;
  job->data = data;
  job->promise = promise;
  job->guard = guard;
}

value_t offer_process_job(runtime_t *runtime, value_t process, job_t *job) {
  CHECK_FAMILY(ofProcess, process);
  value_t work_queue = get_process_work_queue(process);
  value_t data[kProcessWorkQueueWidth] = { job->code, job->data, job->promise,
      job->guard };
  return offer_to_fifo_buffer(runtime, work_queue, data, kProcessWorkQueueWidth);
}

bool take_process_ready_job(value_t process, job_t *job_out) {
  CHECK_FAMILY(ofProcess, process);
  // Scan through the jobs to find the first one that's ready to run.
  fifo_buffer_iter_t iter;
  fifo_buffer_iter_init(&iter, get_process_work_queue(process));
  while (fifo_buffer_iter_advance(&iter)) {
    value_t result[kProcessWorkQueueWidth];
    fifo_buffer_iter_get_current(&iter, result, kProcessWorkQueueWidth);
    // Read the current entry into the output job for convenience.
    job_init(job_out, result[0], result[1], result[2], result[3]);
    value_t guard = job_out->guard;
    if (is_nothing(guard) || is_promise_settled(guard)) {
      // This one is ready to run. Remove it from the buffer.
      fifo_buffer_iter_take_current(&iter);
      return true;
    }
  }
  return false;
}

value_t finalize_process(garbage_value_t dead_self) {
  // Because this deals with a dead object during gc there are hardly any
  // implicit type checks, instead this has to be done with raw offsets and
  // explicit checks. Errors in this code are likely to be a nightmare to debug
  // so extra effort to sanity check everything is worthwhile.
  CHECK_EQ("running process finalizer on non-process", ofProcess,
      get_garbage_object_family(dead_self));
  garbage_value_t dead_airlock_ptr = get_garbage_object_field(dead_self,
      kProcessAirlockPtrOffset);
  CHECK_EQ("invalid process during finalization", ofVoidP,
      get_garbage_object_family(dead_airlock_ptr));
  garbage_value_t airlock_value = get_garbage_object_field(dead_airlock_ptr,
      kVoidPValueOffset);
  void *raw_airlock_ptr = value_to_pointer_bit_cast(airlock_value.value);
  process_airlock_t *airlock = (process_airlock_t*) raw_airlock_ptr;
  if (!process_airlock_destroy(airlock))
    return new_system_call_failed_condition("free");
  return success();
}

process_airlock_t *get_process_airlock(value_t process) {
  value_t ptr = get_process_airlock_ptr(process);
  return (process_airlock_t*) get_void_p_value(ptr);
}

process_airlock_t *process_airlock_new(runtime_t *runtime) {
  process_airlock_t *airlock = allocator_default_malloc_struct(process_airlock_t);
  if (airlock == NULL)
    return NULL;
  airlock->runtime = runtime;
  airlock->undelivered_undertakings = atomic_int64_new(0);
  if (!worklist_init(kAirlockPendingAtomicCount, 1)(&airlock->delivered_undertakings))
    return NULL;
  return airlock;
}

void process_airlock_begin_undertaking(process_airlock_t *airlock,
    undertaking_t *undertaking) {
  CHECK_EQ("opening non initialized", usInitialized, undertaking->state);
  undertaking->state = usBegun;
  atomic_int64_increment(&airlock->undelivered_undertakings);
}

void process_airlock_deliver_undertaking(process_airlock_t *airlock,
    undertaking_t *undertaking) {
  CHECK_EQ("deliveing undertaking not begun", usBegun, undertaking->state);
  undertaking->state = usDelivered;
  opaque_t o_undertaking = p2o(undertaking);
  // Decrement the number of undertakings first to avoid the 1-case where
  // the last open undertaking has just been delivered and there will be no more
  // but the count is briefly nonzero.
  atomic_int64_decrement(&airlock->undelivered_undertakings);
  bool offered = worklist_schedule(kAirlockPendingAtomicCount, 1)
      (&airlock->delivered_undertakings, &o_undertaking, 1, duration_unlimited());
  CHECK_TRUE("out of capacity", offered);
}

static value_t undertaking_finish(undertaking_t *undertaking,
    value_t process, process_airlock_t *airlock) {
  CHECK_EQ("finishing undelivered", usDelivered, undertaking->state);
  undertaking->state = usFinished;
  return (undertaking->controller->finish)(undertaking, process, airlock);
}

static void undertaking_destroy(runtime_t *runtime, undertaking_t *undertaking) {
  CHECK_EQ("destroying unfinished", usFinished, undertaking->state);
  (undertaking->controller->destroy)(runtime, undertaking);
}

value_t finish_process_delivered_undertakings(value_t process, bool blocking,
    size_t *count_out) {
  CHECK_FAMILY(ofProcess, process);
  process_airlock_t *airlock = get_process_airlock(process);
  undertaking_t *undertaking = NULL;
  if (count_out != NULL)
    *count_out = 0;
  duration_t timeout = blocking ? duration_unlimited() : duration_instant();
  while (process_airlock_next_delivered_undertaking(airlock, timeout, &undertaking)) {
    TRY(undertaking_finish(undertaking, process, airlock));
    undertaking_destroy(airlock->runtime, undertaking);
    undertaking = NULL;
    timeout = duration_instant();
    if (count_out != NULL)
      (*count_out)++;
  }
  return success();
}

bool process_airlock_next_delivered_undertaking(process_airlock_t *airlock,
    duration_t timeout, undertaking_t **result_out) {
  opaque_t next = o0();
  bool took = worklist_take(kAirlockPendingAtomicCount, 1)(
      &airlock->delivered_undertakings, &next, 1, timeout);
  if (took)
    *result_out = (undertaking_t*) o2p(next);
  return took;
}

bool process_airlock_destroy(process_airlock_t *airlock) {
  CHECK_EQ("open undertakings", 0, atomic_int64_get(&airlock->undelivered_undertakings));
  worklist_dispose(kAirlockPendingAtomicCount, 1)(&airlock->delivered_undertakings);
  allocator_default_free_struct(process_airlock_t, airlock);
  return true;
}

/// ## Reified arguments

FIXED_GET_MODE_IMPL(reified_arguments, vmMutable);
GET_FAMILY_PRIMARY_TYPE_IMPL(reified_arguments);

ACCESSORS_IMPL(ReifiedArguments, reified_arguments, snInFamily(ofArray), Params,
    params);
ACCESSORS_IMPL(ReifiedArguments, reified_arguments, snInFamily(ofArray), Values,
    values);
ACCESSORS_IMPL(ReifiedArguments, reified_arguments, snInFamily(ofArray), Argmap,
    argmap);
ACCESSORS_IMPL(ReifiedArguments, reified_arguments, snInFamily(ofCallTags),
    Tags, tags);

value_t reified_arguments_validate(value_t self) {
  VALIDATE_FAMILY(ofReifiedArguments, self);
  VALIDATE_FAMILY(ofArray, get_reified_arguments_params(self));
  VALIDATE_FAMILY(ofArray, get_reified_arguments_values(self));
  VALIDATE_FAMILY(ofArray, get_reified_arguments_argmap(self));
  VALIDATE_FAMILY(ofCallTags, get_reified_arguments_tags(self));
  return success();
}

void reified_arguments_print_on(value_t value, print_on_context_t *context) {
  if (context->depth == 1) {
    // If we can't print the elements anyway we might as well just show the
    // argument count.
    string_buffer_printf(context->buf, "#<reified_arguments[%i]>",
        (int) get_array_length(get_reified_arguments_values(value)));
  } else {
    value_t values = get_reified_arguments_values(value);
    value_t tags = get_reified_arguments_tags(value);
    size_t size = get_array_length(values);
    invocation_entry_t *entries = allocator_default_malloc_structs(invocation_entry_t, size);
    for (size_t i = 0; i < size; i++) {
      entries[i].tag = get_call_tags_tag_at(tags, i);
      int64_t offset = get_call_tags_offset_at(tags, i);
      entries[i].value = get_array_at(values, offset);
    }
    string_buffer_printf(context->buf, "#<reified_arguments ");
    generic_invocation_print_on(entries, size, ocInvoke, context);
    string_buffer_printf(context->buf, ">");
    allocator_default_free_structs(invocation_entry_t, size, entries);
  }
}

// If there is an argument corresponding to the given tag, store the value array
// index that holds the value in the out param and return true. Otherwise return
// false.
static bool reified_arguments_get_value_index(value_t self, value_t tag,
    size_t *index_out) {
  // First try to find the argument based on the tags used by the caller. The
  // expectation is that this will usually work.
  value_t call_tags = get_reified_arguments_tags(self);
  size_t argc = (size_t) get_call_tags_entry_count(call_tags);
  for (size_t ia = 0; ia < argc; ia++) {
    value_t candidate = get_call_tags_tag_at(call_tags, ia);
    if (value_identity_compare(candidate, tag)) {
      // Found it among the arguments.
      size_t offset = (size_t) get_call_tags_offset_at(call_tags, ia);
      *index_out = offset;
      return true;
    }
  }

  // Didn't find the value under the tags used by the caller; go through the
  // params to see if there is an alias we can find it under.
  value_t params = get_reified_arguments_params(self);
  // Paramc may be different from argc if there are extra arguments not
  // anticipated in the method declaration.
  size_t paramc = (size_t) get_array_length(params);
  for (size_t ip = 0; ip < paramc; ip++) {
    value_t param = get_array_at(params, ip);
    value_t tags = get_parameter_ast_tags(param);
    for (int64_t it = 0; it < get_array_length(tags); it++) {
      value_t candidate = get_array_at(tags, it);
      if (value_identity_compare(candidate, tag)) {
        // Found it among the parameters!
        value_t argmap = get_reified_arguments_argmap(self);
        size_t eval_index = (size_t) get_integer_value(get_array_at(argmap, ip));
        *index_out = eval_index;
        return true;
      }
    }
  }

  return false;
}

static value_t reified_arguments_get_at(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofReifiedArguments, self);
  value_t tag = get_builtin_argument(args, 0);
  size_t index = 0;
  if (!reified_arguments_get_value_index(self, tag, &index))
    ESCAPE_BUILTIN(args, no_such_tag, tag);
  value_t values = get_reified_arguments_values(self);
  return get_array_at(values, index);
}

static value_t reified_arguments_replace_argument(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofReifiedArguments, self);
  value_t tag = get_builtin_argument(args, 0);
  size_t index = 0;
  if (!reified_arguments_get_value_index(self, tag, &index))
    ESCAPE_BUILTIN(args, no_such_tag, tag);
  value_t old_values = get_reified_arguments_values(self);
  runtime_t *runtime = get_builtin_runtime(args);
  // Found the index to replace. Clone the values array and replace the value.
  value_t new_value = get_builtin_argument(args, 1);
  size_t argc = (size_t) get_array_length(old_values);
  TRY_DEF(new_values, new_heap_array(runtime, argc));
  value_array_copy_to(get_array_elements(new_values), get_array_elements(old_values));
  set_array_at(new_values, index, new_value);
  return new_heap_reified_arguments(runtime, get_reified_arguments_params(self),
      new_values, get_reified_arguments_argmap(self), get_reified_arguments_tags(self));
}

value_t add_reified_arguments_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL_MAY_ESCAPE("reified_arguments[]", 1, 1, reified_arguments_get_at);
  ADD_BUILTIN_IMPL_MAY_ESCAPE("reified_arguments.replace_argument", 2, 1,
      reified_arguments_replace_argument);
  return success();
}


/// ## Incoming request thunk
///
/// All the data required to run an externally initiated request to a value
/// in this process.

FIXED_GET_MODE_IMPL(incoming_request_thunk, vmMutable);
TRIVIAL_PRINT_ON_IMPL(IncomingRequestThunk, incoming_request_thunk);
GET_FAMILY_PRIMARY_TYPE_IMPL(incoming_request_thunk);

ACCESSORS_IMPL(IncomingRequestThunk, incoming_request_thunk,
    snInFamilyOpt(ofExportedService), Service, service);
ACCESSORS_IMPL(IncomingRequestThunk, incoming_request_thunk,
    snInFamilyOrNull(ofReifiedArguments), Request, request);
ACCESSORS_IMPL(IncomingRequestThunk, incoming_request_thunk,
    snInFamilyOrNull(ofPromise), Promise, promise);

value_t incoming_request_thunk_validate(value_t self) {
  VALIDATE_FAMILY(ofIncomingRequestThunk, self);
  VALIDATE_FAMILY_OPT(ofExportedService, get_incoming_request_thunk_service(self));
  VALIDATE_FAMILY_OR_NULL(ofReifiedArguments, get_incoming_request_thunk_request(self));
  VALIDATE_FAMILY_OR_NULL(ofPromise, get_incoming_request_thunk_promise(self));
  return success();
}

static value_t incoming_request_thunk_handler(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofIncomingRequestThunk, self);
  value_t service = get_incoming_request_thunk_service(self);
  return is_nothing(service) ? null() : get_exported_service_handler(service);
}

static value_t incoming_request_thunk_module(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofIncomingRequestThunk, self);
  value_t service = get_incoming_request_thunk_service(self);
  return is_nothing(service) ? null() : get_exported_service_module(service);
}

static value_t incoming_request_thunk_request(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofIncomingRequestThunk, self);
  return get_incoming_request_thunk_request(self);
}

static value_t incoming_request_thunk_promise(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofIncomingRequestThunk, self);
  return get_incoming_request_thunk_promise(self);
}

static value_t incoming_request_thunk_clear(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofIncomingRequestThunk, self);
  set_incoming_request_thunk_service(self, nothing());
  set_incoming_request_thunk_request(self, null());
  set_incoming_request_thunk_promise(self, null());
  return null();
}

value_t add_incoming_request_thunk_builtin_implementations(runtime_t *runtime,
    safe_value_t s_map) {
  ADD_BUILTIN_IMPL("incoming_request_thunk.handler", 0, incoming_request_thunk_handler);
  ADD_BUILTIN_IMPL("incoming_request_thunk.module", 0, incoming_request_thunk_module);
  ADD_BUILTIN_IMPL("incoming_request_thunk.request", 0, incoming_request_thunk_request);
  ADD_BUILTIN_IMPL("incoming_request_thunk.promise", 0, incoming_request_thunk_promise);
  ADD_BUILTIN_IMPL("incoming_request_thunk.clear!", 0, incoming_request_thunk_clear);
  return success();
}
