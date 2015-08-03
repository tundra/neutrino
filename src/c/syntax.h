//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _SYNTAX
#define _SYNTAX

#include "codegen.h"
#include "value.h"
#include "serialize.h"

// --- M i s c ---

// Initialize the map from syntax factory names to the factories themselves.
value_t init_plankton_syntax_factories(value_t map, runtime_t *runtime);

// Emits bytecode representing the given syntax tree value. If the value is not
// a syntax tree an InvalidSyntax condition is returned.
value_t emit_value(value_t value, assembler_t *assm);

// Configuration of how to do expression compilation.
typedef struct {
  // If this is something it signals that the compiler should reify the
  // arguments in the preamble.
  value_t reify_params_opt;
  // If we're reifying parameters should the subject be cleared? This is useful
  // in cases where the subject is "magical" and shouldn't be exposed to the
  // surface language.
  bool clear_reified_subject;
} single_compile_config_t;

// Clears the given config to its defaults.
void single_compile_config_clear(single_compile_config_t *config);

// The state used when compiling a single expression. This is really all
// implementation details but it's convenient to have it factored out so it can
// be passed around explicitly.
typedef struct {
  runtime_t *runtime;
  assembler_t assm;
  // Optional flags that control compilation.
  single_compile_config_t *config;
} single_compile_state_t;

// Compiles the given expression syntax tree to a code block. The scope callback
// allows this compilation to access symbols defined in an outer scope. If the
// callback is null it is taken to mean that there is no outer scope.
value_t compile_expression(runtime_t *runtime, value_t ast,
    value_t fragment, scope_o *scope, single_compile_config_t *config_opt);

// Does the same a compile_expression but takes an existing initialized state
// rather than create one.
value_t compile_expression_with_state(single_compile_state_t *state,
    value_t ast);

// Compiles a method into a bytecode implementation.
value_t compile_method_body(assembler_t *assm, value_t method_ast, bool is_catch);

// Retrying version of compile_expression.
value_t safe_compile_expression(runtime_t *runtime, safe_value_t ast,
    safe_value_t module, scope_o *scope);

// Determines the parameter ordering to use given an array of parameter asts.
//
// The result will be returned as an array where the i'th entry of the gives the
// parameter index for the parameter in the i'th entry in the tags array. It
// will be allocated using the given scratch memory.
//
// Ordering the same parameters twice yields the same ordering, even if the
// parameters have been relocated in the meantime.
size_t *calc_parameter_ast_ordering(reusable_scratch_memory_t *scratch,
    value_t params);

// The worst possible parameter score used for values where we don't really
// care about the ordering.
static const size_t kMaxOrderIndex = ~0L;

// Returns the array ordering index for the given array of tags. The array
// ordering index is minimum over the non-array (value) ordering indexes of the
// elements in the array. The non-array indexes are:
//
//   0: the subject key
//   1: the selector key
//   n+2: the integer n
//   kMaxOrderIndex: any other value
size_t get_parameter_order_index_for_array(value_t tags);

// Builds a method signature based on a signature syntax tree.
value_t build_method_signature(runtime_t *runtime, value_t fragment,
    reusable_scratch_memory_t *scratch, value_t signature_ast);

// So... yeah. Evaluates a piece of syntax without too much fuss. Should be
// replaced by proper evaluation as soon as it makes sense.
value_t quick_and_dirty_evaluate_syntax(runtime_t *runtime, value_t fragment,
    value_t value_ast);


// --- L i t e r a l   a s t ---

static const size_t kLiteralAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kLiteralAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The value this literal syntax tree represents.
ACCESSORS_DECL(literal_ast, value);


// --- A r r a y   a s t ---

static const size_t kArrayAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kArrayAstElementsOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The array of element expressions.
ACCESSORS_DECL(array_ast, elements);


/// ## Signal ast

static const size_t kSignalAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kSignalAstEscapeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kSignalAstArgumentsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kSignalAstDefaultOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// Escape after executing the handler or keep running?
ACCESSORS_DECL(signal_ast, escape);

// The array of element expressions.
ACCESSORS_DECL(signal_ast, arguments);

// The optional default expression to execute if there are no handlers.
ACCESSORS_DECL(signal_ast, default);


/// ## Signal handler ast

static const size_t kSignalHandlerAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kSignalHandlerAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kSignalHandlerAstHandlersOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The body of this expression.
ACCESSORS_DECL(signal_handler_ast, body);

// The signal handlers to install before running the body.
ACCESSORS_DECL(signal_handler_ast, handlers);


// --- E n s u r e   a s t ---

static const size_t kEnsureAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kEnsureAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kEnsureAstOnExitOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The body to execute normally.
ACCESSORS_DECL(ensure_ast, body);

// The code to execute after the body completes, either normally or by escaping.
ACCESSORS_DECL(ensure_ast, on_exit);


// --- I n v o c a t i o n   a s t ---

static const size_t kInvocationAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kInvocationAstArgumentsOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The array of element expressions.
ACCESSORS_DECL(invocation_ast, arguments);


/// ## Call literal ast

static const size_t kCallLiteralAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kCallLiteralAstArgumentsOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The array of element expressions.
ACCESSORS_DECL(call_literal_ast, arguments);


/// ## Call literal argument ast

static const size_t kCallLiteralArgumentAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kCallLiteralArgumentAstTagOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kCallLiteralArgumentAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The tag of this call argument.
ACCESSORS_DECL(call_literal_argument_ast, tag);

// The value of this call argument.
ACCESSORS_DECL(call_literal_argument_ast, value);


// --- A r g u m e n t   a s t ---

static const size_t kArgumentAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kArgumentAstTagOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kArgumentAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kArgumentAstNextGuardOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The argument tag.
ACCESSORS_DECL(argument_ast, tag);

// The argument value.
ACCESSORS_DECL(argument_ast, value);

// An optional guard that specifies what to find the next method relative to.
ACCESSORS_DECL(argument_ast, next_guard);


// --- S e q u e n c e   a s t ---

static const size_t kSequenceAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kSequenceAstValuesOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The array of values to evaluate in sequence.
ACCESSORS_DECL(sequence_ast, values);


// --- L o c a l   d e c l a r a t i o n   a s t ---

static const size_t kLocalDeclarationAstSize = HEAP_OBJECT_SIZE(4);
static const size_t kLocalDeclarationAstSymbolOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kLocalDeclarationAstIsMutableOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kLocalDeclarationAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kLocalDeclarationAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// The declaration's symbol.
ACCESSORS_DECL(local_declaration_ast, symbol);

// Is this variable mutable or fixed?
ACCESSORS_DECL(local_declaration_ast, is_mutable);

// The variable's value.
ACCESSORS_DECL(local_declaration_ast, value);

// The expression that's in scope of the variable.
ACCESSORS_DECL(local_declaration_ast, body);


// --- B l o c k   a s t ---

static const size_t kBlockAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kBlockAstSymbolOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kBlockAstMethodsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kBlockAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The block's symbol.
ACCESSORS_DECL(block_ast, symbol);

// The block's methods.
ACCESSORS_DECL(block_ast, methods);

// The expression that's in scope of the block.
ACCESSORS_DECL(block_ast, body);


// --- W i t h   e s c a p e   a s t ---

static const size_t kWithEscapeAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kWithEscapeAstSymbolOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kWithEscapeAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The symbol that represents the captured escape.
ACCESSORS_DECL(with_escape_ast, symbol);

// The expression that's in scope of the escape.
ACCESSORS_DECL(with_escape_ast, body);


// --- V a r i a b l e   a s s i g n m e n t   a s t ---

static const size_t kVariableAssignmentAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kVariableAssignmentAstTargetOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kVariableAssignmentAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The variable to store the value in.
ACCESSORS_DECL(variable_assignment_ast, target);

// The value to store.
ACCESSORS_DECL(variable_assignment_ast, value);


// --- L o c a l   v a r i a b l e   a s t ---

static const size_t kLocalVariableAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kLocalVariableAstSymbolOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The symbol referenced by this local variable.
ACCESSORS_DECL(local_variable_ast, symbol);


// --- N a m e s p a c e   v a r i a b l e   a s t ---

static const size_t kNamespaceVariableAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kNamespaceVariableAstIdentifierOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The identifier to look up through this namespace variable.
ACCESSORS_DECL(namespace_variable_ast, identifier);


// --- S y m b o l ---

static const size_t kSymbolAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kSymbolAstNameOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kSymbolAstOriginOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The display name of this symbol
ACCESSORS_DECL(symbol_ast, name);

// A pointer back to the declaration that introduced this symbol.
ACCESSORS_DECL(symbol_ast, origin);


// --- L a m b d a   a s t ---

static const size_t kLambdaAstSize = HEAP_OBJECT_SIZE(1);
static const size_t kLambdaAstMethodsOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The methods that implements this lambda.
ACCESSORS_DECL(lambda_ast, methods);


// --- P a r a m e t e r   a s t ---

static const size_t kParameterAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kParameterAstSymbolOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kParameterAstTagsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kParameterAstGuardOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The symbol that identifies this parameter.
ACCESSORS_DECL(parameter_ast, symbol);

// The list of tags matched by this parameter.
ACCESSORS_DECL(parameter_ast, tags);

// The argument guard.
ACCESSORS_DECL(parameter_ast, guard);


// --- G u a r d   a s t ---

static const size_t kGuardAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kGuardAstTypeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kGuardAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The value of this guard used to match by gtId and gtIs and unused for gtAny.
ACCESSORS_DECL(guard_ast, value);

// The type of match to perform for this guard.
TYPED_ACCESSORS_DECL(guard_ast, guard_type_t, type);


// --- S i g n a t u r e   a s t ---

static const size_t kSignatureAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kSignatureAstParametersOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kSignatureAstAllowExtraOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kSignatureAstReifiedOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The array of parameters for this signature.
ACCESSORS_DECL(signature_ast, parameters);

// Permit extra arguments to be passed that don't correspond to formal params.
ACCESSORS_DECL(signature_ast, allow_extra);

// If we want the arguments to be reified this will hold the symbol under which
// they're available. Otherwise nothing.
ACCESSORS_DECL(signature_ast, reified);


// --- M e t h o d   a s t ---

static const size_t kMethodAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kMethodAstSignatureOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kMethodAstBodyOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The signature ast of this method ast.
ACCESSORS_DECL(method_ast, signature);

// The implementation (body) of this method.
ACCESSORS_DECL(method_ast, body);


// --- N a m e s p a c e   d e c l a r a t i o n   a s t ---

static const size_t kNamespaceDeclarationAstSize = HEAP_OBJECT_SIZE(3);
static const size_t kNamespaceDeclarationAstAnnotationsOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kNamespaceDeclarationAstPathOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kNamespaceDeclarationAstValueOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The list of annotations attached to this declaration.
ACCESSORS_DECL(namespace_declaration_ast, annotations);

// The path this declaration declares.
ACCESSORS_DECL(namespace_declaration_ast, path);

// The value to be bound by this declaration.
ACCESSORS_DECL(namespace_declaration_ast, value);


// --- M e t h o d   d e c l a r a t i o n   a s t ---

static const size_t kMethodDeclarationAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kMethodDeclarationAstAnnotationsOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kMethodDeclarationAstMethodOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The list of annotations attached to this declaration.
ACCESSORS_DECL(method_declaration_ast, annotations);

// The method object describing the method being declared.
ACCESSORS_DECL(method_declaration_ast, method);


// --- I s   d e c l a r a t i o n   a s t ---

static const size_t kIsDeclarationAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kIsDeclarationAstSubtypeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kIsDeclarationAstSupertypeOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The subtype being declared.
ACCESSORS_DECL(is_declaration_ast, subtype);

// The new supertype of the subtype.
ACCESSORS_DECL(is_declaration_ast, supertype);


// --- P r o g r a m -  a s t --

static const size_t kProgramAstSize = HEAP_OBJECT_SIZE(2);
static const size_t kProgramAstEntryPointOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kProgramAstModuleOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The program entry-point expression.
ACCESSORS_DECL(program_ast, entry_point);

// The module that provides context for the entry-point.
ACCESSORS_DECL(program_ast, module);


// --- C u r r e n t   m o d u l e   a s t ---

static const size_t kCurrentModuleAstSize = HEAP_OBJECT_SIZE(0);


#endif // _SYNTAX
