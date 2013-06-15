#include "alloc.h"
#include "test.h"
#include "value.h"

TEST(alloc, heap_string) {
  heap_t heap;
  heap_init(&heap, NULL);

  string_t chars;
  string_init(&chars, "Hut!");
  value_ptr_t str = new_heap_string(&heap, &chars);
  ASSERT_EQ(vtObject, get_value_tag(str));
  ASSERT_EQ(otString, get_object_type(str));
  ASSERT_EQ(4, get_string_length(str));
  string_t out;
  get_string_contents(str, &out);
  ASSERT_TRUE(string_equals(&chars, &out));

  heap_dispose(&heap);
}
