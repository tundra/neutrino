#include "plankton.h"
#include "test.h"

TEST(plankton, simple) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t value = new_integer(1);
  value_t encoded = plankton_serialize(&runtime, value);
  value_t decoded = plankton_deserialize(&runtime, encoded);
  value_print_ln(encoded);
  value_print_ln(decoded);
  ASSERT_EQ(value, decoded);

  runtime_dispose(&runtime);
}
