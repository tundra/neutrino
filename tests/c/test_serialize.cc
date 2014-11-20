//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "plankton.h"
#include "serialize.h"
#include "try-inl.h"
#include "value-inl.h"
END_C_INCLUDES

// Encodes and decodes a plankton value and returns the result.
static value_t transcode_plankton(runtime_t *runtime, value_mapping_t *resolver,
    value_mapping_t *access, value_t value) {
  // Encode and decode the value.
  value_t encoded = plankton_serialize(runtime, resolver, value);
  ASSERT_SUCCESS(encoded);
  value_t decoded = plankton_deserialize(runtime, access, encoded);
  ASSERT_SUCCESS(decoded);
  return decoded;
}

// Encodes and decodes a plankton value and checks that the result is the
// same as the input. Returns the decoded value.
static value_t check_plankton(runtime_t *runtime, value_t value) {
  value_t decoded = transcode_plankton(runtime, NULL, NULL, value);
  ASSERT_VALEQ(value, decoded);
  return decoded;
}

TEST(serialize, simple) {
  CREATE_RUNTIME();

  // Integers
  check_plankton(runtime, new_integer(0));
  check_plankton(runtime, new_integer(1));
  check_plankton(runtime, new_integer(-1));
  check_plankton(runtime, new_integer(65536));
  check_plankton(runtime, new_integer(-65536));

  // Singletons
  check_plankton(runtime, null());
  check_plankton(runtime, yes());
  check_plankton(runtime, no());

  DISPOSE_RUNTIME();
}

TEST(serialize, array) {
  CREATE_RUNTIME();

  value_t arr = new_heap_array(runtime, 5);
  check_plankton(runtime, arr);
  set_array_at(arr, 0, new_integer(5));
  check_plankton(runtime, arr);

  DISPOSE_RUNTIME();
}

TEST(serialize, map) {
  CREATE_RUNTIME();

  value_t map = new_heap_id_hash_map(runtime, 16);
  check_plankton(runtime, map);
  for (size_t i = 0; i < 16; i++) {
    set_id_hash_map_at(runtime, map, new_integer(i), new_integer(5));
    check_plankton(runtime, map);
  }

  DISPOSE_RUNTIME();
}

// Declares a new variable that holds a heap string with the given contents.
#define DEF_HEAP_STR(name, value)                                              \
utf8_t name##_chars = new_c_string(value);                                     \
value_t name = new_heap_utf8(runtime, name##_chars)

TEST(serialize, string) {
  CREATE_RUNTIME();

  DEF_HEAP_STR(foo, "foo");
  check_plankton(runtime, foo);
  DEF_HEAP_STR(empty, "");
  check_plankton(runtime, empty);
  DEF_HEAP_STR(hello, "Hello, World!");
  check_plankton(runtime, hello);

  DISPOSE_RUNTIME();
}

TEST(serialize, instance) {
  CREATE_RUNTIME();

  value_t instance = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  check_plankton(runtime, instance);
  DEF_HEAP_STR(x, "x");
  ASSERT_SUCCESS(try_set_instance_field(instance, x, new_integer(8)));
  DEF_HEAP_STR(y, "y");
  ASSERT_SUCCESS(try_set_instance_field(instance, y, new_integer(13)));
  value_t decoded = check_plankton(runtime, instance);
  ASSERT_SUCCESS(decoded);
  ASSERT_VALEQ(new_integer(8), get_instance_field(decoded, x));

  DISPOSE_RUNTIME();
}

TEST(serialize, references) {
  CREATE_RUNTIME();

  value_t i0 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i1 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i2 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t array = new_heap_array(runtime, 6);
  set_array_at(array, 0, i0);
  set_array_at(array, 1, i2);
  set_array_at(array, 2, i0);
  set_array_at(array, 3, i1);
  set_array_at(array, 4, i2);
  set_array_at(array, 5, i1);
  value_t decoded = check_plankton(runtime, array);
  ASSERT_SAME(get_array_at(decoded, 0), get_array_at(decoded, 2));
  ASSERT_NSAME(get_array_at(decoded, 0), get_array_at(decoded, 1));
  ASSERT_SAME(get_array_at(decoded, 1), get_array_at(decoded, 4));
  ASSERT_NSAME(get_array_at(decoded, 1), get_array_at(decoded, 3));
  ASSERT_SAME(get_array_at(decoded, 3), get_array_at(decoded, 5));

  DISPOSE_RUNTIME();
}

TEST(serialize, cycles) {
  CREATE_RUNTIME();

  value_t i0 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t k0 = new_integer(78);
  ASSERT_SUCCESS(set_instance_field(runtime, i0, k0, i0));
  value_t d0 = transcode_plankton(runtime, NULL, NULL, i0);
  ASSERT_SAME(d0, get_instance_field(d0, k0));

  value_t i1 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i2 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i3 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t k1 = new_integer(79);
  ASSERT_SUCCESS(set_instance_field(runtime, i1, k0, i2));
  ASSERT_SUCCESS(set_instance_field(runtime, i1, k1, i3));
  ASSERT_SUCCESS(set_instance_field(runtime, i2, k1, i3));
  ASSERT_SUCCESS(set_instance_field(runtime, i3, k0, i1));
  value_t d1 = transcode_plankton(runtime, NULL, NULL, i1);
  value_t d2 = get_instance_field(d1, k0);
  value_t d3 = get_instance_field(d1, k1);
  ASSERT_NSAME(d1, d2);
  ASSERT_NSAME(d1, d3);
  ASSERT_SAME(d3, get_instance_field(d2, k1));
  ASSERT_SAME(d1, get_instance_field(d3, k0));


  DISPOSE_RUNTIME();
}

// Struct containing test data for the environment test.
typedef struct {
  value_t i0;
  value_t i1;
} test_resolver_data_t;

// Map values to ints.
static value_t value_to_int(value_t value, runtime_t *runtime, void *ptr) {
  test_resolver_data_t *data = (test_resolver_data_t*) ptr;
  if (value_identity_compare(value, data->i0)) {
    return new_integer(0);
  } else if (value_identity_compare(value, data->i1)) {
    return new_integer(1);
  } else {
    return new_condition(ccNothing);
  }
}

// Map ints to values.
static value_t int_to_value(value_t value, runtime_t *runtime, void *ptr) {
  test_resolver_data_t *data = (test_resolver_data_t*) ptr;
  switch (get_integer_value(value)) {
    case 0:
      return data->i0;
    case 1:
      return data->i1;
    default:
      UNREACHABLE("int to value");
      return new_integer(0);
  }
}

TEST(serialize, env_resolution) {
  CREATE_RUNTIME();

  test_resolver_data_t data;
  data.i0 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  data.i1 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i2 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));

  value_mapping_t resolver;
  value_mapping_init(&resolver, value_to_int, &data);
  value_mapping_t access;
  value_mapping_init(&access, int_to_value, &data);

  value_t d0 = transcode_plankton(runtime, &resolver, &access, data.i0);
  ASSERT_TRUE(value_identity_compare(data.i0, d0));
  value_t d1 = transcode_plankton(runtime, &resolver, &access, data.i1);
  ASSERT_TRUE(value_identity_compare(data.i1, d1));
  value_t d2 = transcode_plankton(runtime, &resolver, &access, i2);
  ASSERT_FALSE(value_identity_compare(i2, d2));

  value_t a0 = new_heap_array(runtime, 4);
  set_array_at(a0, 0, data.i0);
  set_array_at(a0, 1, data.i1);
  set_array_at(a0, 2, i2);
  set_array_at(a0, 3, data.i0);
  value_t da0 = transcode_plankton(runtime, &resolver, &access, a0);
  ASSERT_TRUE(value_identity_compare(data.i0, get_array_at(da0, 0)));
  ASSERT_TRUE(value_identity_compare(data.i1, get_array_at(da0, 1)));
  ASSERT_FALSE(value_identity_compare(i2, get_array_at(da0, 2)));
  ASSERT_TRUE(value_identity_compare(data.i0, get_array_at(da0, 3)));

  DISPOSE_RUNTIME();
}

// Writes a tagged plankton string to the given buffer.
static value_t write_string(pton_assembler_t *assm, const char *c_str) {
  utf8_t str = new_c_string(c_str);
  pton_assembler_emit_default_string(assm, str.chars, string_size(str));
  return success();
}

// Writes an ast factory reference with the given ast type to the given buffer.
static value_t write_ast_factory(pton_assembler_t *assm, const char *ast_type) {
  pton_assembler_begin_environment_reference(assm);
  pton_assembler_begin_array(assm, 2);
  TRY(write_string(assm, "ast"));
  TRY(write_string(assm, ast_type));
  return success();
}

// Deserializes the contents of the given buffer as plankton within the given
// runtime, resolving environment references using a syntax mapping.
static value_t deserialize(runtime_t *runtime, pton_assembler_t *assm) {
  memory_block_t code = pton_assembler_peek_code(assm);
  blob_t raw_blob = new_blob(code.memory, code.size);
  value_t blob = new_heap_blob_with_data(runtime, raw_blob);
  value_mapping_t syntax_mapping;
  TRY(init_plankton_environment_mapping(&syntax_mapping, runtime));
  return plankton_deserialize(runtime, &syntax_mapping, blob);
}

TEST(serialize, env_construction) {
  CREATE_RUNTIME();

  // Environment references resolve correctly to ast factories.
  {
    pton_assembler_t *assm = pton_new_assembler();
    write_ast_factory(assm, "Literal");
    value_t value = deserialize(runtime, assm);
    ASSERT_FAMILY(ofFactory, value);
    pton_dispose_assembler(assm);
  }

  // Objects with ast factory headers produce asts.
  {
    pton_assembler_t *assm = pton_new_assembler();
    pton_assembler_begin_object(assm, 1);
    write_ast_factory(assm, "Literal");
    write_string(assm, "value");
    pton_assembler_emit_bool(assm, true);
    value_t value = deserialize(runtime, assm);
    ASSERT_FAMILY(ofLiteralAst, value);
    ASSERT_VALEQ(yes(), get_literal_ast_value(value));
    pton_dispose_assembler(assm);
  }

  DISPOSE_RUNTIME();
}
