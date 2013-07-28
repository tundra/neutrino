#include "alloc.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"

// A malloc that refuses to yield any memory.
void *blocking_malloc(void *data, size_t size) {
  return NULL;
}

TEST(runtime, create) {
  // Successfully create a runtime.
  runtime_t r0;
  ASSERT_SUCCESS(runtime_init(&r0, NULL));
  ASSERT_SUCCESS(runtime_dispose(&r0));

  // Propagating failure correctly when malloc fails during startup.
  runtime_t r1;
  space_config_t config;
  space_config_init_defaults(&config);
  config.allocator.malloc = blocking_malloc;
  ASSERT_SIGNAL(scSystemError, runtime_init(&r1, &config));
}

TEST(runtime, singletons) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t null = runtime_null(&runtime);
  ASSERT_FAMILY(ofNull, null);
  value_t thrue = runtime_bool(&runtime, true);
  value_t fahlse = runtime_bool(&runtime, false);
  ASSERT_FAMILY(ofBool, thrue);
  ASSERT_TRUE(get_bool_value(thrue));
  ASSERT_FAMILY(ofBool, fahlse);
  ASSERT_FALSE(get_bool_value(fahlse));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(runtime, runtime_validation) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));
  ASSERT_SUCCESS(runtime_validate(&runtime));


  // Break a root.
  value_t old_null = runtime_null(&runtime);
  runtime.roots.null = new_integer(0);
  ASSERT_SIGNAL(scValidationFailed, runtime_validate(&runtime));
  runtime.roots.null = old_null;
  ASSERT_SUCCESS(runtime_validate(&runtime));

  // Break a non-root.
  size_t capacity = 16;
  value_t map = new_heap_id_hash_map(&runtime, capacity);
  ASSERT_SUCCESS(runtime_validate(&runtime));
  set_id_hash_map_capacity(map, capacity + 1);
  ASSERT_SIGNAL(scValidationFailed, runtime_validate(&runtime));
  set_id_hash_map_capacity(map, capacity);
  ASSERT_SUCCESS(runtime_validate(&runtime));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}


TEST(runtime, gc) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));
  ASSERT_SUCCESS(runtime_garbage_collect(&runtime));
  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
