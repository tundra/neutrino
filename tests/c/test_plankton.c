#include "plankton.h"
#include "test.h"

TEST(plankton, simple) {
  value_t value = new_integer(1);
  value_t encoded = plankton_serialize(value);
  value_t decoded = plankton_deserialize(encoded);
  ASSERT_EQ(value, decoded);
}
