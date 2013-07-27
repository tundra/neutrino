#include "alloc.h"
#include "plankton.h"
#include "value-inl.h"
#include "test.h"

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

TEST(plankton, simple) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Integers
  check_plankton(&runtime, new_integer(0));
  check_plankton(&runtime, new_integer(1));
  check_plankton(&runtime, new_integer(-1));
  check_plankton(&runtime, new_integer(65536));
  check_plankton(&runtime, new_integer(-65536));

  // Singletons
  check_plankton(&runtime, runtime_null(&runtime));
  check_plankton(&runtime, runtime_bool(&runtime, true));
  check_plankton(&runtime, runtime_bool(&runtime, false));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(plankton, array) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t arr = new_heap_array(&runtime, 5);
  check_plankton(&runtime, arr);
  set_array_at(arr, 0, new_integer(5));
  check_plankton(&runtime, arr);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(plankton, map) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t map = new_heap_id_hash_map(&runtime, 16);
  check_plankton(&runtime, map);
  for (size_t i = 0; i < 16; i++) {
    set_id_hash_map_at(&runtime, map, new_integer(i), new_integer(5));
    check_plankton(&runtime, map);
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Declares a new variable that holds a heap string with the given contents.
#define DEF_HEAP_STR(name, value)                                              \
DEF_STR(name##_chars, value);                                                  \
value_t name = new_heap_string(&runtime, &name##_chars)

TEST(plankton, string) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  DEF_HEAP_STR(foo, "foo");
  check_plankton(&runtime, foo);
  DEF_HEAP_STR(empty, "");
  check_plankton(&runtime, empty);
  DEF_HEAP_STR(hello, "Hello, World!");
  check_plankton(&runtime, hello);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(plankton, instance) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t instance = new_heap_instance(&runtime);
  check_plankton(&runtime, instance);
  DEF_HEAP_STR(x, "x");
  ASSERT_SUCCESS(try_set_instance_field(instance, x, new_integer(8)));
  DEF_HEAP_STR(y, "y");
  ASSERT_SUCCESS(try_set_instance_field(instance, y, new_integer(13)));
  value_t decoded = check_plankton(&runtime, instance);
  ASSERT_SUCCESS(decoded);
  ASSERT_VALEQ(new_integer(8), get_instance_field(decoded, x));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(plankton, references) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t i0 = new_heap_instance(&runtime);
  value_t i1 = new_heap_instance(&runtime);
  value_t i2 = new_heap_instance(&runtime);
  value_t array = new_heap_array(&runtime, 6);
  set_array_at(array, 0, i0);
  set_array_at(array, 1, i2);
  set_array_at(array, 2, i0);
  set_array_at(array, 3, i1);
  set_array_at(array, 4, i2);
  set_array_at(array, 5, i1);
  value_t decoded = check_plankton(&runtime, array);
  ASSERT_EQ(get_array_at(decoded, 0), get_array_at(decoded, 2));
  ASSERT_FALSE(get_array_at(decoded, 0) == get_array_at(decoded, 1));
  ASSERT_EQ(get_array_at(decoded, 1), get_array_at(decoded, 4));
  ASSERT_FALSE(get_array_at(decoded, 1) == get_array_at(decoded, 3));
  ASSERT_EQ(get_array_at(decoded, 3), get_array_at(decoded, 5));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(plankton, cycles) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t i0 = new_heap_instance(&runtime);
  value_t k0 = new_integer(78);
  ASSERT_SUCCESS(set_instance_field(&runtime, i0, k0, i0));
  value_t d0 = transcode_plankton(&runtime, NULL, NULL, i0);
  ASSERT_EQ(d0, get_instance_field(d0, k0));

  value_t i1 = new_heap_instance(&runtime);
  value_t i2 = new_heap_instance(&runtime);
  value_t i3 = new_heap_instance(&runtime);
  value_t k1 = new_integer(79);
  ASSERT_SUCCESS(set_instance_field(&runtime, i1, k0, i2));
  ASSERT_SUCCESS(set_instance_field(&runtime, i1, k1, i3));
  ASSERT_SUCCESS(set_instance_field(&runtime, i2, k1, i3));
  ASSERT_SUCCESS(set_instance_field(&runtime, i3, k0, i1));
  value_t d1 = transcode_plankton(&runtime, NULL, NULL, i1);
  value_t d2 = get_instance_field(d1, k0);
  value_t d3 = get_instance_field(d1, k1);
  ASSERT_TRUE(d1 != d2);
  ASSERT_TRUE(d1 != d3);
  ASSERT_EQ(d3, get_instance_field(d2, k1));
  ASSERT_EQ(d1, get_instance_field(d3, k0));


  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Struct containing test data for the environment test.
typedef struct {
  value_t i0;
  value_t i1;
} test_resolver_data_t;

// Map values to ints.
static value_t value_to_int(value_t value, runtime_t *runtime, void *ptr) {
  test_resolver_data_t *data = ptr;
  if (value_are_identical(value, data->i0)) {
    return new_integer(0);
  } else if (value_are_identical(value, data->i1)) {
    return new_integer(1);
  } else {
    return new_signal(scNothing);
  }
}

// Map ints to values.
static value_t int_to_value(value_t value, runtime_t *runtime, void *ptr) {
  test_resolver_data_t *data = ptr;
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

TEST(plankton, env_resolution) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  test_resolver_data_t data;
  data.i0 = new_heap_instance(&runtime);
  data.i1 = new_heap_instance(&runtime);
  value_t i2 = new_heap_instance(&runtime);

  value_mapping_t resolver;
  value_mapping_init(&resolver, value_to_int, &data);
  value_mapping_t access;
  value_mapping_init(&access, int_to_value, &data);

  value_t d0 = transcode_plankton(&runtime, &resolver, &access, data.i0);
  ASSERT_TRUE(value_are_identical(data.i0, d0));
  value_t d1 = transcode_plankton(&runtime, &resolver, &access, data.i1);
  ASSERT_TRUE(value_are_identical(data.i1, d1));
  value_t d2 = transcode_plankton(&runtime, &resolver, &access, i2);
  ASSERT_FALSE(value_are_identical(i2, d2));

  value_t a0 = new_heap_array(&runtime, 4);
  set_array_at(a0, 0, data.i0);
  set_array_at(a0, 1, data.i1);
  set_array_at(a0, 2, i2);
  set_array_at(a0, 3, data.i0);
  value_t da0 = transcode_plankton(&runtime, &resolver, &access, a0);
  ASSERT_TRUE(value_are_identical(data.i0, get_array_at(da0, 0)));
  ASSERT_TRUE(value_are_identical(data.i1, get_array_at(da0, 1)));
  ASSERT_FALSE(value_are_identical(i2, get_array_at(da0, 2)));
  ASSERT_TRUE(value_are_identical(data.i0, get_array_at(da0, 3)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Writes a tagged plankton string to the given buffer.
static value_t write_string(byte_buffer_t *buf, const char *c_str) {
  string_t str;
  string_init(&str, c_str);
  byte_buffer_append(buf, pString);
  return plankton_wire_encode_string(buf, &str);
}

// Writes an ast factory reference with the given ast type to the given buffer.
static value_t write_ast_factory(byte_buffer_t *buf, const char *ast_type) {
  byte_buffer_append(buf, pEnvironment);
  byte_buffer_append(buf, pArray);
  TRY(plankton_wire_encode_uint32(buf, 2));
  TRY(write_string(buf, "ast"));
  TRY(write_string(buf, ast_type));
  return success();
}

// Deserializes the contents of the given buffer as plankton within the given
// runtime, resolving environment references using a syntax mapping.
static value_t deserialize(runtime_t *runtime, byte_buffer_t *buf) {
  blob_t raw_blob;
  byte_buffer_flush(buf, &raw_blob);
  value_t blob = new_heap_blob_with_data(runtime, &raw_blob);
  value_mapping_t syntax_mapping;
  TRY(init_syntax_mapping(&syntax_mapping, runtime));
  return plankton_deserialize(runtime, &syntax_mapping, blob);
}

TEST(plankton, env_construction) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Environment references resolve correctly to ast factories.
  {
    byte_buffer_t buf;
    byte_buffer_init(&buf, NULL);
    write_ast_factory(&buf, "Literal");
    value_t value = deserialize(&runtime, &buf);
    ASSERT_FAMILY(ofFactory, value);
    byte_buffer_dispose(&buf);
  }

  // Objects with ast factory headers produce asts.
  {
    byte_buffer_t buf;
    byte_buffer_init(&buf, NULL);
    byte_buffer_append(&buf, pObject);
    write_ast_factory(&buf, "Literal");
    byte_buffer_append(&buf, pMap);
    plankton_wire_encode_uint32(&buf, 1);
    write_string(&buf, "value");
    byte_buffer_append(&buf, pTrue);
    value_t value = deserialize(&runtime, &buf);
    ASSERT_FAMILY(ofLiteralAst, value);
    ASSERT_VALEQ(runtime_bool(&runtime, true), get_literal_ast_value(value));
    byte_buffer_dispose(&buf);
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
