// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "interp.h"
#include "value.h"
#include "plankton.h"

#ifndef _SYNTAX
#define _SYNTAX


// --- M i s c ---

// Initialize a value mapping such that it maps syntax constructors to syntax
// value factories from the given runtime.
value_t init_syntax_mapping(value_mapping_t *mapping, runtime_t *runtime);

// Initialize the map from syntax factory names to the factories themselves.
value_t init_syntax_factory_map(value_t map, runtime_t *runtime);

// Emits bytecode representing the given syntax tree value. If the value is not
// a syntax tree an InvalidSyntax signal is returned.
value_t emit_value(value_t value, assembler_t *assm);

// Compiles the given syntax tree to a code block.
value_t compile_syntax(runtime_t *runtime, value_t ast);


// --- L i t e r a l ---

static const size_t kLiteralAstSize = OBJECT_SIZE(1);
static const size_t kLiteralAstValueOffset = 1;

// The value this literal syntax tree represents.
ACCESSORS_DECL(literal_ast, value);


// --- A r r a y ---

static const size_t kArrayAstSize = OBJECT_SIZE(1);
static const size_t kArrayAstElementsOffset = 1;

// The array of element expressions.
ACCESSORS_DECL(array_ast, elements);


#endif // _SYNTAX
