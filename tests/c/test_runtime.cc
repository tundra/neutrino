//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test.hh"

BEGIN_C_INCLUDES
#include "alloc.h"
#include "runtime.h"
#include "safe-inl.h"
#include "value-inl.h"
END_C_INCLUDES

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
  ASSERT_CONDITION(ccSystemError, runtime_init(&r1, NULL));
  allocator_set_default(prev_alloc);
}

TEST(runtime, singletons) {
  CREATE_RUNTIME();

  value_t thrue = yes();
  value_t fahlse = no();
  ASSERT_TRUE(get_boolean_value(thrue));
  ASSERT_FALSE(get_boolean_value(fahlse));

  DISPOSE_RUNTIME();
}

TEST(runtime, runtime_validation) {
  CREATE_RUNTIME();
  ASSERT_SUCCESS(runtime_validate(runtime, nothing()));

  // Break a root.
  value_t old_empty_array = ROOT(runtime, empty_array);
  ROOT(runtime, empty_array) = new_integer(0);
  ASSERT_CHECK_FAILURE(ccValidationFailed, runtime_validate(runtime, nothing()));
  ROOT(runtime, empty_array) = old_empty_array;
  ASSERT_SUCCESS(runtime_validate(runtime, nothing()));

  // Break a non-root.
  size_t capacity = 16;
  value_t map = new_heap_id_hash_map(runtime, capacity);
  ASSERT_SUCCESS(runtime_validate(runtime, nothing()));
  set_id_hash_map_capacity(map, capacity + 1);
  ASSERT_CHECK_FAILURE(ccValidationFailed, runtime_validate(runtime, nothing()));
  set_id_hash_map_capacity(map, capacity);
  ASSERT_SUCCESS(runtime_validate(runtime, nothing()));

  DISPOSE_RUNTIME();
}

TEST(runtime, gc_move_null) {
  CREATE_RUNTIME();

  // Check that anything gets moved at all and that we can call behavior
  // correctly.
  heap_object_layout_t layout_before;
  value_t empty_array_before = ROOT(runtime, empty_array);
  get_heap_object_layout(empty_array_before, &layout_before);
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t empty_array_after = ROOT(runtime, empty_array);
  ASSERT_NSAME(empty_array_before, empty_array_after);
  heap_object_layout_t layout_after;
  get_heap_object_layout(empty_array_after, &layout_after);
  ASSERT_EQ(layout_before.size, layout_after.size);
  ASSERT_EQ(layout_before.value_offset, layout_after.value_offset);

  DISPOSE_RUNTIME();
}

TEST(runtime, safe_value_loop) {
  CREATE_RUNTIME();

  value_t a0b = new_heap_array(runtime, 2);
  value_t a1b = new_heap_array(runtime, 1);
  set_array_at(a0b, 0, a1b);
  set_array_at(a0b, 1, a1b);
  set_array_at(a1b, 0, a0b);
  safe_value_t s_a0 = runtime_protect_value(runtime, a0b);
  safe_value_t s_a1 = runtime_protect_value(runtime, a1b);
  ASSERT_SUCCESS(runtime_garbage_collect(runtime));
  value_t a0a = deref(s_a0);
  value_t a1a = deref(s_a1);
  ASSERT_SAME(a1a, get_array_at(a0a, 0));
  ASSERT_SAME(a1a, get_array_at(a0a, 1));
  ASSERT_SAME(a0a, get_array_at(a1a, 0));
  dispose_safe_value(runtime, s_a0);
  dispose_safe_value(runtime, s_a1);

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

static void check_species_with_mode(runtime_t *runtime, value_t other,
    value_mode_t target_mode) {
  value_t target = get_modal_species_sibling_with_mode(runtime, other, target_mode);
  ASSERT_EQ(target_mode, get_modal_species_mode(target));
  ASSERT_EQ(get_species_instance_family(other), get_species_instance_family(target));
}

TEST(runtime, modal_species_change) {
  CREATE_RUNTIME();

  value_t fluid = ROOT(runtime, fluid_array_species);
  check_species_with_mode(runtime, fluid, vmFluid);
  check_species_with_mode(runtime, fluid, vmMutable);
  check_species_with_mode(runtime, fluid, vmFrozen);
  check_species_with_mode(runtime, fluid, vmDeepFrozen);
  value_t muhtable = ROOT(runtime, mutable_array_species);
  check_species_with_mode(runtime, muhtable, vmFluid);
  check_species_with_mode(runtime, muhtable, vmMutable);
  check_species_with_mode(runtime, muhtable, vmFrozen);
  check_species_with_mode(runtime, muhtable, vmDeepFrozen);
  value_t frozen = ROOT(runtime, frozen_array_species);
  check_species_with_mode(runtime, frozen, vmFluid);
  check_species_with_mode(runtime, frozen, vmMutable);
  check_species_with_mode(runtime, frozen, vmFrozen);
  check_species_with_mode(runtime, frozen, vmDeepFrozen);
  value_t deep_frozen = ROOT(runtime, deep_frozen_array_species);
  check_species_with_mode(runtime, deep_frozen, vmFluid);
  check_species_with_mode(runtime, deep_frozen, vmMutable);
  check_species_with_mode(runtime, deep_frozen, vmFrozen);
  check_species_with_mode(runtime, deep_frozen, vmDeepFrozen);

  DISPOSE_RUNTIME();
}

TEST(runtime, ambience_gc) {
  CREATE_RUNTIME();
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);

  value_t stage = new_stage_offset(11);
  safe_value_t s_ambience = protect(pool, ambience);
  ASSERT_FAMILY(ofAmbience, deref(s_ambience));
  safe_value_t s_fragment = protect(pool, new_heap_module_fragment(runtime,
      stage, nothing(), nothing(), nothing(), nothing(), nothing()));
  set_ambience_present_core_fragment(ambience, deref(s_fragment));
  ASSERT_FAMILY(ofModuleFragment, deref(s_fragment));

  runtime_garbage_collect(runtime);

  ASSERT_FAMILY(ofAmbience, deref(s_ambience));
  ASSERT_FAMILY(ofModuleFragment, deref(s_fragment));
  ASSERT_SAME(deref(s_fragment),
      get_ambience_present_core_fragment(deref(s_ambience)));
  ASSERT_SAME(stage, get_module_fragment_stage(deref(s_fragment)));

  DISPOSE_SAFE_VALUE_POOL(pool);
  DISPOSE_RUNTIME();
}
