#include "runtime.h"
#include "test.h"
#include "value-inl.h"

// A malloc that refuses to yield any memory.
address_t blocking_malloc(void *data, size_t size) {
  return NULL;
}

TEST(runtime, create) {
  // Successfully create a runtime.
  runtime_t r0;
  ASSERT_SUCCESS(runtime_init(&r0, NULL));
  runtime_dispose(&r0);

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

  runtime_dispose(&runtime);
}

TEST(runtime, runtime_validation) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  // Initially it validates.
  ASSERT_SUCCESS(runtime_validate(&runtime));

  // Break this runtime.
  runtime.roots.null = new_integer(0);

  // Now it no longer validates.
  ASSERT_SIGNAL(scValidationFailed, runtime_validate(&runtime));

  runtime_dispose(&runtime);
}
