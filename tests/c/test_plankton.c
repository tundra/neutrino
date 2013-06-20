#include "alloc.h"
#include "plankton.h"
#include "test.h"

// Encodes and decodes a plankton value and checks that the result is the
// same as the input.
static void check_plankton(runtime_t *runtime, value_t value) {
  // Encode and decode the value.
  value_t encoded = plankton_serialize(runtime, value);
  value_t decoded = plankton_deserialize(runtime, encoded);
  ASSERT_VALEQ(value, decoded);
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
value_t name = new_heap_string(&runtime, &name##_chars);

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
