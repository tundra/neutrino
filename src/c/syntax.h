#include "value.h"

#ifndef _SYNTAX
#define _SYNTAX


// --- L i t e r a l ---

static const size_t kLiteralAstSize = OBJECT_SIZE(1);
static const size_t kLiteralAstValueOffset = 3;

// Returns the value this literal syntax tree represents.
value_t get_literal_ast_value(value_t value);

// Sets the value this literal syntax tree represents.
void set_literal_ast_value(value_t literal, value_t value);

#endif // _SYNTAX
