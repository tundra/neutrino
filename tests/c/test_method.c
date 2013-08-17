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
  value_t id_zero = new_heap_guard(&runtime, gtEq, zero);
  value_t id_null = new_heap_guard(&runtime, gtEq, null);

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

  value_t is_x = new_heap_guard(&runtime, gtEq, x);
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
  value_t signature = new_heap_signature(&runtime, empty_array, 0, 0, false);
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

// Description of a parameter used for testing.
typedef struct {
  value_t guard;
  bool is_optional;
  variant_t tags;
} test_param_t;

// Packs its arguments into a test_param_t struct so they can be passed on to
// make_signature. This is all a bit of a mess but building complex literals is
// tough in C.
#define PARAM(guard, is_optional, tags) ((test_param_t) {guard, is_optional, tags})

// Collects a set of parameters into an array that can be passed to
// make_signature.
#define PARAMS(N, values) ((test_param_t[(N) + 1]) {values, PARAM(new_integer(0), false, vNull())})

// Make a signature object out of the given input.
static value_t make_signature(runtime_t *runtime, bool allow_extra, test_param_t *params) {
  size_t param_count = 0;
  size_t mandatory_count = 0;
  size_t tag_count = 0;
  // Loop over the parameters, stop at the first non-array which will be the end
  // marker added by the PARAMS macro. First we just collect some information,
  // then we'll build the signature.
  for (size_t i = 0; params[i].tags.type == vtArray; i++) {
    test_param_t test_param = params[i];
    param_count++;
    if (!test_param.is_optional)
      mandatory_count++;
    tag_count += test_param.tags.value.as_array.length;
  }
  // Create an array with pairs of values, the first entry of which is the tag
  // and the second is the parameter.
  TRY_DEF(param_vector, new_heap_pair_array(runtime, tag_count));
  // Loop over all the tags, t being the tag index across the whole signature.
  size_t t = 0;
  for (size_t i = 0; params[i].tags.type == vtArray; i++) {
    test_param_t test_param = params[i];
    TRY_DEF(tags_array, variant_to_value(runtime, test_param.tags));
    size_t param_tag_count = get_array_length(tags_array);
    TRY_DEF(param, new_heap_parameter(runtime, test_param.guard, test_param.is_optional,
        i));
    for (size_t j = 0; j < param_tag_count; j++, t++) {
      TRY_DEF(tag, get_array_at(tags_array, j));
      set_pair_array_first_at(param_vector, t, tag);
      set_pair_array_second_at(param_vector, t, param);
    }
  }
  co_sort_pair_array(param_vector);
  return new_heap_signature(runtime, param_vector, param_count, mandatory_count,
      allow_extra);
}

TEST(method, make_signature) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t any_guard = runtime.roots.any_guard;
  value_t s0 = make_signature(&runtime,
      false,
      PARAMS(2,
          PARAM(any_guard, false, vArray(1, vInt(0))) o
          PARAM(any_guard, false, vArray(1, vInt(1)))));
  ASSERT_EQ(2, get_signature_tag_count(s0));
  ASSERT_VAREQ(vInt(0), get_signature_tag_at(s0, 0));
  ASSERT_VAREQ(vInt(1), get_signature_tag_at(s0, 1));

  value_t s1 = make_signature(&runtime,
      false,
      PARAMS(3,
          PARAM(any_guard, false, vArray(1, vInt(6))) o
          PARAM(any_guard, false, vArray(1, vInt(3))) o
          PARAM(any_guard, false, vArray(1, vInt(7)))));
  ASSERT_EQ(3, get_signature_tag_count(s1));
  ASSERT_VAREQ(vInt(3), get_signature_tag_at(s1, 0));
  ASSERT_VAREQ(vInt(6), get_signature_tag_at(s1, 1));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s1, 2));

  value_t s2 = make_signature(&runtime,
      false,
      PARAMS(3,
          PARAM(any_guard, false, vArray(2, vInt(9) o vInt(11))) o
          PARAM(any_guard, false, vArray(1, vInt(13))) o
          PARAM(any_guard, false, vArray(3, vInt(15) o vInt(7) o vInt(27)))));
  ASSERT_EQ(6, get_signature_tag_count(s2));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s2, 0));
  ASSERT_VAREQ(vInt(9), get_signature_tag_at(s2, 1));
  ASSERT_VAREQ(vInt(11), get_signature_tag_at(s2, 2));
  ASSERT_VAREQ(vInt(13), get_signature_tag_at(s2, 3));
  ASSERT_VAREQ(vInt(15), get_signature_tag_at(s2, 4));
  ASSERT_VAREQ(vInt(27), get_signature_tag_at(s2, 5));

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}

// Description of an argument used in testing.
typedef struct {
  variant_t tag;
  variant_t value;
  bool is_valid;
} test_argument_t;

// Packs its arguments into a test_argument_t struct.
#define ARG(tag, value) ((test_argument_t) {tag, value, true})

// Collects a set of arguments into an array.
#define ARGS(N, args) ((test_argument_t[(N) + 1]) {args, ((test_argument_t) {vInt(0), vInt(0), false})})

void assert_match(runtime_t *runtime, match_result_t expected, value_t signature,
    test_argument_t *args) {
  // Count how many arguments there are.
  size_t arg_count = 0;
  for (size_t i = 0; args[i].is_valid; i++)
    arg_count++;
  // Build a descriptor from the tags and a stack from the values.
  value_t tags = new_heap_array(runtime, arg_count);
  value_t stack = new_heap_stack(runtime, 16);
  frame_t frame;
  push_stack_frame(runtime, stack, &frame, arg_count);
  for (size_t i = 0; i < arg_count; i++) {
    test_argument_t arg = args[i];
    set_array_at(tags, i, variant_to_value(runtime, arg.tag));
    frame_push_value(&frame, variant_to_value(runtime, arg.value));
  }
  value_t vector = build_invocation_record_vector(runtime, tags);
  value_t record = new_heap_invocation_record(runtime, vector);
  score_t scores[16];
  match_result_t result = match_signature(signature, record, &frame, scores, 16);
  ASSERT_EQ(expected, result);
}

TEST(method, simple_matching) {
  runtime_t runtime;
  ASSERT_SUCCESS(runtime_init(&runtime, NULL));

  value_t any_guard = runtime.roots.any_guard;
  value_t sig = make_signature(&runtime,
      false,
      PARAMS(2,
          PARAM(any_guard, false, vArray(1, vInt(0))) o
          PARAM(any_guard, false, vArray(1, vInt(1)))));

  assert_match(&runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar"))));

  assert_match(&runtime, mrUnexpectedArgument, sig, ARGS(3,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar")) o
      ARG(vInt(2), vStr("baz"))));

//      assertMatch(UNEXPECTED_ARGUMENT, sig, arg(0, "foo"), arg(1, "bar"), arg(2, "baz"));
//      assertMatch(MISSING_ARGUMENT, sig, arg(0, "foo"));
//      assertMatch(MISSING_ARGUMENT, sig, arg(1, "bar"));
//      assertMatch(UNEXPECTED_ARGUMENT, sig, arg(2, "baz"));
//      assertMatch(MISSING_ARGUMENT, sig);

  ASSERT_SUCCESS(runtime_dispose(&runtime));
}
