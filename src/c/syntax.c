#include "alloc.h"
#include "behavior.h"
#include "syntax.h"
#include "value-inl.h"

static value_t resolve_syntax_factory(value_t key, runtime_t *runtime, void *data) {
  // We only accept keys of the form ["ast", <name>] so first we check that the
  // key does indeed have that shape.
  if (get_object_family(key) != ofArray || get_array_length(key) != 2)
    return new_signal(scNothing);

  // Check that the first element is the string "ast".
  value_t first_obj = get_array_at(key, 0);
  if (get_object_family(first_obj) != ofString)
    return new_signal(scNothing);
  string_t first;
  get_string_contents(first_obj, &first);
  if (!string_equals_cstr(&first, "ast"))
    return new_signal(scNothing);

  // Check that the second element is a string and look it up if it is.
  value_t second_obj = get_array_at(key, 1);
  if (get_object_family(second_obj) != ofString)
    return new_signal(scNothing);
  return get_id_hash_map_at(runtime->roots.syntax_factories, second_obj);
}

// Initialize a value mapping such that it maps syntax constructors to syntax
// value factories from the given runtime.
value_t init_syntax_mapping(value_mapping_t *mapping, runtime_t *runtime) {
  value_mapping_init(mapping, resolve_syntax_factory, NULL);
  return success();
}


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

static value_t new_literal_ast(runtime_t *runtime) {
  return new_heap_literal_ast(runtime, runtime->roots.null);
}

value_t set_literal_ast_contents(value_t value, runtime_t *runtime, value_t contents) {
  // TODO: actually set the contents.
  return success();
}


// --- F a c t o r i e s ---

// Adds a syntax factory object to the given syntax factory map under the given
// name.
static value_t add_syntax_factory(value_t map, const char *name,
    factory_constructor_t constructor, runtime_t *runtime) {
  string_t key_str;
  string_init(&key_str, name);
  TRY_DEF(key_obj, new_heap_string(runtime, &key_str));
  TRY_DEF(factory, new_heap_factory(runtime, constructor));
  TRY(set_id_hash_map_at(runtime, map, key_obj, factory));
  return success();
}

value_t init_syntax_factory_map(value_t map, runtime_t *runtime) {
  TRY(add_syntax_factory(map, "Literal", new_literal_ast, runtime));
  return success();
}
