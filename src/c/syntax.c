#include "behavior.h"
#include "syntax.h"
#include "value-inl.h"


// --- L i t e r a l ---

OBJECT_IDENTITY_IMPL(literal_ast);
FIXED_SIZE_IMPL(literal_ast, LiteralAst);

value_t get_literal_ast_value(value_t value) {
  CHECK_FAMILY(ofLiteralAst, value);
  return *access_object_field(value, kLiteralAstValueOffset);
}

void set_literal_ast_value(value_t literal, value_t value) {
  CHECK_FAMILY(ofLiteralAst, literal);
  *access_object_field(literal, kLiteralAstValueOffset) = value;
}

value_t literal_ast_validate(value_t value) {
  VALIDATE_VALUE_FAMILY(ofLiteralAst, value);
  return success();
}

void literal_ast_print_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<literal: ");
  value_print_atomic_on(get_literal_ast_value(value), buf);
  string_buffer_printf(buf, ">");
}

void literal_ast_print_atomic_on(value_t value, string_buffer_t *buf) {
  string_buffer_printf(buf, "#<literal>");
}
