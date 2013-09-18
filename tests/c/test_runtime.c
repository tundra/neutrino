// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"
#include "value-inl.h"

// A malloc that refuses to yield any memory.
memory_block_t blocking_malloc(void *data, size_t size) {
  return memory_block_empty();
}

TEST(runtime, create) {
  // Successfully create a runtime->
  runtime_t r0;
  ASSERT_SUCCESS(runtime_init(&r0, NULL));
  ASSERT_SUCCESS(runtime_dispose(&r0));

  // Propagating failure correctly when malloc fails during startup.
  runtime_t r1;
  allocator_t blocker;
  blocker.malloc = blocking_malloc;
  allocator_t *prev_alloc = allocator_set_default(&blocker);
  ASSERT_SIGNAL(scSystemError, runtime_init(&r1, NULL));
  allocator_set_default(prev_alloc);
}

TEST(runtime, singletons) {
  CREATE_RUNTIME();

  value_t null = ROOT(runtime, null);
  ASSERT_FAMILY(ofNull, null);
  value_t thrue = runtime_bool(runtime, true);
  value_t fahlse = runtime_bool(runtime, false);
  ASSERT_FAMILY(ofBoolean, thrue);
  ASSERT_TRUE(get_boolean_value(thrue));
  ASSERT_FAMILY(ofBoolean, fahlse);
  ASSERT_FALSE(get_boolean_value(fahlse));

  DISPOSE_RUNTIME();
}

TEST(runtime, runtime_validation) {
  CREATE_RUNTIME();
  ASSERT_SUCCESS(runtime_validate(runtime));


  // Break a root.
  value_t old_null = ROOT(runtime, null);
  ROOT(runtime, null) = new_integer(0);
  ASSERT_CHECK_FAILURE(scValidationFailed, runtime_validate(runtime));
  ROOT(runtime, null) = old_null;
  ASSERT_SUCCESS(runtime_validate(runtime));

  // Break a non-root.
  size_t capacity = 16;
  value_t map = new_heap_id_hash_map(runtime, capacity);
  ASSERT_SUCCESS(runtime_validate(runtime));
  set_id_hash_map_capacity(map, capacity + 1);
  ASSERT_CHECK_FAILURE(scValidationFailed, runtime_validate(runtime));
  set_id_hash_map_capacity(map, capacity);
  ASSERT_SUCCESS(runtime_validate(runtime));

  DISPOSE_RUNTIME();
}

TEST(runtime, gc_move_null) {
  CREATE_RUNTIME();

  // Check that anything gets moved at all and that we can call behavior
  // correctly.
  object_layout_t layout_before;
  value_t null_before = ROOT(runtime, null);
  get_object_layout(null_before, &layout_before);
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t null_after = ROOT(runtime, null);
  ASSERT_NSAME(null_before, null_after);
  object_layout_t layout_after;
  get_object_layout(null_after, &layout_after);
  ASSERT_EQ(layout_before.size, layout_after.size);
  ASSERT_EQ(layout_before.value_offset, layout_after.value_offset);

  DISPOSE_RUNTIME();
}

TEST(runtime, simple_gc_safe) {
  CREATE_RUNTIME();

  value_t array_before = new_heap_array(runtime, 2);
  set_array_at(array_before, 0, runtime_bool(runtime, true));
  set_array_at(array_before, 1, runtime_bool(runtime, false));
  gc_safe_t *gc_safe = runtime_new_gc_safe(runtime, array_before);
  ASSERT_SAME(array_before, gc_safe_get_value(gc_safe));
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t array_after = gc_safe_get_value(gc_safe);
  ASSERT_NSAME(array_before, array_after);
  ASSERT_VALEQ(runtime_bool(runtime, true), get_array_at(array_after, 0));
  ASSERT_VALEQ(runtime_bool(runtime, false), get_array_at(array_after, 1));

  runtime_dispose_gc_safe(runtime, gc_safe);

  DISPOSE_RUNTIME();
}

TEST(runtime, gc_safe_loop) {
  CREATE_RUNTIME();

  value_t a0b = new_heap_array(runtime, 2);
  value_t a1b = new_heap_array(runtime, 1);
  set_array_at(a0b, 0, a1b);
  set_array_at(a0b, 1, a1b);
  set_array_at(a1b, 0, a0b);
  gc_safe_t *sa0 = runtime_new_gc_safe(runtime, a0b);
  gc_safe_t *sa1 = runtime_new_gc_safe(runtime, a1b);
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t a0a = gc_safe_get_value(sa0);
  value_t a1a = gc_safe_get_value(sa1);
  ASSERT_SAME(a1a, get_array_at(a0a, 0));
  ASSERT_SAME(a1a, get_array_at(a0a, 1));
  ASSERT_SAME(a0a, get_array_at(a1a, 0));
  runtime_dispose_gc_safe(runtime, sa0);
  runtime_dispose_gc_safe(runtime, sa1);

  DISPOSE_RUNTIME();
}

TEST(runtime, gc_fuzzer) {
  static const size_t kMin = 10;
  static const size_t kMean = 100;
  gc_fuzzer_t fuzzer;
  gc_fuzzer_init(&fuzzer, kMin, kMean, 43245);
  size_t ticks_since_last = 0;
  size_t total_failures = 0;
  for (size_t i = 0; i < 65536; i++) {
    if (gc_fuzzer_tick(&fuzzer)) {
      total_failures++;
      ASSERT_TRUE(ticks_since_last >= kMin);
      ticks_since_last = 0;
    } else {
      ticks_since_last++;
    }
  }
  double average = 65536.0 / total_failures;
  double deviation = (average - kMean) / kMean;
  if (deviation < 0)
    deviation = -deviation;
  ASSERT_TRUE(deviation < 0.1);
}
