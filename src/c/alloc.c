#include "alloc.h"

value_ptr_t new_heap_string(heap_t *heap, string_t *contents) {
  size_t bytes = calc_string_size(string_length(contents));
  value_ptr_t result = alloc_heap_object(heap, bytes, otString);
  TRY(result);
  set_string_length(result, string_length(contents));
  string_copy_to(contents, get_string_chars(result), string_length(contents) + 1);
  return result;
}

value_ptr_t alloc_heap_object(heap_t *heap, size_t bytes, object_type_t type) {
  address_t addr;
  if (!heap_try_alloc(heap, bytes, &addr))
    return new_signal(scHeapExhausted);
  value_ptr_t result = new_object(addr);
  set_object_type(result, type);
  return result;  
}
