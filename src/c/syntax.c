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
