#include "plankton.h"
#include "test.h"

// Encodes and decodes a plankton value and checks that the result is the
// same as the input.
static void check_plankton(value_t value) {
  // Create a runtime.
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));
  // Encode and decode the value.
  value_t encoded = plankton_serialize(&runtime, value);
  value_t decoded = plankton_deserialize(&runtime, encoded);
  ASSERT_EQ(value, decoded);
  // Clean up.
  runtime_dispose(&runtime);
}

TEST(plankton, simple) {
  check_plankton(new_integer(0));
  check_plankton(new_integer(1));
  check_plankton(new_integer(-1));
  check_plankton(new_integer(65536));
  check_plankton(new_integer(-65536));
}
