// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Bytecode generation.


#ifndef _CODEGEN
#define _CODEGEN

#include "interp.h"
#include "value.h"


// A word on the terminology used about bindings.
//
//  - An _outer_ binding is one that is defined in an enclosing frame. Outer
//    bindings happen when a lambda or block use a variable from an enclosing
//    scope. Outer is the broadest concept.
//  - A _captured_ binding is a special case of outer bindings. It refers to the
//    implementation technique whereby all a lambda's outer variables are copied
//    into the heap. This way they can survive after their originating scope
//    exits.
//  - A _refracted_ binding is also an outer binding but one that doesn't use
//    copying. Instead, outer variables accessed by blocks are read from their
//    original location on the stack since they are guaranteed to be present
//    when the block runs.
//
// Captured and refracted bindings can be combined: a refracted binding can be
// outer to a lambda and hence captured, and a captured binding can be outer to
// a block and accessed through refraction. The code tries to make the
// distinction as clear as possible by avoiding the term "outer" unless it
// really refers to both types.


// Identifies what kind of binding a bound symbol represents.
typedef enum {
  // A local variable in the current scope.
  btLocal = 0,
  // An argument to the current immediate function.
  btArgument,
  // A symbol captured by an enclosing method.
  btLambdaCaptured
} binding_type_t;

// A collection of information about a binding. This is going to be encoded as
// an int and stored in a tagged integer so it can't be larger than effectively
// 61 bits.
typedef struct {
  // The type of the binding.
  binding_type_t type;
  // Extra data about the binding.
  uint16_t data;
  // Counter that indicates how many layers of blocks to traverse to find the
  // value.
  uint16_t block_depth;
} binding_info_t;

// Sets the fields of a binding info struct.
void binding_info_set(binding_info_t *info, binding_type_t type, uint16_t data,
    uint16_t block_depth);

// A function that performs a scoped lookup. If the info_out argument is NULL
// we're only checking whether the binding exists, not actually accessing it.
// The return value should be a NotFound condition if the symbol could not be
// resolved, a non-condition otherwise.
typedef value_t (*scope_lookup_function_t)(value_t symbol, void *data,
    binding_info_t *info_out);

// A callback that does scoped symbol resolution.
typedef struct {
  // The function that implements the callback.
  scope_lookup_function_t function;
  // Extra data that'll be passed to the function.
  void *data;
} scope_lookup_callback_t;

// Initializes a scope lookup callback.
void scope_lookup_callback_init(scope_lookup_callback_t *callback,
    scope_lookup_function_t function, void *data);

// Invokes a scope lookup callback with a symbol. The result will be stored in
// the info-out parameter. If there's any problem the result will be a condition.
value_t scope_lookup_callback_call(scope_lookup_callback_t *callback,
    value_t symbol, binding_info_t *info_out);

// Returns a "bottom" scope lookup callback that is always empty. This will
// always return the same value.
scope_lookup_callback_t *scope_lookup_callback_get_bottom();

// A block of reusable scratch memory. It can be used to grab a block of memory
// of a given size without worrying about releasing it. Just be sure not to
// have two different users at the same time.
typedef struct {
  // The current memory block.
  memory_block_t memory;
} reusable_scratch_memory_t;

// Initializes a reusable scratch memory block.
void reusable_scratch_memory_init(reusable_scratch_memory_t *memory);

// Disposes this scratch memory block, releasing any memory returned from this
// block. Obviously this invalidates any memory blocks ever returned from this
// block.
void reusable_scratch_memory_dispose(reusable_scratch_memory_t *memory);

// Returns a memory block of the given size backed by the given reusable memory
// block. This invalidates any memory blocks previously returned, so only the
// last block returned can be used. You don't have to explicitly release this
// block, it will be disposed along with the reusable memory block whenever
// it is disposed.
void *reusable_scratch_memory_alloc(reusable_scratch_memory_t *memory,
    size_t size);

// Returns two blocks of memory from a reusable scratch memory block. All the
// same rules apply as with reusable_scratch_malloc. Really this is just a
// shorthand for allocating one block and splitting it in two.
void reusable_scratch_memory_double_alloc(reusable_scratch_memory_t *memory,
    size_t first_size, void **first, size_t second_size, void **second);


// Bytecode assembler data.
typedef struct assembler_t {
  // The runtime we're generating code within.
  runtime_t *runtime;
  // The buffer that holds the code being built.
  short_buffer_t code;
  // The value pool map.
  value_t value_pool;
  // The current stack height.
  size_t stack_height;
  // The highest the stack has been at any point.
  size_t high_water_mark;
  // The callback for resolving local symbols.
  scope_lookup_callback_t *scope_callback;
  // A reusable memory block.
  reusable_scratch_memory_t scratch_memory;
  // The module fragment we're compiling within.
  value_t fragment;
} assembler_t;

// Initializes an assembler. If the given scope callback is NULL it is taken to
// mean that there is no enclosing scope.
value_t assembler_init(assembler_t *assm, runtime_t *runtime, value_t fragment,
    scope_lookup_callback_t *scope_callback);

// Initializes an assembler to the bare minimum required to assembler code with
// no value pool.
value_t assembler_init_stripped_down(assembler_t *assm, runtime_t *runtime);

// Sets the scope callback for the given assembler, returning the previous
// value.
scope_lookup_callback_t *assembler_set_scope_callback(assembler_t *assm,
    scope_lookup_callback_t *scope_callback);

// Disposes of an assembler.
void assembler_dispose(assembler_t *assm);

// Returns a code block object containing the code written to this assembler.
value_t assembler_flush(assembler_t *assm);

// Returns the scratch memory block provided by this assembler. Be sure to
// respect the scratch memory discipline when using this (see the documentation
// for reusable_scratch_memory_t).
reusable_scratch_memory_t *assembler_get_scratch_memory(assembler_t *assm);

// Adds the given delta to the recorded stack height and updates the high water
// mark if necessary.
void assembler_adjust_stack_height(assembler_t *assm, int delta);

// Returns the offset in words of the next location in the code stream which
// will be written, that is, one past the last written instruction.
size_t assembler_get_code_cursor(assembler_t *assm);

// Emits a push instruction.
value_t assembler_emit_push(assembler_t *assm, value_t value);

// Emits a pop instruction. Pops count elements off the stack.
value_t assembler_emit_pop(assembler_t *assm, size_t count);

// Emits a store-local-and-pop instruction. Pops off the top, then pops off
// count additional values, and finally pushes the top back on.
value_t assembler_emit_slap(assembler_t *assm, size_t count);

// Emits a new-array instruction that builds an array from the top 'length'
// elements.
value_t assembler_emit_new_array(assembler_t *assm, size_t length);

// Emits an invocation using the given record.
value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record,
    value_t helper);

// Emits a signal opcode using the given record.
value_t assembler_emit_signal(assembler_t *assm, opcode_t opcode, value_t record);

// Emits a raw call to a builtin with the given implementation which can't cause
// signals.
value_t assembler_emit_builtin(assembler_t *assm, builtin_method_t builtin);

// Emits a raw call to a builtin with the given implementation that may cause
// a leave signal to be returned which requires leave_argc slots on the stack.
value_t assembler_emit_builtin_maybe_leave(assembler_t *assm,
    builtin_method_t builtin, size_t leave_argc,
    short_buffer_cursor_t *leave_offset_out);

// Emits a return instruction.
value_t assembler_emit_return(assembler_t *assm);

// Emits a set-reference instruction.
value_t assembler_emit_set_reference(assembler_t *assm);

// Emits a get-reference instruction.
value_t assembler_emit_get_reference(assembler_t *assm);

// Wraps a reference around the top stack value.
value_t assembler_emit_new_reference(assembler_t *assm);

// Emits a local variable load of the local with the given index.
value_t assembler_emit_load_local(assembler_t *assm, size_t index);

// Emits an outer local load of the local with the given index in the frame
// block_depth nesting levels from the current location.
value_t assembler_emit_load_refracted_local(assembler_t *assm, size_t index,
    size_t block_depth);

// Emits a global variable load of the local with the given name within the
// given module.
value_t assembler_emit_load_global(assembler_t *assm, value_t name,
    value_t module);

// Emits an argument load of the argument with the given parameter index.
value_t assembler_emit_load_argument(assembler_t *assm, size_t param_index);

// Emits an argument load of the argument with the given parameter index from
// the frame block_depth nesting levels from the current location.
value_t assembler_emit_load_refracted_argument(assembler_t *assm,
    size_t param_index, size_t block_depth);

// Emits a load of a captured outer variable in the subject lambda.
value_t assembler_emit_load_lambda_capture(assembler_t *assm, size_t index);

// Emits a load of a captured outer variable in the subject lambda from the
// frame block_depth nesting levels from the current scope.
value_t assembler_emit_load_refracted_capture(assembler_t *assm, size_t index,
    size_t block_depth);

// Emits a lambda that understands the given methods and which expects the given
// number of captured variables to be present on the stack.
value_t assembler_emit_lambda(assembler_t *assm, value_t methods,
    size_t capture_count);

// Emits a block that understands the given methods.
value_t assembler_emit_create_block(assembler_t *assm, value_t methods);

// Emits an ensure block that executes the given block of code.
value_t assembler_emit_create_ensurer(assembler_t *assm, value_t code_block);

// Calls the ensure block below the top stack value.
value_t assembler_emit_call_ensurer(assembler_t *assm);

// Cleans up after a ensure block call.
value_t assembler_emit_dispose_ensurer(assembler_t *assm);

// Installs a methodspace as a scoped signal handler.
value_t assembler_emit_install_signal_handler(assembler_t *assm, value_t space,
    short_buffer_cursor_t *continue_offset_out);

// Uninstalls a methodspace as a scoped signal handler.
value_t assembler_emit_uninstall_signal_handler(assembler_t *assm);

// Hacky implementation of calling lambdas. Later this should be replaced by a
// more general delegate operation.
value_t assembler_emit_delegate_lambda_call(assembler_t *assm);

// Ditto for blocks.
value_t assembler_emit_delegate_block_call(assembler_t *assm);

// Capture an escape, pushing it onto the stack. The offset_out is a cursor
// where the offset to jump to when returning to the escape should be written.
value_t assembler_emit_create_escape(assembler_t *assm,
    short_buffer_cursor_t *offset_out);

// Emits a goto instruction that moves an as yet undetermined amount forward.
value_t assembler_emit_goto_forward(assembler_t *assm,
    short_buffer_cursor_t *offset_out);

// Either fire the next barrier if the current escape lies below it, or fire
// the current escape if there are no more barriers to fire.
value_t assembler_emit_fire_escape_or_barrier(assembler_t *assm);

// Either fire the next barrier if the current signal handler lies below it, or
// leave for there if there are no more barriers to fire.
value_t assembler_emit_leave_or_fire_barrier(assembler_t *assm, size_t argc);

// Pops off the escape currently on the stack and marks it as dead. Note that
// this expects there to be a value above the escape, so this in effect does a
// slap-1, not a pop-1.
value_t assembler_emit_dispose_escape(assembler_t *assm);

// Pops off the local lambda currently on the stack and marks it as dead. Note
// that this expects there to be a value above the lambda, so this in effect
// does a slap-1, not a pop-1.
value_t assembler_emit_dispose_block(assembler_t *assm);

// Emits a stack bottom instruction that indicates that we're done executing.
value_t assembler_emit_stack_bottom(assembler_t *assm);

// Emits a stack piece bottom instruction that indicates that we've reached the
// bottom of one stack piece and should step down to the next piece.
value_t assembler_emit_stack_piece_bottom(assembler_t *assm);


// A scope defining a single symbol.
typedef struct {
  // The symbol.
  value_t symbol;
  // The symbol's binding.
  binding_info_t binding;
  // The callback that performs lookup in this scope.
  scope_lookup_callback_t callback;
  // The enclosing scope.
  scope_lookup_callback_t *outer;
} single_symbol_scope_t;

// Pushes a single symbol scope onto the scope stack.
void assembler_push_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_t *scope, value_t symbol, binding_type_t type,
    uint32_t data);

// Pops a single symbol scope off the scope stack.
void assembler_pop_single_symbol_scope(assembler_t *assm,
    single_symbol_scope_t *scope);


// A scope whose symbols are defined in a hash map.
typedef struct {
  // The map of symbols.
  value_t map;
  // The callback that performs lookup in this scope.
  scope_lookup_callback_t callback;
  // The enclosing scope.
  scope_lookup_callback_t *outer;
  // The assembler this scope belongs to.
  assembler_t *assembler;
} map_scope_t;

// Pushes a map symbol scope onto the scope stack. This involves allocating a
// map on the heap and if that fails a condition is returned.
value_t assembler_push_map_scope(assembler_t *assm, map_scope_t *scope);

// Pops a map symbol scope off the scope stack.
void assembler_pop_map_scope(assembler_t *assm, map_scope_t *scope);

// Binds a symbol on the given map scope. The symbol must not already be bound
// in this scope.
value_t map_scope_bind(map_scope_t *scope, value_t symbol, binding_type_t type,
    uint32_t data);


// A scope that records any variables looked up in an enclosing scope and turns
// them into captures rather than direct access.
typedef struct {
  // The callback that performs lookup in this scope.
  scope_lookup_callback_t callback;
  // Then enclosing scope.
  scope_lookup_callback_t *outer;
  // The list of captured symbols.
  value_t captures;
  // The assembler this scope belongs to.
  assembler_t *assembler;
} lambda_scope_t;

// Pushes a lambda scope onto the scope stack.
value_t assembler_push_lambda_scope(assembler_t *assm, lambda_scope_t *scope);

// Pops a lambda scope off the scope stack.
void assembler_pop_lambda_scope(assembler_t *assm, lambda_scope_t *scope);


// A scope that turns direct access to symbols into indirect block reads.
typedef struct {
  // The callback that performs lookup in this scope.
  scope_lookup_callback_t callback;
  // Then enclosing scope.
  scope_lookup_callback_t *outer;
  // The assembler this scope belongs to.
  assembler_t *assembler;
} block_scope_t;

// Pushes a block scope onto the scope stack.
value_t assembler_push_block_scope(assembler_t *assm, block_scope_t *scope);

// Pops a block scope off the scope stack.
void assembler_pop_block_scope(assembler_t *assm, block_scope_t *scope);


// Looks up a symbol in the current and surrounding scopes. Returns a condition if
// the symbol is not found, otherwise if the stores the binding in the given out
// argument.
value_t assembler_lookup_symbol(assembler_t *assm, value_t symbol,
    binding_info_t *info_out);

// Returns true if this assembler currently has a binding for the given symbol.
bool assembler_is_symbol_bound(assembler_t *assm, value_t symbol);

#endif // _CODEGEN
