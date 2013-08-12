// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"

TEST(method, identity_guard) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);
  value_t zero = new_integer(0);
  value_t null = runtime_null(&runtime);
  value_t id_zero = new_heap_guard(&runtime, gtId, zero);
  value_t id_null = new_heap_guard(&runtime, gtId, null);

  ASSERT_TRUE(is_score_match(guard_match(&runtime, id_zero, zero, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, id_zero, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, id_null, zero, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, id_null, null, space)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, any_guard) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);
  value_t any_guard = runtime.roots.any_guard;

  ASSERT_TRUE(is_score_match(guard_match(&runtime, any_guard, new_integer(0), space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, any_guard, new_integer(1), space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, any_guard, runtime_null(&runtime), space)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, method_space) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t space = new_heap_method_space(&runtime);
  value_t null = runtime_null(&runtime);
  value_t p1 = new_heap_protocol(&runtime, null);
  value_t p2 = new_heap_protocol(&runtime, null);
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, p1, p2));
  value_t p3 = new_heap_protocol(&runtime, null);
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, p2, p3));
  value_t p4 = new_heap_protocol(&runtime, null);
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, p2, p4));

  ASSERT_EQ(1, get_array_buffer_length(get_protocol_parents(&runtime, space, p1)));
  ASSERT_EQ(2, get_array_buffer_length(get_protocol_parents(&runtime, space, p2)));
  ASSERT_EQ(0, get_array_buffer_length(get_protocol_parents(&runtime, space, p3)));
  ASSERT_EQ(0, get_array_buffer_length(get_protocol_parents(&runtime, space, p4)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

/*
@Test
  public void testSimpleIs() {
    RProtocol sStrP = new RProtocol();
    IHierarchy hierarchy = newHierarchy(
        entry(INT_P, OBJ_P), entry(STR_P, OBJ_P), entry(sStrP, STR_P));
    Guard isInt = Guard.is(INT_P);
    Guard isObj = Guard.is(OBJ_P);
    Guard isStr = Guard.is(STR_P);
    Guard isSStr = Guard.is(sStrP);

    IValue zero = toValue(0);
    assertTrue(Guard.isMatch(isInt.match(zero, hierarchy)));
    assertTrue(Guard.isMatch(isObj.match(zero, hierarchy)));
    assertFalse(Guard.isMatch(isStr.match(zero, hierarchy)));
    assertFalse(Guard.isMatch(isSStr.match(zero, hierarchy)));

    IValue x = toValue("x");
    assertFalse(Guard.isMatch(isInt.match(x, hierarchy)));
    assertTrue(Guard.isMatch(isObj.match(x, hierarchy)));
    assertTrue(Guard.isMatch(isStr.match(x, hierarchy)));
    assertFalse(Guard.isMatch(isSStr.match(x, hierarchy)));

    IValue sStr = withProtocol(sStrP);
    assertFalse(Guard.isMatch(isInt.match(sStr, hierarchy)));
    assertTrue(Guard.isMatch(isObj.match(sStr, hierarchy)));
    assertTrue(Guard.isMatch(isStr.match(sStr, hierarchy)));
    assertTrue(Guard.isMatch(isSStr.match(sStr, hierarchy)));

    IValue nil = toValue(null);
    assertFalse(Guard.isMatch(isInt.match(nil, hierarchy)));
    assertFalse(Guard.isMatch(isObj.match(nil, hierarchy)));
    assertFalse(Guard.isMatch(isStr.match(nil, hierarchy)));
    assertFalse(Guard.isMatch(isSStr.match(nil, hierarchy)));
  }
  */

TEST(method, simple_is) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t s_str_p = new_heap_protocol(&runtime, runtime_null(&runtime));
  value_t obj_p = new_heap_protocol(&runtime, runtime_null(&runtime));
  value_t int_p = runtime.roots.integer_protocol;
  value_t str_p = runtime.roots.string_protocol;
  value_t space = new_heap_method_space(&runtime);
  // int <: obj
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, int_p, obj_p));
  // s-str <: str <: obj
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, str_p, obj_p));
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, s_str_p, str_p));
  value_t is_int = new_heap_guard(&runtime, gtIs, int_p);
  value_t is_obj = new_heap_guard(&runtime, gtIs, obj_p);
  value_t is_str = new_heap_guard(&runtime, gtIs, str_p);
  value_t is_s_str = new_heap_guard(&runtime, gtIs, s_str_p);

  value_t zero = new_integer(0);
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_int, zero, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_obj, zero, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_str, zero, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_s_str, zero, space)));

  DEF_STR(x_str, "x");
  value_t x = new_heap_string(&runtime, &x_str);
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_int, x, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_obj, x, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_str, x, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_s_str, x, space)));

  value_t null = runtime_null(&runtime);
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_int, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_obj, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_str, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_s_str, null, space)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
