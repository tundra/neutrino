#include "alloc.h"
#include "heap.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"


// Really simple value tagging stuff.
TEST(value, tagged_integers) {
  value_t v0 = new_integer(10);
  ASSERT_DOMAIN(vdInteger, v0);
  ASSERT_EQ(10, get_integer_value(v0));
  value_t v1 = new_integer(-10);
  ASSERT_DOMAIN(vdInteger, v1);
  ASSERT_EQ(-10, get_integer_value(v1));
  value_t v2 = new_integer(0);
  ASSERT_DOMAIN(vdInteger, v2);
  ASSERT_EQ(0, get_integer_value(v2));
}

TEST(value, signals) {
  value_t v0 = new_signal(scHeapExhausted);
  ASSERT_DOMAIN(vdSignal, v0);
  ASSERT_EQ(scHeapExhausted, get_signal_cause(v0));  
}

TEST(value, objects) {
  heap_t heap;
  heap_init(&heap, NULL);

  address_t addr;
  ASSERT_TRUE(heap_try_alloc(&heap, 16, &addr));
  value_t v0 = new_object(addr);
  ASSERT_DOMAIN(vdObject, v0);
  ASSERT_EQ(addr, get_object_address(v0));

  heap_dispose(&heap);
}

TEST(runtime, string_validation) {
  runtime_t r;
  ASSERT_FALSE(in_domain(vdSignal, runtime_init(&r, NULL)));

  string_t chars;
  string_init(&chars, "Hut!");
  value_t str = new_heap_string(&r, &chars);

  // String starts out validating.
  ASSERT_FALSE(in_domain(vdSignal, object_validate(str)));

  // Zap the null terminator.
  get_string_chars(str)[4] = 'x';

  // Now the string no longer terminates.
  ASSERT_SIGNAL(scValidationFailed, object_validate(str));

  runtime_dispose(&r);
}
 