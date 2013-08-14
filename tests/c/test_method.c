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

// Returns a new instance with the given primary protocol.
static value_t new_instance_of(runtime_t *runtime, value_t proto) {
  CHECK_FAMILY(ofProtocol, proto);
  value_t species = new_heap_instance_species(runtime, proto);
  return new_heap_instance(runtime, species);
}

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

  string_t x_str = STR("x");
  value_t x = new_heap_string(&runtime, &x_str);
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_int, x, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_obj, x, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_str, x, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_s_str, x, space)));

  value_t s_str = new_instance_of(&runtime, s_str_p);
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_int, s_str, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_obj, s_str, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_str, s_str, space)));
  ASSERT_TRUE(is_score_match(guard_match(&runtime, is_s_str, s_str, space)));

  value_t null = runtime_null(&runtime);
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_int, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_obj, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_str, null, space)));
  ASSERT_FALSE(is_score_match(guard_match(&runtime, is_s_str, null, space)));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Compare the score of matching guard GA against VA with the score of
// matching GB against VB.
#define ASSERT_COMPARE(GA, VA, REL, GB, VB)                                    \
ASSERT_TRUE(compare_scores(guard_match(&runtime, GA, VA, space),             \
    guard_match(&runtime, GB, VB, space)) REL 0)

TEST(method, is_score) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t s_str_p = new_heap_protocol(&runtime, runtime_null(&runtime));
  value_t obj_p = new_heap_protocol(&runtime, runtime_null(&runtime));
  value_t str_p = runtime.roots.string_protocol;
  value_t space = new_heap_method_space(&runtime);
  // s-str <: str <: obj
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, str_p, obj_p));
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, s_str_p, str_p));

  string_t x_str = STR("x");
  value_t x = new_heap_string(&runtime, &x_str);
  value_t s_str = new_instance_of(&runtime, s_str_p);

  value_t is_x = new_heap_guard(&runtime, gtId, x);
  value_t is_obj = new_heap_guard(&runtime, gtIs, obj_p);
  value_t is_str = new_heap_guard(&runtime, gtIs, str_p);
  value_t is_s_str = new_heap_guard(&runtime, gtIs, s_str_p);

  ASSERT_COMPARE(is_str, x, <, is_obj, x);
  ASSERT_COMPARE(is_x, x, <, is_str, x);
  ASSERT_COMPARE(is_str, s_str, <, is_obj, s_str);
  ASSERT_COMPARE(is_s_str, s_str, <, is_str, s_str);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, multi_score) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t int_str_p = new_heap_protocol(&runtime, runtime_null(&runtime));
  value_t int_p = runtime.roots.integer_protocol;
  value_t str_p = runtime.roots.string_protocol;
  value_t space = new_heap_method_space(&runtime);
  value_t is_str = new_heap_guard(&runtime, gtIs, str_p);
  value_t is_int = new_heap_guard(&runtime, gtIs, int_p);
  // int-str <: int, str
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, int_str_p, int_p));
  ASSERT_SUCCESS(add_method_space_inheritance(&runtime, space, int_str_p, str_p));

  value_t int_str = new_instance_of(&runtime, int_str_p);

  ASSERT_COMPARE(is_str, int_str, ==, is_int, int_str);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

#undef ASSERT_COMPARE

TEST(method, signature) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t empty_array = runtime.roots.empty_array;
  value_t signature = new_heap_signature(&runtime, empty_array, empty_array,
      0, 0, false);
  ASSERT_SUCCESS(signature);

  value_t param = new_heap_parameter(&runtime, runtime.roots.any_guard,
      true, 0);
  ASSERT_SUCCESS(param);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, invocation_record) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

#define kCount 8
  variant_t raw_array = vArray(kCount, vInt(7) o vInt(6) o vInt(5) o vInt(4) o
      vInt(3) o vInt(2) o vInt(1) o vInt(0));
  value_t argument_vector = build_invocation_record_vector(&runtime,
      variant_to_value(&runtime, raw_array));
  value_t record = new_heap_invocation_record(&runtime, argument_vector);
  ASSERT_EQ(kCount, get_invocation_record_argument_count(record));
  for (size_t i = 0; i < kCount; i++) {
    ASSERT_VALEQ(new_integer(i), get_invocation_record_tag_at(record, i));
    ASSERT_EQ(i, get_invocation_record_offset_at(record, i));
  }
#undef kCount

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Makes an invocation record for the given array of tags, passed as a variant
// for convenience.
static value_t make_invocation_record(runtime_t *runtime, variant_t variant) {
  TRY_DEF(argument_vector, build_invocation_record_vector(runtime,
      variant_to_value(runtime, variant)));
  return new_heap_invocation_record(runtime, argument_vector);
}

TEST(method, make_invocation_record) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t record = make_invocation_record(&runtime, vArray(3, vStr("z") o
      vStr("x") o vStr("y")));
  ASSERT_VAREQ(vStr("x"), get_invocation_record_tag_at(record, 0));
  ASSERT_VAREQ(vStr("y"), get_invocation_record_tag_at(record, 1));
  ASSERT_VAREQ(vStr("z"), get_invocation_record_tag_at(record, 2));
  ASSERT_EQ(1, get_invocation_record_offset_at(record, 0));
  ASSERT_EQ(0, get_invocation_record_offset_at(record, 1));
  ASSERT_EQ(2, get_invocation_record_offset_at(record, 2));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

TEST(method, invocation_record_with_stack) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t stack = new_heap_stack(&runtime, 16);
  frame_t frame;
  ASSERT_SUCCESS(push_stack_frame(&runtime, stack, &frame, 3));
  value_t record = make_invocation_record(&runtime, vArray(3, vStr("b") o
      vStr("c") o vStr("a")));
  frame_push_value(&frame, new_integer(7));
  frame_push_value(&frame, new_integer(8));
  frame_push_value(&frame, new_integer(9));
  ASSERT_VAREQ(vInt(9), get_invocation_record_argument_at(record, &frame, 0));
  ASSERT_VAREQ(vInt(7), get_invocation_record_argument_at(record, &frame, 1));
  ASSERT_VAREQ(vInt(8), get_invocation_record_argument_at(record, &frame, 2));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
