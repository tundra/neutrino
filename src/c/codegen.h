// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Bytecode generation.


#ifndef _CODEGEN
#define _CODEGEN

#include "interp.h"
#include "value.h"



// Identifies what kind of binding a bound symbol represents.
typedef enum {
  // A local variable in the current scope.
  btLocal = 0,
  // An argument to the current immediate function.
  btArgument,
  // A symbol captured by an enclosing method.
  btCaptured
} binding_type_t;

// A collection of information about a binding. This is going to be encoded as
// an int and stored in a tagged integer so it can't be larger than effectively
// 61 bits.
typedef struct {
  // The type of the binding.
  binding_type_t type;
  // Extra data about the binding.
  uint32_t data;
} binding_info_t;

// A function that performs a scoped lookup. If the info_out argument is NULL
// we're only checking whether the binding exists, not actually accessing it.
// The return value should be a NotFound signal if the symbol could not be
// resolved, a non-signal otherwise.
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

// Initializes a scope lookup callback to be the bottom scope which has no
// symbols.
void scope_lookup_callback_init_bottom(scope_lookup_callback_t *callback);

// Invokes a scope lookup callback with a symbol. The result will be stored in
// the info-out parameter. If there's any problem the result will be a signal.
value_t scope_lookup_callback_call(scope_lookup_callback_t *callback,
    value_t symbol, binding_info_t *info_out);

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

// Bytecode assembler data.
typedef struct assembler_t {
  // The runtime we're generating code within.
  runtime_t *runtime;
  // The buffer that holds the code being built.
  byte_buffer_t code;
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
} assembler_t;

// Initializes an assembler. If the given scope callback is NULL it is taken to
// mean that there is no enclosing scope.
value_t assembler_init(assembler_t *assm, runtime_t *runtime,
    scope_lookup_callback_t *scope_callback);

// Sets the scope callback for the given assembler, returning the previous
// value.
scope_lookup_callback_t *assembler_set_scope_callback(assembler_t *assm,
    scope_lookup_callback_t *scope_callback);

// Disposes of an assembler.
void assembler_dispose(assembler_t *assm);

// Returns a code block object containing the code written to this assembler.
value_t assembler_flush(assembler_t *assm);

// Allocates a block of scratch memory. Only one scratch block can be used at
// any given time, the next call will invalidate and/or reuse the block returned
// by the previous call.
void *assembler_scratch_malloc(assembler_t *assm, size_t size);

// Similar to assembler_scratch_malloc but allocates two blocks of memory in one
// go. Really it's just a convenience for allocating one big block and splitting
// it in two.
void assembler_scratch_double_malloc(assembler_t *assm, size_t first_size,
    void **first, size_t second_size, void **second);

// Emits a push instruction.
value_t assembler_emit_push(assembler_t *assm, value_t value);

// Emits a pop instruction. Pops count elements off the stack.
value_t assembler_emit_pop(assembler_t *assm, size_t count);

// Emits a new-array instruction that builds an array from the top 'length'
// elements.
value_t assembler_emit_new_array(assembler_t *assm, size_t length);

// Emits an invocation using the given record.
value_t assembler_emit_invocation(assembler_t *assm, value_t space, value_t record);

// Emits a raw call to a builtin with the given implementation and number of
// arguments.
value_t assembler_emit_builtin(assembler_t *assm, builtin_method_t builtin);

// Emits a return instruction.
value_t assembler_emit_return(assembler_t *assm);

// Emits a local variable load of the local with the given index.
value_t assembler_emit_load_local(assembler_t *assm, size_t index);

// Emits a global variable load of the local with the given name within the
// given namespace.
value_t assembler_emit_load_global(assembler_t *assm, value_t name,
    value_t namespace);

// Emits an argument load of the argument with the given parameter index.
value_t assembler_emit_load_argument(assembler_t *assm, size_t param_index);

// Emits a load of a captured outer variable in the subject object.
value_t assembler_emit_load_outer(assembler_t *assm, size_t index);

// Emits a lambda that understands the given methods and which expects the given
// number of outer variables to be present on the stack.
value_t assembler_emit_lambda(assembler_t *assm, value_t methods,
    size_t outer_count);

// Hacky implementation of calling lambdas. Later this should be replaced by a
// more general delegate operation.
value_t assembler_emit_delegate_lambda_call(assembler_t *assm);


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
// map on the heap and if that fails a signal is returned.
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
} capture_scope_t;

// Pushes a capture scope onto the scope stack.
value_t assembler_push_capture_scope(assembler_t *assm, capture_scope_t *scope);

// Pops a map symbol scope off the scope stack.
void assembler_pop_capture_scope(assembler_t *assm, capture_scope_t *scope);


// Looks up a symbol in the current and surrounding scopes. Returns a signal if
// the symbol is not found, otherwise if the stores the binding in the given out
// argument.
value_t assembler_lookup_symbol(assembler_t *assm, value_t symbol,
    binding_info_t *info_out);

// Returns true if this assembler currently has a binding for the given symbol.
bool assembler_is_symbol_bound(assembler_t *assm, value_t symbol);

#endif // _CODEGEN
