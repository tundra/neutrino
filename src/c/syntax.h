// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).


#ifndef _SYNTAX
#define _SYNTAX

#include "codegen.h"
#include "value.h"
#include "plankton.h"

// --- M i s c ---

// Initialize a value mapping such that it maps environment references through
// the plankton environment of the given runtime.
value_t init_plankton_environment_mapping(value_mapping_t *mapping,
    runtime_t *runtime);

// Initialize the map from syntax factory names to the factories themselves.
value_t init_plankton_syntax_factories(value_t map, runtime_t *runtime);

// Emits bytecode representing the given syntax tree value. If the value is not
// a syntax tree an InvalidSyntax signal is returned.
value_t emit_value(value_t value, assembler_t *assm);

// Compiles the given expression syntax tree to a code block. The scope callback
// allows this compilation to access symbols defined in an outer scope. If the
// callback is null it is taken to mean that there is no outer scope.
value_t compile_expression(runtime_t *runtime, value_t ast,
    scope_lookup_callback_t *scope_callback);

// Does the same a compile_expression but takes an existing assembler rather
// than create one.
value_t compile_expression_with_assembler(runtime_t *runtime, value_t ast,
    assembler_t *assm);

// Compiles signature and body syntax into a compiled signature and a bytecode
// implementation. The implementation is returned, the signature is stored in
// the signature_out argument.
value_t compile_method_body(assembler_t *assm, value_t signature_ast,
    value_t body_ast);

// Retrying version of compile_expression.
value_t safe_compile_expression(runtime_t *runtime, safe_value_t ast,
    scope_lookup_callback_t *scope_callback);

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
value_t build_method_signature(runtime_t *runtime,
    reusable_scratch_memory_t *scratch, value_t signature_ast);


// --- L i t e r a l   a s t ---

static const size_t kLiteralAstSize = OBJECT_SIZE(1);
static const size_t kLiteralAstValueOffset = OBJECT_FIELD_OFFSET(0);

// The value this literal syntax tree represents.
ACCESSORS_DECL(literal_ast, value);


// --- A r r a y   a s t ---

static const size_t kArrayAstSize = OBJECT_SIZE(1);
static const size_t kArrayAstElementsOffset = OBJECT_FIELD_OFFSET(0);

// The array of element expressions.
ACCESSORS_DECL(array_ast, elements);


// --- I n v o c a t i o n   a s t ---

static const size_t kInvocationAstSize = OBJECT_SIZE(2);
static const size_t kInvocationAstArgumentsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kInvocationAstMethodspaceOffset = OBJECT_FIELD_OFFSET(1);

// The array of element expressions.
ACCESSORS_DECL(invocation_ast, arguments);

// The methodspace to perform the invocation in.
ACCESSORS_DECL(invocation_ast, methodspace);


// --- A r g u m e n t   a s t ---

static const size_t kArgumentAstSize = OBJECT_SIZE(2);
static const size_t kArgumentAstTagOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kArgumentAstValueOffset = OBJECT_FIELD_OFFSET(1);

// The argument tag.
ACCESSORS_DECL(argument_ast, tag);

// The argument value.
ACCESSORS_DECL(argument_ast, value);


// --- S e q u e n c e   a s t ---

static const size_t kSequenceAstSize = OBJECT_SIZE(1);
static const size_t kSequenceAstValuesOffset = OBJECT_FIELD_OFFSET(0);

// The array of values to evaluate in sequence.
ACCESSORS_DECL(sequence_ast, values);


// --- L o c a l   d e c l a r a t i o n   a s t ---

static const size_t kLocalDeclarationAstSize = OBJECT_SIZE(3);
static const size_t kLocalDeclarationAstSymbolOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kLocalDeclarationAstValueOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kLocalDeclarationAstBodyOffset = OBJECT_FIELD_OFFSET(2);

// The declaration's symbol.
ACCESSORS_DECL(local_declaration_ast, symbol);

// The variable's value.
ACCESSORS_DECL(local_declaration_ast, value);

// The expression that's in scope of the variable.
ACCESSORS_DECL(local_declaration_ast, body);


// --- L o c a l   v a r i a b l e   a s t ---

static const size_t kLocalVariableAstSize = OBJECT_SIZE(1);
static const size_t kLocalVariableAstSymbolOffset = OBJECT_FIELD_OFFSET(0);

// The symbol referenced by this local variable.
ACCESSORS_DECL(local_variable_ast, symbol);


// --- N a m e s p a c e   v a r i a b l e   a s t ---

static const size_t kNamespaceVariableAstSize = OBJECT_SIZE(2);
static const size_t kNamespaceVariableAstNameOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kNamespaceVariableAstNamespaceOffset = OBJECT_FIELD_OFFSET(1);

// The name to look up through this namespace variable.
ACCESSORS_DECL(namespace_variable_ast, name);

// The namespace to look up within.
ACCESSORS_DECL(namespace_variable_ast, namespace);


// --- S y m b o l ---

static const size_t kSymbolAstSize = OBJECT_SIZE(1);
static const size_t kSymbolAstNameOffset = OBJECT_FIELD_OFFSET(0);

// The display name of this symbol
ACCESSORS_DECL(symbol_ast, name);


// --- L a m b d a   a s t ---

static const size_t kLambdaAstSize = OBJECT_SIZE(2);
static const size_t kLambdaAstSignatureOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kLambdaAstBodyOffset = OBJECT_FIELD_OFFSET(1);

// The signature of this lambda's one method.
ACCESSORS_DECL(lambda_ast, signature);

// The body of this lambda.
ACCESSORS_DECL(lambda_ast, body);


// --- P a r a m e t e r   a s t ---

static const size_t kParameterAstSize = OBJECT_SIZE(3);
static const size_t kParameterAstSymbolOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kParameterAstTagsOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kParameterAstGuardOffset = OBJECT_FIELD_OFFSET(2);

// The symbol that identifies this parameter.
ACCESSORS_DECL(parameter_ast, symbol);

// The list of tags matched by this parameter.
ACCESSORS_DECL(parameter_ast, tags);

// The argument guard.
ACCESSORS_DECL(parameter_ast, guard);


// --- S i g n a t u r e   a s t ---

static const size_t kSignatureAstSize = OBJECT_SIZE(1);
static const size_t kSignatureAstParametersOffset = OBJECT_FIELD_OFFSET(0);

// The array of parameters for this signature.
ACCESSORS_DECL(signature_ast, parameters);


// --- M e t h o d   a s t ---

static const size_t kMethodAstSize = OBJECT_SIZE(2);
static const size_t kMethodAstSignatureOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kMethodAstBodyOffset = OBJECT_FIELD_OFFSET(1);

// The signature ast of this method ast.
ACCESSORS_DECL(method_ast, signature);

// The implementation (body) of this method.
ACCESSORS_DECL(method_ast, body);


// --- P r o g r a m -  a s t --

static const size_t kProgramAstSize = OBJECT_SIZE(2);
static const size_t kProgramAstEntryPointOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kProgramAstModuleOffset = OBJECT_FIELD_OFFSET(1);

// The program entry-point expression.
ACCESSORS_DECL(program_ast, entry_point);

// The program's main module.
ACCESSORS_DECL(program_ast, module);


// --- N a m e ---

static const size_t kNameAstSize = OBJECT_SIZE(2);
static const size_t kNameAstPathOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kNameAstPhaseOffset = OBJECT_FIELD_OFFSET(1);

// The path (ie. x:y:z etc.) of this name.
ACCESSORS_DECL(name_ast, path);

// The phase (ie. $..., @..., etc) of this name.
ACCESSORS_DECL(name_ast, phase);


// --- P a t h ---

static const size_t kPathAstSize = OBJECT_SIZE(1);
static const size_t kPathAstPartsOffset = OBJECT_FIELD_OFFSET(0);


#endif // _SYNTAX
