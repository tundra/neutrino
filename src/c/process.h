//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// # Process and execution objects
///
/// Types related to process execution are defined in this header. The code that
/// actually executes bytecode lives in {{interp.h}}.
///
/// The aim has been to have a few different execution mechanisms that work as
/// a foundation for the control abstractions provided by the surface language.
/// The main abstractions are:
///
///   - Segmented {{#Stack}}(Stacks). A stack segment is known as a
///     {{#StackPiece}}(stack piece). A stack segment is basically a dumb object
///     array, both of these types have very little functionality on their own.
///   - The {{#Frame}}(frame) type provides access to a segmented stack, both
///     for inspection and modification. This is a stack-allocated type, not
///     heap allocated.
///   - {{#Escape}}(Escapes) capture the information you need to do a non-local
///     return. They can be thought of as pointers into the stack which allow
///     you top abort execution and jump back to the place they're pointing to.
///     If you know `setjmp`/`longjmp` from
///     [setjmp.h](http://en.wikipedia.org/wiki/Setjmp.h), this is basically the
///     same thing.
///   - Various closures: {{#Lambda}}(lambdas), {{#Block}}(blocks), and
///     {{#CodeShard}}(code shards). These are similar but work slightly differently:
///     lambdas and blocks are visible at the surface language level; they're
///     different in that lambdas can outlive their scope so they have to
///     capture the values they close over whereas blocks are killed when their
///     scope exits and so can access outer values directly through the stack.
///     Code shards are like blocks but are not visible on the surface so they
///     don't need to be killed because the runtime knows they won't outlive
///     their scope.

#ifndef _PROCESS
#define _PROCESS

#include "value-inl.h"

/// ## Stack piece
///
/// A stack piece is a contiguous segment of a segmented call stack. The actual
/// backing store of a stack piece is just an array. All values on the stack are
/// properly tagged so a stack piece can be gc'ed trivially.
///
/// ### Open vs. closed
///
/// A stack piece can be either _open_ or _closed_. All interaction with a stack
/// piece happens through a `frame_t` which holds information about the piece's
/// top frame. Opening a stack piece means writing information about the top
/// frame into a `frame_t`. Closing it means writing the information back into
/// the piece. Saving information about the current top frame also happens when
/// invoking a method and the same code is responsible for doing both. The fake
/// frame that is at the top of a closed stack piece is called the _lid_.

static const size_t kStackPieceSize = OBJECT_SIZE(3);
static const size_t kStackPieceStorageOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kStackPiecePreviousOffset= OBJECT_FIELD_OFFSET(1);
static const size_t kStackPieceLidFramePointerOffset = OBJECT_FIELD_OFFSET(2);

// The plain array used for storage for this stack piece.
ACCESSORS_DECL(stack_piece, storage);

// The previous, lower, stack piece.
ACCESSORS_DECL(stack_piece, previous);

// The frame pointer for the lid frame. Only set if the piece is closed.
ACCESSORS_DECL(stack_piece, lid_frame_pointer);

// Returns true if the given stack piece is in the closed state.
bool is_stack_piece_closed(value_t self);

// Flags that describe a stack frame.
typedef enum {
  // This is a maintenance frame inserted by the runtime.
  ffSynthetic        = 0x01,
  // This is a bottom frame of a stack piece, the one that returns to the
  // previous stack piece.
  ffStackPieceBottom = 0x02,
  // This isn't a real frame but the initial state of a stack piece that has
  // no frames.
  ffStackPieceEmpty  = 0x04,
  // This is the bottom frame of a stack, the one that ends execution on that
  // stack.
  ffStackBottom      = 0x08,
  // This is an organic stack frame generated by a method invocation.
  ffOrganic          = 0x10,
  // This frame is the lid of a closed stack piece.
  ffLid              = 0x20
} frame_flag_t;


/// ## Frame

// A transient stack frame. The current structure isn't clever but it's not
// intended to be, it's intended to work and be fully general.
typedef struct frame_t {
  // Pointer to the next available field on the stack.
  value_t *stack_pointer;
  // Pointer to the bottom of the stack fields.
  value_t *frame_pointer;
  // The limit beyond which no more data can be written for this frame.
  value_t *limit_pointer;
  // The flags describing this frame.
  value_t flags;
  // The stack piece that contains this frame.
  value_t stack_piece;
  // The program counter.
  size_t pc;
} frame_t;

// The number of word-size fields in a frame.
#define kFrameFieldCount 5

// The number of words in a stack frame header.
static const size_t kFrameHeaderSize
    = (kFrameFieldCount - 2)  // The frame fields minus the stack pointer which
                              //   is implicit and the stack piece.
    + 1                       // The code block
    + 1                       // The PC
    + 1;                      // The argument map

// Offsets _down_ from the frame pointer to the header fields.
static const size_t kFrameHeaderPreviousFramePointerOffset = 0;
static const size_t kFrameHeaderPreviousLimitPointerOffset = 1;
static const size_t kFrameHeaderPreviousFlagsOffset = 2;
static const size_t kFrameHeaderPreviousPcOffset = 3;
static const size_t kFrameHeaderCodeBlockOffset = 4;
static const size_t kFrameHeaderArgumentMapOffset = 5;

// Tries to allocate a new frame above the given frame of the given capacity.
// Returns true iff allocation succeeds.
bool try_push_new_frame(frame_t *frame, size_t capacity, uint32_t flags,
    bool is_lid);

// Puts the given stack piece in the open state and stores the state required to
// interact with it in the given frame struct.
void open_stack_piece(value_t piece, frame_t *frame);

// Records the state stored in the given frame in its stack piece and closes
// the stack piece.
void close_frame(frame_t *frame);

// Pops the given frame off, storing the next frame. The stack piece that holds
// the given frame must have more frames, otherwise the behavior is undefined
// (or fails when checks are enabled).
void frame_pop_within_stack_piece(frame_t *frame);

// Record the frame pointer for the previous stack frame, the one below this one.
void frame_set_previous_frame_pointer(frame_t *frame, size_t value);

// Returns the frame pointer for the previous stack frame, the one below this
// one.
size_t frame_get_previous_frame_pointer(frame_t *frame);

// Record the limit pointer of the previous stack frame.
void frame_set_previous_limit_pointer(frame_t *frame, size_t value);

// Returns the limit pointer of the previous stack frame.
size_t frame_get_previous_limit_pointer(frame_t *frame);

// Record the flags of the previous stack frame.
void frame_set_previous_flags(frame_t *frame, value_t flags);

// Returns the flags of the previous stack frame.
value_t frame_get_previous_flags(frame_t *frame);

// Sets the code block this frame is executing.
void frame_set_code_block(frame_t *frame, value_t code_block);

// Returns the code block this frame is executing.
value_t frame_get_code_block(frame_t *frame);

// Sets the program counter for this frame.
void frame_set_previous_pc(frame_t *frame, size_t pc);

// Returns the program counter for this frame.
size_t frame_get_previous_pc(frame_t *frame);

// Sets the mapping from parameters to argument indices for this frame.
void frame_set_argument_map(frame_t *frame, value_t map);

// Returns the mapping from parameter to argument indices for this frame.
value_t frame_get_argument_map(frame_t *frame);

// Pushes a value onto this stack frame. The returned value will always be
// success except on bounds check failures in soft check failure mode where it
// will be OutOfBounds.
value_t frame_push_value(frame_t *frame, value_t value);

// Pops a value off this stack frame. Bounds checks whether there is a value to
// pop and in soft check failure mode returns an OutOfBounds condition if not.
value_t frame_pop_value(frame_t *frame);

// Returns the index'th value counting from the top of this stack. Bounds checks
// whether there is a value to return and in soft check failure mode returns an
// OutOfBounds condition if not.
value_t frame_peek_value(frame_t *frame, size_t index);

// Returns the value of the index'th parameter.
value_t frame_get_argument(frame_t *frame, size_t param_index);

// Returns the value of the index'th local variable in this frame.
value_t frame_get_local(frame_t *frame, size_t index);

// Is this frame synthetic, that is, does it correspond to an activation
// inserted by the runtime and not caused by an invocation?
bool frame_has_flag(frame_t *frame, frame_flag_t flag);

// Returns a pointer to the bottom of the stack piece on which this frame is
// currently executing.
value_t *frame_get_stack_piece_bottom(frame_t *frame);

// Returns a pointer to te top of the stack piece which this frame is currently
// executing.
value_t *frame_get_stack_piece_top(frame_t *frame);

// Points the frame struct to the next frame on the stack without writing to the
// stack.
void frame_walk_down_stack(frame_t *frame);

// Creates a joint refraction point and barrier in the given frame that refracts
// for the given refractor object.
void frame_push_refracting_barrier(frame_t *frame, value_t refractor, value_t data);

// Pops a refraction point off the stack. Returns the refractor that created
// the point.
value_t frame_pop_refraction_point(frame_t *frame);

// Pops a joint refraction point and barrier off the stack. Returns the
// refractor that created the point.
value_t frame_pop_refracting_barrier(frame_t *frame);

// Pops the barrier part off a refracting barrier.
void frame_pop_partial_barrier(frame_t *frame);


/// ### Frame iterator
///
/// Utility for scanning through the frames in a stack without mutating the
/// stack.

// Data used while iterating the frames of a stack.
typedef struct {
  // The currently active frame.
  frame_t current;
} frame_iter_t;

// Initializes the given frame iterator. After this call the current frame will
// be the one passed as an argument.
void frame_iter_init_from_frame(frame_iter_t *iter, frame_t *frame);

// Returns the current frame. The result is well-defined until the first call to
// frame_iter_advance that returns false.
frame_t *frame_iter_get_current(frame_iter_t *iter);

// Advances the iterator to the next frame. Returns true iff advancing was
// successful, in which case frame_iter_get_current can be called to get the
// next frame.
bool frame_iter_advance(frame_iter_t *iter);


/// ## Stack
///
/// Most of the execution works with individual stack pieces, not the stack, but
/// but whenever we need to manipulate the pieces, create new ones for instance,
/// that's the stack's responsibility.
///
/// The stack is also responsible for the _barriers_ that are threaded all the
/// way through the stack pieces. A barrier is a section of a stack piece that
/// must be processed before the scope that created the barrier exits. During
/// normal execution the scope's code ensures that the barrier is processed
/// whereas when escaping the barriers are traversed and processed from the
/// outside. This is how ensure blocks are implemented: a barrier is pushed that
/// when processed executes the code corresponding to the ensure block.
///
/// Each barrier contains a pointer to the next one so the stack only needs to
/// keep track of the top one and it'll point to the others. That's convenient
/// because it means that all the space required to hold the barriers can be
/// stack allocated.

static const size_t kStackSize = OBJECT_SIZE(4);
static const size_t kStackTopPieceOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kStackDefaultPieceCapacityOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kStackTopBarrierPieceOffset = OBJECT_FIELD_OFFSET(2);
static const size_t kStackTopBarrierPointerOffset = OBJECT_FIELD_OFFSET(3);

// The top stack piece of this stack.
ACCESSORS_DECL(stack, top_piece);

// The default capacity of the stack pieces that make up this stack.
INTEGER_ACCESSORS_DECL(stack, default_piece_capacity);

// The stack piece that holds the current top barrier.
ACCESSORS_DECL(stack, top_barrier_piece);

// Pointer to the location in the top barrier piece where the top barrier is.
ACCESSORS_DECL(stack, top_barrier_pointer);

// Allocates a new frame on this stack. If allocating fails, for instance if a
// new stack piece is required and we're out of memory, a condition is returned.
// The arg map array is used to determine how many arguments should be copied
// from the old to the new segment in the case where we have to create a new
// one. It is passed as a value rather than a size because the value is easily
// available wherever this gets called and it saves reading the size in the
// common case where no new segment gets allocated. If you're _absolutely_ sure
// no new segment will be allocated you can pass null for the arg map.
value_t push_stack_frame(runtime_t *runtime, value_t stack, frame_t *frame,
    size_t frame_capacity, value_t arg_map);

// Pops frames off the given stack until one is reached that has one of the
// given flags set. There must be such a frame on the stack.
void drop_to_stack_frame(value_t stack, frame_t *frame, frame_flag_t flags);

// Opens the top stack piece of the given stack into the given frame.
frame_t open_stack(value_t stack);


/// ### Stack barrier
///
/// Helper type that encapsulates a region of the stack that holds a stack
/// barrier.

typedef struct {
  value_t *bottom;
} stack_barrier_t;

static const int32_t kStackBarrierSize = 3;
static const size_t kStackBarrierHandlerOffset = 0;
static const size_t kStackBarrierNextPieceOffset = 1;
static const size_t kStackBarrierNextPointerOffset = 2;

// Returns the handler value for this barrier that determines what the barrier
// actually does.
value_t stack_barrier_get_handler(stack_barrier_t *barrier);

// The stack piece of the next barrier.
value_t stack_barrier_get_next_piece(stack_barrier_t *barrier);

// The stack pointer of the next barrier.
value_t stack_barrier_get_next_pointer(stack_barrier_t *barrier);


/// ## Escape

static const size_t kEscapeSize = OBJECT_SIZE(3);
static const size_t kEscapeIsLiveOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kEscapeStackPieceOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kEscapeStackPointerOffset = OBJECT_FIELD_OFFSET(2);

// The number of stack entries it takes to record the complete state of a frame.
static const size_t kCapturedStateSize
    = (kFrameFieldCount - 1) // Everything from the frame except the stack piece.
    + 1;                     // The PC.

// Is it valid to invoke this escape, that is, are we still within the body of
// the with_escape block that produced this escape?
ACCESSORS_DECL(escape, is_live);

// The stack piece to drop to when escaping.
ACCESSORS_DECL(escape, stack_piece);

// The stack pointer that indicates where the stored state is located on the
// stack piece.
ACCESSORS_DECL(escape, stack_pointer);


/// ## Lambda
///
/// Long-lived (or potentially long lived) code objects that are visible to the
/// surface language. Because a lambda can live indefinitely there's no
/// guarantee that it won't be called after the scope that defines its outer
/// state has exited. Hence, pessimistically capture all their outer state on
/// construction.

static const size_t kLambdaSize = OBJECT_SIZE(2);
static const size_t kLambdaMethodsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kLambdaCapturesOffset = OBJECT_FIELD_OFFSET(1);

// Returns the method space where the methods supported by this lambda live.
ACCESSORS_DECL(lambda, methods);

// Returns the array of captured outers for this lambda.
ACCESSORS_DECL(lambda, captures);

// Returns the index'th outer value captured by the given lambda.
value_t get_lambda_capture(value_t self, size_t index);


/// ## Block
///
/// Code objects visible at the surface level which only live as long as the
/// scope that defined them. Because blocks can only be executed while the scope
/// is still alive they can access their outer state through the stack through
/// a mechanism called _refraction_. Whenever a block is created a section is
/// pushed onto the stack, the block's _home_.
///
//%                                     (block 1)
//%           :            :           +----------+
//%           +============+     +---  |   home   |
//%     +---  |     fp     |     |     +----------+
//%     |     +------------+     |
//%     |     |   methods  |     |
//%     |     +------------+     |
//%     |     |   block 1  |  <--+
//%     |     +============+
//%     |     :            :
//%     |     :   locals   :
//%     |     :            :
//%     +-->  +============+
//%           |   subject  |  ---+
//%           +------------+     |
//%           :  possibly  :     |
//%           :    many    :     |      (block 2)
//%           :   frames   :     +-->  +----------+
//%           +============+           |   home   |
//%     +---  |     fp     |     +---  +----------+
//%     |     +------------+     |
//%     |     |   methods  |     |
//%     |     +------------+     |
//%     |     |   block 2  |  <--+
//%     |     +============+
//%     |     :            :
//%     |     :   locals   :
//%     |     :            :
//%     +-->  +============+
///
/// To access say a local variable in an enclosing scope from a block you follow
/// the block's pointer back to its home section. There the frame pointer of the
/// frame that created the block which is all you need to access state in that
/// frame. To access scopes yet further down the stack, in the case where you
/// have nested blocks within blocks, you can peel off the scopes one at a time
/// by first going through the block itself into the frame that created it, then
/// the next enclosing block which will be the subject of that frame, and so on
/// until all the layers of blocks scopes have been peeled off. This process of
/// going through (potentially) successive blocks' originating scopes is what
/// is referred to as refraction.
///
/// Because blocks don't need to capture state they're cheap to create, just
/// a few pushes and then a fixed-size object that points into the stack with
/// a flag that can be used to disable the block when its scope exits.
///
/// ### Block barriers
///
/// Because blocks need to be killed when their scope exits there has to be a
/// stack barrier for each block that ensure that this happens whichever way the
/// scope exits. Blocks also need a refraction point which is also pushed onto
/// the stack and which contains some of the state also needed by the barrier.
/// To avoid pushing the same state multiple times barriers and refraction
/// points are laid out such that they can share state:
///
//%           :            :
//%           +------------+
//%   N+2     |  prev ptr  | --+
//%           +------------+   |
//%   N+1     | prev piece |   |  barrier
//%           +------------+   |
//%    N      |    block   | <-+ <-+
//%           +------------+       |
//%   N-1     |    data    |       |  refraction point
//%           +------------+       |
//%   N-2     |     fp     |     --+
//%           +------------+
//%           :            :
///
/// This is why refraction points count fields from the top whereas barriers
/// count them from the bottom. The same trick applies with code shards.
///
/// The reason the barrier is above the refraction point is such that it can
/// be removed before calling the block when exiting the scope normally.
/// Otherwise if the block fired an escape while the barrier was still in place
/// you'd just end up calling the same block again (and potentially endlessly).


static const size_t kBlockSize = OBJECT_SIZE(3);
// These two must be at the same offsets as the other refractors, currently
// code shards.
static const size_t kBlockHomeStackPieceOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kBlockHomeStatePointerOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kBlockIsLiveOffset = OBJECT_FIELD_OFFSET(2);

// Returns the flag indicating whether this block is still live.
ACCESSORS_DECL(block, is_live);

// Returns the array of outer variables for this block.
ACCESSORS_DECL(block, home_stack_piece);

// Returns the array of outer variables for this block.
ACCESSORS_DECL(block, home_state_pointer);

/// ### Refractor accessors
///
/// Because blocks and code shards are so similar it's convenient in a few
/// places to be able to interact with them without knowing which of the two
/// you've got. The refractor methods work on both types because the objects
/// offsets that hold the home pointer are in the same positions in both types
/// of objects.

struct frame_t;

// Returns an incomplete frame that provides access to arguments and locals for
// the frame that is located block_depth scopes outside the given block.
void get_refractor_refracted_frame(value_t self, size_t block_depth,
    struct frame_t *frame_out);

// Returns the home stack piece field of a refractor, that is, either a code
// shard or a block.
value_t get_refractor_home_stack_piece(value_t value);

// Returns the home state pointer of a refractor, that is, either a code shard
// or a block.
value_t get_refractor_home_state_pointer(value_t self);

// Sets the home state pointer of a refractor.
void set_refractor_home_state_pointer(value_t self, value_t value);


/// ## Code shard
///
/// Code shards are scoped closures like blocks but are internal to the runtime
/// so they don't need as many safeguards. They also can't be called with
/// different methods, they have just one block of code which will always be
/// the one to be called. This, for instance, is how ensure-blocks work.
///
/// Code shards use refraction exactly the same way blocks do.

static const size_t kCodeShardSize = OBJECT_SIZE(2);
// These two must be at the same offsets as the other refractors, currently
// blocks.
static const size_t kCodeShardHomeStackPieceOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kCodeShardHomeStatePointerOffset = OBJECT_FIELD_OFFSET(1);

// Returns the stack piece that contains this code shard's home.
ACCESSORS_DECL(code_shard, home_stack_piece);

// Returns a pointer within the stack piece to where this code shard's home is.
ACCESSORS_DECL(code_shard, home_state_pointer);


/// ## Refraction points
///
/// Refraction points are the areas of the stack through which refractors look
/// up their outer variables. The code for working with refraction points gets
/// used in a few different places so it's convenient to wrap it in an explicit
/// abstraction.

typedef struct {
  // Base pointer into the stack where the refraction point's state is.
  value_t *top;
} refraction_point_t;

static const int32_t kRefractionPointSize = 3;
static const size_t kRefractionPointRefractorOffset = 0;
static const size_t kRefractionPointDataOffset = 1;
static const size_t kRefractionPointFramePointerOffset = 2;

static const size_t kRefractingBarrierOverlapSize = 1;

// Are you serious that this has to be a macro!?!
#define kRefractingBarrierSize ((int32_t) (kRefractionPointSize + kStackBarrierSize - kRefractingBarrierOverlapSize))

// Returns the home refraction point for the given refractor.
refraction_point_t get_refractor_home(value_t self);

// Returns the extra data that was specified when the refraction point was
// created.
value_t get_refraction_point_data(refraction_point_t *point);

// Returns the frame pointer of the frame that contains the given refraction
// point.
size_t get_refraction_point_frame_pointer(refraction_point_t *point);

// Returns the refractor object (block or code shard) whose creation caused this
// refraction point.
value_t get_refraction_point_refractor(refraction_point_t *point);


// --- B a c k t r a c e ---

static const size_t kBacktraceSize = OBJECT_SIZE(1);
static const size_t kBacktraceEntriesOffset = OBJECT_FIELD_OFFSET(0);

// The array buffer of backtrace entries.
ACCESSORS_DECL(backtrace, entries);

// Creates a new backtrace by traversing the stack starting from the given
// frame.
value_t capture_backtrace(runtime_t *runtime, frame_t *frame);


// --- B a c k t r a c e   e n t r y ---

static const size_t kBacktraceEntrySize = OBJECT_SIZE(2);
static const size_t kBacktraceEntryInvocationOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kBacktraceEntryIsSignalOffset = OBJECT_FIELD_OFFSET(1);

// The invocation record for this entry.
ACCESSORS_DECL(backtrace_entry, invocation);

// Is this backtrace entry the result of a signal being raised or a normal call?
ACCESSORS_DECL(backtrace_entry, is_signal);

// Print the given invocation map on the given context. This is really an
// implementation detail of how backtrace entries print themselves but it's
// tricky enough that it makes sense to be able to test as a separate thing.
void backtrace_entry_invocation_print_on(value_t invocation, bool is_signal,
    print_on_context_t *context);

// Creates a backtrace entry from the given stack frame. If no entry can be
// created nothing is returned.
value_t capture_backtrace_entry(runtime_t *runtime, frame_t *frame);


#endif // _PROCESS
