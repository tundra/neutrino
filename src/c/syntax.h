#include "value.h"

#ifndef _SYNTAX
#define _SYNTAX

// --- S y n t a x   s p e c i e s ---

static const size_t kSyntaxSpeciesSize = OBJECT_SIZE(4);
static const size_t kSyntaxSpeciesTypeOffset = 3;


#define ENUM_SYNTAX_TYPES(F)                                                   \
  F(Literal, literal)

// Identifies the different types of syntax trees.
typedef enum {
  __stFirst__ = -1
#define DECLARE_SYNTAX_TYPE_ENUM(Division, division) , sd##Division
  ENUM_SYNTAX_TYPES(DECLARE_SYNTAX_TYPE_ENUM)
#undef DECLARE_SYNTAX_TYPE_ENUM
} syntax_type_t;

// Returns the type of a syntax tree object.
syntax_type_t get_syntax_type(value_t value);

// Returns the syntax type of a given syntax species.
syntax_type_t get_syntax_species_type(value_t value);

// Sets the syntax type of the given syntax species.
void set_syntax_species_type(value_t value, syntax_type_t type);


// --- L i t e r a l ---

static const size_t kLiteralSize = OBJECT_SIZE(1);
static const size_t kLiteralValueOffset = 3;

// Returns the value this literal syntax tree represents.
value_t get_literal_value(value_t value);

// Sets the value this literal syntax tree represents.
void set_literal_value(value_t literal, value_t value);

#endif // _SYNTAX
