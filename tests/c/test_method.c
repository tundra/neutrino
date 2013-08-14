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

  DEF_STR(x_str, "x");
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

  DEF_STR(x_str, "x");
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

  size_t count = 8;
  value_t record = new_heap_invocation_record(&runtime, count);
  for (size_t i = 0; i < count; i++) {
    set_invocation_record_tag_at(record, i, new_integer(i));
    set_invocation_record_offset_at(record, i, 10 - i);
  }
  ASSERT_EQ(count, get_invocation_record_argument_count(record));
  for (size_t i = 0; i < count; i++) {
    ASSERT_VALEQ(new_integer(i), get_invocation_record_tag_at(record, i));
    ASSERT_EQ(10 - i, get_invocation_record_offset_at(record, i));
  }

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
