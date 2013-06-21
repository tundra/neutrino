#include "syntax.h"

syntax_type_t get_syntax_type(value_t value) {
  value_t species = get_object_species(value);
  return get_syntax_species_type(species);
}

syntax_type_t get_syntax_species_type(value_t value) {
  return get_integer_value(*access_object_field(value, kSyntaxSpeciesTypeOffset));
}

// Sets the syntax type of the given syntax species.
void set_syntax_species_type(value_t value, syntax_type_t type) {
  *access_object_field(value, kSyntaxSpeciesTypeOffset) = new_integer(type);
}


// --- L i t e r a l ---

value_t get_literal_ast_value(value_t value) {
  CHECK_FAMILY(ofLiteralAst, value);
  return *access_object_field(value, kLiteralAstValueOffset);
}

void set_literal_ast_value(value_t literal, value_t value) {
  CHECK_FAMILY(ofLiteralAst, literal);
  *access_object_field(literal, kLiteralAstValueOffset) = value;
}
