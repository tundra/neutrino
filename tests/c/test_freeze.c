// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "heap.h"
#include "log.h"
#include "runtime.h"
#include "test.h"
#include "try-inl.h"
#include "value-inl.h"

TEST(freeze, deep_freeze) {
  CREATE_RUNTIME();

  value_t zero = new_integer(0);
  ASSERT_TRUE(is_frozen(zero));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, zero, NULL));

  ASSERT_TRUE(is_frozen(null()));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, null(), NULL));

  value_t null_arr = new_heap_array(runtime, 2);
  ASSERT_TRUE(is_mutable(null_arr));
  ASSERT_FALSE(is_frozen(null_arr));
  value_t offender = whatever();
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, null_arr, &offender));
  ASSERT_SAME(null_arr, offender);
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, null_arr));
  ASSERT_FALSE(is_mutable(null_arr));
  ASSERT_TRUE(is_frozen(null_arr));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, null_arr, NULL));

  value_t mut = new_heap_array(runtime, 2);
  value_t mut_arr = new_heap_array(runtime, 2);
  set_array_at(mut_arr, 0, mut);
  ASSERT_TRUE(is_mutable(mut_arr));
  offender = whatever();
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, mut_arr, &offender));
  ASSERT_VALEQ(mut_arr, offender);
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, mut_arr));
  ASSERT_FALSE(is_mutable(mut_arr));
  offender = whatever();
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, mut_arr, &offender));
  ASSERT_SAME(mut, offender);
  offender = whatever();
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, mut_arr, &offender));
  ASSERT_SAME(mut, offender);
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, mut));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, mut_arr, NULL));

  value_t circ_arr = new_heap_array(runtime, 2);
  set_array_at(circ_arr, 0, circ_arr);
  set_array_at(circ_arr, 1, circ_arr);
  ASSERT_TRUE(is_mutable(circ_arr));
  offender = success();
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, circ_arr, &offender));
  ASSERT_SAME(circ_arr, offender);
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, circ_arr));
  ASSERT_FALSE(is_mutable(circ_arr));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, circ_arr, NULL));

  DISPOSE_RUNTIME();
}

TEST(freeze, ownership_freezing) {
  CREATE_RUNTIME();

  value_t empty_map = new_heap_id_hash_map(runtime, 16);
  ASSERT_TRUE(is_mutable(empty_map));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, empty_map));
  ASSERT_TRUE(is_frozen(empty_map));
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, empty_map, NULL));
  ASSERT_SUCCESS(ensure_frozen(runtime, empty_map));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, empty_map, NULL));

  value_t mut = new_heap_array(runtime, 2);
  value_t mut_map = new_heap_id_hash_map(runtime, 16);
  ASSERT_SUCCESS(try_set_id_hash_map_at(mut_map, new_integer(0), mut, false));
  ASSERT_TRUE(is_mutable(mut_map));
  ASSERT_SUCCESS(ensure_shallow_frozen(runtime, mut_map));
  ASSERT_SUCCESS(ensure_frozen(runtime, mut_map));
  value_t offender = new_integer(0);
  ASSERT_VALEQ(no(), try_validate_deep_frozen(runtime, mut_map, &offender));
  ASSERT_SAME(mut, offender);
  ASSERT_SUCCESS(ensure_frozen(runtime, mut));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, mut_map, NULL));

  DISPOSE_RUNTIME();
}

TEST(freeze, freeze_cheat) {
  CREATE_RUNTIME();

  value_t cheat = new_heap_freeze_cheat(runtime, new_integer(121));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, cheat, NULL));
  ASSERT_VALEQ(new_integer(121), get_freeze_cheat_value(cheat));
  set_freeze_cheat_value(cheat, new_integer(212));
  ASSERT_VALEQ(yes(), try_validate_deep_frozen(runtime, cheat, NULL));
  ASSERT_VALEQ(new_integer(212), get_freeze_cheat_value(cheat));

  DISPOSE_RUNTIME();
}
