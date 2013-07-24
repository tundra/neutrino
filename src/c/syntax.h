#include "value.h"
#include "plankton.h"

#ifndef _SYNTAX
#define _SYNTAX


// Initialize a value mapping such that it maps syntax constructors to syntax
// value factories from the given runtime.
value_t init_syntax_mapping(value_mapping_t *mapping, runtime_t *runtime);

// Initialize the map from syntax factory names to the factories themselves.
value_t init_syntax_factory_map(value_t map, runtime_t *runtime);


// --- L i t e r a l ---

static const size_t kLiteralAstSize = OBJECT_SIZE(1);
static const size_t kLiteralAstValueOffset = 3;

// Returns the value this literal syntax tree represents.
value_t get_literal_ast_value(value_t value);

// Sets the value this literal syntax tree represents.
void set_literal_ast_value(value_t literal, value_t value);

#endif // _SYNTAX
