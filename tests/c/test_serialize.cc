//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"
#include "plankton-inl.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "plankton.h"
#include "safe-inl.h"
#include "serialize.h"
#include "try-inl.h"
#include "value-inl.h"
END_C_INCLUDES

// Encodes and decodes a plankton value and returns the result.
static value_t transcode_plankton(runtime_t *runtime, value_t value) {
  // Encode and decode the value.
  value_t encoded = plankton_serialize_to_blob(runtime, value);
  ASSERT_SUCCESS(encoded);
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);
  safe_value_t s_encoded = protect(pool, encoded);
  value_t decoded = plankton_deserialize_blob(runtime, NULL, s_encoded);
  ASSERT_SUCCESS(decoded);
  DISPOSE_SAFE_VALUE_POOL(pool);
  return decoded;
}

// Encodes and decodes a plankton value and checks that the result is the
// same as the input. Returns the decoded value.
static value_t check_plankton(runtime_t *runtime, value_t value) {
  value_t decoded = transcode_plankton(runtime, value);
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
  value_t d0 = transcode_plankton(runtime, i0);
  ASSERT_SAME(d0, get_instance_field(d0, k0));

  value_t i1 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i2 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t i3 = new_heap_instance(runtime, ROOT(runtime, empty_instance_species));
  value_t k1 = new_integer(79);
  ASSERT_SUCCESS(set_instance_field(runtime, i1, k0, i2));
  ASSERT_SUCCESS(set_instance_field(runtime, i1, k1, i3));
  ASSERT_SUCCESS(set_instance_field(runtime, i2, k1, i3));
  ASSERT_SUCCESS(set_instance_field(runtime, i3, k0, i1));
  value_t d1 = transcode_plankton(runtime, i1);
  value_t d2 = get_instance_field(d1, k0);
  value_t d3 = get_instance_field(d1, k1);
  ASSERT_NSAME(d1, d2);
  ASSERT_NSAME(d1, d3);
  ASSERT_SAME(d3, get_instance_field(d2, k1));
  ASSERT_SAME(d1, get_instance_field(d3, k0));


  DISPOSE_RUNTIME();
}

static plankton::Variant test_data_gen(plankton::Factory *factory, size_t depth,
    size_t seed) {
  if (depth == 0)
    return (seed % 239);
  switch (depth % 3) {
    case 0: {
      plankton::Array result = factory->new_array();
      result.add(test_data_gen(factory, depth - 1, (3 * seed) + 5));
      result.add(test_data_gen(factory, depth - 1, (5 * seed) + 7));
      return result;
    }
    case 1: {
      size_t root = (seed % 257);
      plankton::Map result = factory->new_map();
      result.set(root, test_data_gen(factory, depth - 1, (11 * seed) + 13));
      result.set(root + 1, test_data_gen(factory, depth - 1, (13 * seed) + 17));
      return result;
    }
    default: {
      size_t root = (seed % 259);
      plankton::Seed result = factory->new_seed();
      result.set_header(test_data_gen(factory, depth - 1, (17 * seed) + 19));
      result.set_field(root, test_data_gen(factory, depth - 1, (19 * seed) + 23));
      result.set_field(root + 3, test_data_gen(factory, depth - 1, (23 * seed) + 29));
      return result;
    }
  }
}

static void test_data_validate(value_t value, size_t depth, size_t seed) {
  if (depth == 0) {
    ASSERT_EQ(seed % 239, get_integer_value(value));
    return;
  }
  switch (depth % 3) {
    case 0: {
      ASSERT_TRUE(in_family(ofArray, value));
      test_data_validate(get_array_at(value, 0), depth - 1, (3 * seed) + 5);
      test_data_validate(get_array_at(value, 1), depth - 1, (5 * seed) + 7);
      break;
    }
    case 1: {
      ASSERT_TRUE(in_family(ofIdHashMap, value));
      ASSERT_EQ(2, get_id_hash_map_size(value));
      size_t root = (seed % 257);
      test_data_validate(get_id_hash_map_at(value, new_integer(root)), depth - 1,
          (11 * seed) + 13);
      test_data_validate(get_id_hash_map_at(value, new_integer(root + 1)),
          depth - 1, (13 * seed) + 17);
      break;
    }
    default: {
      ASSERT_TRUE(in_family(ofSeed, value));
      value_t payload = get_seed_payload(value);
      ASSERT_EQ(2, get_id_hash_map_size(payload));
      size_t root = (seed % 259);
      test_data_validate(get_seed_header(value), depth - 1, (17 * seed) + 19);
      test_data_validate(get_id_hash_map_at(payload, new_integer(root)), depth - 1,
          (19 * seed) + 23);
      test_data_validate(get_id_hash_map_at(payload, new_integer(root + 3)),
          depth - 1, (23 * seed) + 29);
      break;
    }
  }
}

static opaque_t add_one(opaque_t opaque_count, opaque_t) {
  size_t *count = (size_t*) o2p(opaque_count);
  (*count)++;
  return opaque_null();
}

TEST(serialize, collection) {
  extended_runtime_config_t config = *extended_runtime_config_get_default();
  config.base.semispace_size_bytes = 650 * kKB;
  CREATE_RUNTIME_WITH_CONFIG(&config);
  CREATE_SAFE_VALUE_POOL(runtime, 1, pool);

  // Build a large blob containing an encoded plankton value.
  safe_value_t s_blob;
  {
    plankton::Arena arena;
    plankton::Variant value = test_data_gen(&arena, 9, 76647);
    plankton::BinaryWriter writer;
    writer.write(value);
    value_t blob = new_heap_blob_with_data(runtime, new_blob(*writer, writer.size()));
    ASSERT_SUCCESS(blob);
    s_blob = protect(pool, blob);
  }

  // It definitely should be possible to deserialize once immediately after a
  // collection, otherwise the heap needs to be bigger.
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  ASSERT_SUCCESS(plankton_deserialize_blob(runtime, NULL, s_blob));

  size_t gc_count = 0;
  runtime_observer_t observer = runtime_observer_empty();
  observer.on_gc_done = unary_callback_new_1(add_one, p2o(&gc_count));
  runtime_push_observer(runtime, &observer);

  // Repeatedly deserialize the value. This should cause gcs while the
  // deserialization is going on.
  for (size_t i = 0; i < 10; i++) {
    value_t decoded = plankton_deserialize_blob(runtime, NULL, s_blob);
    ASSERT_SUCCESS(decoded);
    test_data_validate(decoded, 9, 76647);
  }

  // Assert at least a certain number of collections. The concrete number isn't
  // all that important, it just has to be nontrivial.
  ASSERT_TRUE(gc_count > 10);

  runtime_pop_observer(runtime, &observer);
  callback_destroy(observer.on_gc_done);
  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_RUNTIME();
}
