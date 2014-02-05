// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "log.h"
#include "process.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "value-inl.h"


// --- S t a c k   p i e c e ---

FIXED_GET_MODE_IMPL(stack_piece, vmMutable);

ACCESSORS_IMPL(StackPiece, stack_piece, acInFamily, ofArray, Storage, storage);
ACCESSORS_IMPL(StackPiece, stack_piece, acInFamilyOpt, ofStackPiece, Previous, previous);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopFramePointer, top_frame_pointer);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopStackPointer, top_stack_pointer);
INTEGER_ACCESSORS_IMPL(StackPiece, stack_piece, TopCapacity, top_capacity);
ACCESSORS_IMPL(StackPiece, stack_piece, acInPhylum, tpFlagSet, TopFlags, top_flags);

value_t stack_piece_validate(value_t value) {
  VALIDATE_FAMILY(ofStackPiece, value);
  VALIDATE_FAMILY(ofArray, get_stack_piece_storage(value));
  VALIDATE_FAMILY_OPT(ofStackPiece, get_stack_piece_previous(value));
  VALIDATE_PHYLUM(tpFlagSet, get_stack_piece_top_flags(value));
  return success();
}

void stack_piece_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofStackPiece, value);
  string_buffer_printf(context->buf, "#<stack piece ~%w: st@%i>", value,
      get_array_length(get_stack_piece_storage(value)));
}


// --- S t a c k   ---

FIXED_GET_MODE_IMPL(stack, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Stack, stack);

ACCESSORS_IMPL(Stack, stack, acInFamily, ofStackPiece, TopPiece, top_piece);
INTEGER_ACCESSORS_IMPL(Stack, stack, DefaultPieceCapacity, default_piece_capacity);

value_t stack_validate(value_t value) {
  VALIDATE_FAMILY(ofStack, value);
  VALIDATE_FAMILY(ofStackPiece, get_stack_top_piece(value));
  return success();
}

// Transfers the arguments from the top of the previous piece (which the frame
// points to) to the bottom of the new stack segment.
static void transfer_top_arguments(value_t new_piece, frame_t *frame,
    size_t arg_count) {
  value_t storage = get_stack_piece_storage(new_piece);
  size_t old_sp = get_stack_piece_top_stack_pointer(new_piece);
  for (size_t i = 0; i < arg_count; i++) {
    value_t value = frame_peek_value(frame, arg_count - i - 1);
    set_array_at(storage, old_sp + i, value);
  }
  set_stack_piece_top_stack_pointer(new_piece, old_sp + arg_count);
}

static void push_stack_piece_bottom_frame(runtime_t *runtime, value_t stack_piece,
    size_t arg_count) {
  frame_t bottom;
  value_t code_block = ROOT(runtime, stack_piece_bottom_code_block);
  // The transferred arguments are going to appear as if they were arguments
  // passed from this frame so we have to "allocate" enough room for them on
  // the stack.
  bool pushed = try_push_stack_piece_frame(stack_piece, &bottom,
      get_code_block_high_water_mark(code_block) + arg_count,
      ffSynthetic | ffStackPieceBottom);
  CHECK_TRUE("pushing bottom frame", pushed);
  set_frame_code_block(&bottom, code_block);
}

// Sets the given frame to be the top frame of the given stack piece.
static void get_top_stack_piece_frame(value_t stack_piece, frame_t *frame) {
  CHECK_FAMILY(ofStackPiece, stack_piece);
  frame->stack_piece = stack_piece;
  frame->frame_pointer = get_stack_piece_top_frame_pointer(stack_piece);
  frame->stack_pointer = get_stack_piece_top_stack_pointer(stack_piece);
  frame->capacity = get_stack_piece_top_capacity(stack_piece);
  frame->flags = get_stack_piece_top_flags(stack_piece);
}

// Records the current registers in the given frame in the stack piece.
static void set_stack_piece_top_frame(frame_t *frame) {
  value_t piece = frame->stack_piece;
  set_stack_piece_top_stack_pointer(piece, frame->stack_pointer);
  set_stack_piece_top_frame_pointer(piece, frame->frame_pointer);
  set_stack_piece_top_capacity(piece, frame->capacity);
  set_stack_piece_top_flags(piece, frame->flags);
}

value_t push_stack_frame(runtime_t *runtime, value_t stack, frame_t *frame,
    size_t frame_capacity, value_t arg_map) {
  CHECK_FAMILY(ofStack, stack);
  value_t top_piece = get_stack_top_piece(stack);
  if (!try_push_stack_piece_frame(top_piece, frame, frame_capacity, ffOrganic)) {
    // There wasn't room to push this frame onto the top stack piece so
    // allocate a new top piece that definitely has room.
    size_t default_capacity = get_stack_default_piece_capacity(stack);
    size_t transfer_arg_count = get_array_length(arg_map);
    size_t required_capacity
        = frame_capacity      // the new frame's locals
        + kFrameHeaderSize    // the new frame's header
        + 1                   // the synthetic bottom frame's one local
        + kFrameHeaderSize    // the synthetic bottom frame's header
        + transfer_arg_count; // any arguments to be copied onto the piece
    size_t new_capacity = max_size(default_capacity, required_capacity);
    // Create and initialize the new stack segment. The frame struct is still
    // pointing to the old frame.
    TRY_DEF(new_piece, new_heap_stack_piece(runtime, new_capacity, top_piece));
    push_stack_piece_bottom_frame(runtime, new_piece, transfer_arg_count);
    transfer_top_arguments(new_piece, frame, transfer_arg_count);
    set_stack_top_piece(stack, new_piece);
    // Finally, create a new frame on the new stack which includes updating the
    // struct. The required_capacity calculation ensures that this call will
    // succeed.
    bool pushed_stack_piece = try_push_stack_piece_frame(new_piece,
        frame, frame_capacity, ffOrganic);
    CHECK_TRUE("pushing on new piece failed", pushed_stack_piece);
  }
  set_frame_argument_map(frame, arg_map);
  return success();
}

static void frame_walk_down_stack(frame_t *frame) {
  frame_t snapshot = *frame;
  // Get the frame pointer and capacity from the frame's header.
  frame->frame_pointer = get_frame_previous_frame_pointer(&snapshot);
  frame->capacity = get_frame_previous_capacity(&snapshot);
  frame->flags = get_frame_previous_flags(&snapshot);
  // The stack pointer will be the first field of the top frame's header.
  frame->stack_pointer = snapshot.frame_pointer - kFrameHeaderSize;
}

void drop_to_stack_frame(value_t stack, frame_t *frame, frame_flag_t flags) {
  value_t piece = get_stack_top_piece(stack);
  get_top_stack_piece_frame(piece, frame);
  frame_loop: while (true) {
    CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
    frame_walk_down_stack(frame);
    if (frame_has_flag(frame, ffStackPieceEmpty)) {
      // If we're at the bottom of a stack piece walk down another frame to
      // get to the next one.
      piece = get_stack_piece_previous(piece);
      CHECK_FALSE("bottom of stack", is_nothing(piece));
      set_stack_top_piece(stack, piece);
      get_top_stack_piece_frame(piece, frame);
      CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
    }
    if (!frame_has_flag(frame, flags)) {
      goto frame_loop;
    } else {
      set_stack_piece_top_frame(frame);
      return;
    }
  }
}

bool frame_has_flag(frame_t *frame, frame_flag_t flag) {
  return get_flag_set_at(frame->flags, flag);
}

void get_stack_top_frame(value_t stack, frame_t *frame) {
  CHECK_FAMILY(ofStack, stack);
  value_t top_piece = get_stack_top_piece(stack);
  get_top_stack_piece_frame(top_piece, frame);
}


// --- F r a m e ---

bool try_push_stack_piece_frame(value_t stack_piece, frame_t *frame,
    size_t frame_capacity, uint32_t flags) {
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
  frame->flags = new_flag_set(flags);
  frame->stack_piece = stack_piece;
  // Record the relevant information about the previous frame in the new frame's
  // header.
  set_frame_previous_frame_pointer(frame, old_frame.frame_pointer);
  set_frame_previous_capacity(frame, old_frame.capacity);
  set_frame_previous_flags(frame, old_frame.flags);
  set_frame_pc(frame, 0);
  set_frame_code_block(frame, whatever());
  set_frame_argument_map(frame, whatever());
  // Update the stack piece's top frame data to reflect the new top frame.
  set_stack_piece_top_frame(frame);
  return true;
}

void pop_stack_piece_frame(value_t stack_piece, frame_t *frame) {
  get_top_stack_piece_frame(stack_piece, frame);
  CHECK_FALSE("stack piece empty", frame_has_flag(frame, ffStackPieceEmpty));
  frame_walk_down_stack(frame);
  set_stack_piece_top_frame(frame);
}

// Accesses a frame header field, that is, a bookkeeping field below the frame
// pointer.
static value_t *access_frame_header_field(frame_t *frame, size_t offset) {
  CHECK_REL("frame header field out of bounds", offset, <=, kFrameHeaderSize);
  value_t storage = get_stack_piece_storage(frame->stack_piece);
  size_t index = frame->frame_pointer - offset - 1;
  CHECK_REL("frame header out of bounds", index, <, get_array_length(storage));
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
  CHECK_REL("frame field out of bounds", offset, <, get_array_length(storage));
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

void set_frame_previous_flags(frame_t *frame, value_t flags) {
  *access_frame_header_field(frame, kFrameHeaderPreviousFlagsOffset) = flags;
}

value_t get_frame_previous_flags(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderPreviousFlagsOffset);
}

void set_frame_code_block(frame_t *frame, value_t code_block) {
  *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset) =
      code_block;
}

value_t get_frame_code_block(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderCodeBlockOffset);
}

void set_frame_argument_map(frame_t *frame, value_t argument_map) {
  *access_frame_header_field(frame, kFrameHeaderArgumentMapOffset) =
      argument_map;
}

value_t get_frame_argument_map(frame_t *frame) {
  return *access_frame_header_field(frame, kFrameHeaderArgumentMapOffset);
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
  COND_CHECK_TRUE("push on lower frame", ccWat, is_top_frame(frame));
  COND_CHECK_TRUE("push out of frame bounds", ccOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer));
  *access_frame_field(frame, frame->stack_pointer) = value;
  frame->stack_pointer++;
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  return success();
}

value_t frame_pop_value(frame_t *frame) {
  COND_CHECK_TRUE("pop off lower frame", ccWat, is_top_frame(frame));
  COND_CHECK_TRUE("pop out of frame bounds", ccOutOfBounds,
      is_offset_within_frame(frame, frame->stack_pointer - 1));
  frame->stack_pointer--;
  value_t result = *access_frame_field(frame, frame->stack_pointer);
  set_stack_piece_top_stack_pointer(frame->stack_piece, frame->stack_pointer);
  return result;
}

value_t frame_peek_value(frame_t *frame, size_t index) {
  return *access_frame_field(frame, frame->stack_pointer - index - 1);
}

value_t frame_get_argument(frame_t *frame, size_t param_index) {
  size_t stack_pointer;
  value_t storage;
  stack_pointer = frame->frame_pointer - kFrameHeaderSize;
  storage = get_stack_piece_storage(frame->stack_piece);
  value_t arg_map = get_frame_argument_map(frame);
  size_t offset = get_integer_value(get_array_at(arg_map, param_index));
  value_t *elements = get_array_elements(storage);
  return elements[stack_pointer - offset - 1];
}

value_t frame_get_local(frame_t *frame, size_t index) {
  size_t location = frame->frame_pointer + index;
  COND_CHECK_TRUE("local not defined yet", ccOutOfBounds,
      location < frame->stack_pointer);
  return *access_frame_field(frame, location);
}


// --- F r a m e   i t e r a t o r ---

void frame_iter_init_from_stack(frame_iter_t *iter, value_t stack) {
  get_stack_top_frame(stack, &iter->current);
}

void frame_iter_init_from_frame(frame_iter_t *iter, frame_t *frame) {
  iter->current = *frame;
}

frame_t *frame_iter_get_current(frame_iter_t *iter) {
  return &iter->current;
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
      get_top_stack_piece_frame(current->stack_piece, current);
    } else if (frame_has_flag(current, ffStackBottom)) {
      // If we're at the bottom of the stack there are no more frames.
      return false;
    }
  } while (!frame_has_flag(current, ffOrganic));
  // We must have reached an organic frame so return true.
  return true;
}


// --- E s c a p e ---

FIXED_GET_MODE_IMPL(escape, vmMutable);
TRIVIAL_PRINT_ON_IMPL(Escape, escape);
GET_FAMILY_PRIMARY_TYPE_IMPL(escape);

ACCESSORS_IMPL(Escape, escape, acNoCheck, 0, IsLive, is_live);
ACCESSORS_IMPL(Escape, escape, acInFamily, ofStackPiece, StackPiece, stack_piece);
ACCESSORS_IMPL(Escape, escape, acNoCheck, 0, StackPointer, stack_pointer);

value_t escape_validate(value_t value) {
  VALIDATE_FAMILY(ofEscape, value);
  return success();
}

value_t emit_fire_escape(assembler_t *assm) {
  TRY(assembler_emit_fire_escape(assm));
  return success();
}

value_t escape_is_live(builtin_arguments_t *args) {
  value_t self = get_builtin_subject(args);
  CHECK_FAMILY(ofEscape, self);
  return get_escape_is_live(self);
}

value_t add_escape_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  DEF_CALL(call);
  TRY(add_methodspace_custom_method(runtime, deref(s_space),
      ROOT(runtime, escape_type), call, 1, false,
      emit_fire_escape));
  DEF_PROPERTY(property_is_live, "is_live");
  ADD_BUILTIN(escape, property_is_live, 0, escape_is_live);
  return success();
}


// --- B a c k t r a c e ---

FIXED_GET_MODE_IMPL(backtrace, vmMutable);
GET_FAMILY_PRIMARY_TYPE_IMPL(backtrace);

ACCESSORS_IMPL(Backtrace, backtrace, acInFamily, ofArrayBuffer, Entries, entries);

value_t backtrace_validate(value_t value) {
  VALIDATE_FAMILY(ofBacktrace, value);
  return success();
}

value_t add_backtrace_builtin_methods(runtime_t *runtime, safe_value_t s_space) {
  return success();
}

void backtrace_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBacktrace, value);
  string_buffer_printf(context->buf, "--- backtrace ---");
  value_t entries = get_backtrace_entries(value);
  for (size_t i = 0; i < get_array_buffer_length(entries); i++) {
    string_buffer_putc(context->buf, '\n');
    string_buffer_printf(context->buf, "- ");
    value_print_inner_on(get_array_buffer_at(entries, i), context, -1);
  }
}

value_t capture_backtrace(runtime_t *runtime, frame_t *top) {
  TRY_DEF(frames, new_heap_array_buffer(runtime, 16));
  frame_iter_t iter;
  frame_iter_init_from_frame(&iter, top);
  do {
    frame_t *frame = frame_iter_get_current(&iter);
    TRY_DEF(entry, capture_backtrace_entry(runtime, frame));
    if (!is_nothing(entry))
      TRY(add_to_array_buffer(runtime, frames, entry));
  } while (frame_iter_advance(&iter));
  return new_heap_backtrace(runtime, frames);
}


// --- B a c k t r a c e   e n t r y ---

FIXED_GET_MODE_IMPL(backtrace_entry, vmMutable);

ACCESSORS_IMPL(BacktraceEntry, backtrace_entry, acNoCheck, 0, Invocation,
    invocation);

value_t backtrace_entry_validate(value_t value) {
  VALIDATE_FAMILY(ofBacktraceEntry, value);
  return success();
}

void backtrace_entry_invocation_print_on(value_t invocation, print_on_context_t *context) {
  value_t subject = new_not_found_condition();
  value_t selector = new_not_found_condition();
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, invocation);
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    if (in_family(ofKey, key)) {
      size_t id = get_key_id(key);
      if (id == 0)
        subject = value;
      else if (id == 1)
        selector = value;
    }
  }
  // Print the subject as the first thing.
  if (!is_condition(ccNotFound, subject))
    value_print_inner_on(subject, context, -1);
  // Begin the selector.
  if (in_family(ofOperation, selector)) {
    operation_print_open_on(selector, context);
  } else if (!is_condition(ccNotFound, selector)) {
    value_print_inner_on(selector, context, -1);
  }
  // Number of positional arguments.
  size_t posc;
  // Number of arguments in total discounting the subject and selector.
  size_t argc;
  // Print the positional arguments.
  for (posc = argc = 0; true; posc++, argc++) {
    value_t key = new_integer(posc);
    value_t value = get_id_hash_map_at(invocation, key);
    if (is_condition(ccNotFound, value))
      break;
    if (argc > 0)
      string_buffer_printf(context->buf, ", ");
    value_print_inner_on(value, context, -1);
  }
  // Print any remaining arguments. Note that this will print them in
  // nondeterministic order since the order depends on the iteration order of
  // the map. This is bad.
  id_hash_map_iter_init(&iter, invocation);
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    if (in_family(ofKey, key)) {
      size_t id = get_key_id(key);
      if (id == 0 || id == 1)
        // Don't print the subject/selector again.
        continue;
    } else if (in_domain(vdInteger, key) && ((size_t) get_integer_value(key)) < posc) {
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
    value_print_inner_on(key, &new_context, -1);
    string_buffer_printf(context->buf, ": ");
    value_print_inner_on(value, context, -1);
    argc++;
  }
  // End the selector.
  if (in_family(ofOperation, selector))
    operation_print_close_on(selector, context);
}

void backtrace_entry_print_on(value_t value, print_on_context_t *context) {
  CHECK_FAMILY(ofBacktraceEntry, value);
  value_t invocation = get_backtrace_entry_invocation(value);
  backtrace_entry_invocation_print_on(invocation, context);
}

value_t capture_backtrace_entry(runtime_t *runtime, frame_t *frame) {
  // Check whether the program counter stored for this frame points immediately
  // after an invoke instruction. If it does we'll use that instruction to
  // construct the entry.
  value_t code_block = get_frame_code_block(frame);
  value_t bytecode = get_code_block_bytecode(code_block);
  size_t pc = get_frame_pc(frame);
  if (pc <= kInvokeOperationSize)
    return nothing();
  blob_t data;
  get_blob_data(bytecode, &data);
  opcode_t op = blob_short_at(&data, pc - kInvokeOperationSize);
  if (op != ocInvoke)
    return nothing();
  // Okay so we have an invoke we can use. Grab the invocation record.
  size_t record_index = blob_short_at(&data, pc - kInvokeOperationSize + 1);
  value_t value_pool = get_code_block_value_pool(code_block);
  value_t record = get_array_at(value_pool, record_index);
  // Scan through the record to build the invocation map.
  TRY_DEF(invocation, new_heap_id_hash_map(runtime, 16));
  size_t arg_count = get_invocation_record_argument_count(record);
  for (size_t i = 0;  i < arg_count; i++) {
    value_t tag = get_invocation_record_tag_at(record, i);
    value_t arg = get_invocation_record_argument_at(record, frame, i);
    TRY(set_id_hash_map_at(runtime, invocation, tag, arg));
  }
  // Wrap the result in a backtrace entry.
  return new_heap_backtrace_entry(runtime, invocation);
}
