// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"

TEST(test, variant) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  ASSERT_VALEQ(new_integer(1), variant_to_value(&runtime, vInt(1)));
  ASSERT_VALEQ(new_integer(-1), variant_to_value(&runtime, vInt(-1)));
  ASSERT_VALEQ(runtime_bool(&runtime, true), variant_to_value(&runtime, vBool(true)));
  ASSERT_VALEQ(runtime_bool(&runtime, false), variant_to_value(&runtime, vBool(false)));
  ASSERT_VALEQ(runtime_null(&runtime), variant_to_value(&runtime, vNull()));

  string_t str = STR("blahblahblah");
  ASSERT_VALEQ(new_heap_string(&runtime, &str), variant_to_value(&runtime,
      vStr("blahblahblah")));

  value_t arr = variant_to_value(&runtime, vArray(3, vInt(0) o vInt(1) o vInt(2)));
  ASSERT_EQ(3, get_array_length(arr));
  ASSERT_VALEQ(new_integer(0), get_array_at(arr, 0));
  ASSERT_VALEQ(new_integer(1), get_array_at(arr, 1));
  ASSERT_VALEQ(new_integer(2), get_array_at(arr, 2));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
