#include "value.h"
#include "test.h"


// Really simple value tagging stuff.
TEST(value, tagged_integers) {
  value_ptr_t v0 = new_integer(10);
  ASSERT_EQ(vtInteger, get_value_tag(v0));
  ASSERT_EQ(10, get_integer_value(v0));
  value_ptr_t v1 = new_integer(-10);
  ASSERT_EQ(vtInteger, get_value_tag(v1));
  ASSERT_EQ(-10, get_integer_value(v1));
  value_ptr_t v2 = new_integer(0);
  ASSERT_EQ(vtInteger, get_value_tag(v2));
  ASSERT_EQ(0, get_integer_value(v2));
}

TEST(value, signals) {
  value_ptr_t v0 = new_signal(scHeapExhausted);
  ASSERT_EQ(vtSignal, get_value_tag(v0));
  ASSERT_EQ(scHeapExhausted, get_signal_cause(v0));
}
