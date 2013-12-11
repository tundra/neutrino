// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "test.h"
#include "try-inl.h"

// Checks that scoring value against guard gives a match iff is_match is true.
#define ASSERT_MATCH(is_match, guard, value) do {                              \
  score_t match;                                                               \
  ASSERT_SUCCESS(guard_match(runtime, guard, value, space, &match));           \
  ASSERT_EQ(is_match, is_score_match(match));                                  \
} while (false)

TEST(method, identity_guard) {
  CREATE_RUNTIME();

  value_t space = new_heap_methodspace(runtime);
  value_t zero = new_integer(0);
  value_t id_zero = new_heap_guard(runtime, afFreeze, gtEq, zero);
  value_t id_null = new_heap_guard(runtime, afFreeze, gtEq, null());

  ASSERT_MATCH(true, id_zero, zero);
  ASSERT_MATCH(false, id_zero, null());
  ASSERT_MATCH(false, id_null, zero);
  ASSERT_MATCH(true, id_null, null());

  DISPOSE_RUNTIME();
}

TEST(method, any_guard) {
  CREATE_RUNTIME();

  value_t space = new_heap_methodspace(runtime);
  value_t any_guard = ROOT(runtime, any_guard);

  ASSERT_MATCH(true, any_guard, new_integer(0));
  ASSERT_MATCH(true, any_guard, new_integer(1));
  ASSERT_MATCH(true, any_guard, null());

  DISPOSE_RUNTIME();
}

TEST(method, method_space) {
  CREATE_RUNTIME();

  value_t space = new_heap_methodspace(runtime);
  value_t p1 = new_heap_type(runtime, afFreeze, null());
  value_t p2 = new_heap_type(runtime, afFreeze, null());
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, p1, p2));
  value_t p3 = new_heap_type(runtime, afFreeze, null());
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, p2, p3));
  value_t p4 = new_heap_type(runtime, afFreeze, null());
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, p2, p4));

  ASSERT_EQ(1, get_array_buffer_length(get_type_parents(runtime, space, p1)));
  ASSERT_EQ(2, get_array_buffer_length(get_type_parents(runtime, space, p2)));
  ASSERT_EQ(0, get_array_buffer_length(get_type_parents(runtime, space, p3)));
  ASSERT_EQ(0, get_array_buffer_length(get_type_parents(runtime, space, p4)));

  DISPOSE_RUNTIME();
}

// Returns a new instance with the given primary type.
static value_t new_instance_of(runtime_t *runtime, value_t proto) {
  CHECK_FAMILY(ofType, proto);
  value_t species = new_heap_instance_species(runtime, proto, nothing());
  return new_heap_instance(runtime, species);
}

TEST(method, simple_is) {
  CREATE_RUNTIME();

  value_t s_str_p = new_heap_type(runtime, afFreeze, null());
  value_t obj_p = new_heap_type(runtime, afFreeze, null());
  value_t int_p = ROOT(runtime, integer_type);
  value_t str_p = ROOT(runtime, string_type);
  value_t space = new_heap_methodspace(runtime);
  // int <: obj
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, int_p, obj_p));
  // s-str <: str <: obj
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, str_p, obj_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, s_str_p, str_p));
  value_t is_int = new_heap_guard(runtime, afFreeze, gtIs, int_p);
  value_t is_obj = new_heap_guard(runtime, afFreeze, gtIs, obj_p);
  value_t is_str = new_heap_guard(runtime, afFreeze, gtIs, str_p);
  value_t is_s_str = new_heap_guard(runtime, afFreeze, gtIs, s_str_p);

  value_t zero = new_integer(0);
  ASSERT_MATCH(true, is_int, zero);
  ASSERT_MATCH(true, is_obj, zero);
  ASSERT_MATCH(false, is_str, zero);
  ASSERT_MATCH(false, is_s_str, zero);

  string_t x_str = STR("x");
  value_t x = new_heap_string(runtime, &x_str);
  ASSERT_MATCH(false, is_int, x);
  ASSERT_MATCH(true, is_obj, x);
  ASSERT_MATCH(true, is_str, x);
  ASSERT_MATCH(false, is_s_str, x);

  value_t s_str = new_instance_of(runtime, s_str_p);
  ASSERT_MATCH(false, is_int, s_str);
  ASSERT_MATCH(true, is_obj, s_str);
  ASSERT_MATCH(true, is_str, s_str);
  ASSERT_MATCH(true, is_s_str, s_str);

  ASSERT_MATCH(false, is_int, null());
  ASSERT_MATCH(false, is_obj, null());
  ASSERT_MATCH(false, is_str, null());
  ASSERT_MATCH(false, is_s_str, null());

  DISPOSE_RUNTIME();
}

// Compare the score of matching guard GA against VA with the score of
// matching GB against VB.
#define ASSERT_COMPARE(GA, VA, REL, GB, VB) do {                               \
  score_t score_a;                                                             \
  ASSERT_SUCCESS(guard_match(runtime, GA, VA, space, &score_a));               \
  score_t score_b;                                                             \
  ASSERT_SUCCESS(guard_match(runtime, GB, VB, space, &score_b));               \
  ASSERT_TRUE(compare_scores(score_a, score_b) REL 0);                         \
} while (false)

TEST(method, is_score) {
  CREATE_RUNTIME();

  value_t s_str_p = new_heap_type(runtime, afFreeze, null());
  value_t obj_p = new_heap_type(runtime, afFreeze, null());
  value_t str_p = ROOT(runtime, string_type);
  value_t space = new_heap_methodspace(runtime);
  // s-str <: str <: obj
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, str_p, obj_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, s_str_p, str_p));

  string_t x_str = STR("x");
  value_t x = new_heap_string(runtime, &x_str);
  value_t s_str = new_instance_of(runtime, s_str_p);

  value_t is_x = new_heap_guard(runtime, afFreeze, gtEq, x);
  value_t is_obj = new_heap_guard(runtime, afFreeze, gtIs, obj_p);
  value_t is_str = new_heap_guard(runtime, afFreeze, gtIs, str_p);
  value_t is_s_str = new_heap_guard(runtime, afFreeze, gtIs, s_str_p);

  ASSERT_COMPARE(is_str, x, <, is_obj, x);
  ASSERT_COMPARE(is_x, x, <, is_str, x);
  ASSERT_COMPARE(is_str, s_str, <, is_obj, s_str);
  ASSERT_COMPARE(is_s_str, s_str, <, is_str, s_str);

  DISPOSE_RUNTIME();
}

TEST(method, multi_score) {
  CREATE_RUNTIME();

  value_t int_str_p = new_heap_type(runtime, afFreeze, null());
  value_t int_p = ROOT(runtime, integer_type);
  value_t str_p = ROOT(runtime, string_type);
  value_t space = new_heap_methodspace(runtime);
  value_t is_str = new_heap_guard(runtime, afFreeze, gtIs, str_p);
  value_t is_int = new_heap_guard(runtime, afFreeze, gtIs, int_p);
  // int-str <: int, str
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, int_str_p, int_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, int_str_p, str_p));

  value_t int_str = new_instance_of(runtime, int_str_p);

  ASSERT_COMPARE(is_str, int_str, ==, is_int, int_str);

  DISPOSE_RUNTIME();
}

#undef ASSERT_COMPARE

TEST(method, signature) {
  CREATE_RUNTIME();

  value_t empty_array = ROOT(runtime, empty_array);
  value_t signature = new_heap_signature(runtime, afFreeze, empty_array, 0, 0,
      false);
  ASSERT_SUCCESS(signature);

  value_t param = new_heap_parameter(runtime, afFreeze, ROOT(runtime, any_guard),
      ROOT(runtime, empty_array), true, 0);
  ASSERT_SUCCESS(param);

  DISPOSE_RUNTIME();
}

TEST(method, invocation_record) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

#define kCount 8
  variant_t *raw_array = vArray(vInt(7), vInt(6), vInt(5), vInt(4), vInt(3),
      vInt(2), vInt(1), vInt(0));
  value_t argument_vector = build_invocation_record_vector(runtime, C(raw_array));
  value_t record = new_heap_invocation_record(runtime, afFreeze, argument_vector);
  ASSERT_EQ(kCount, get_invocation_record_argument_count(record));
  for (size_t i = 0; i < kCount; i++) {
    ASSERT_VALEQ(new_integer(i), get_invocation_record_tag_at(record, i));
    ASSERT_EQ(i, get_invocation_record_offset_at(record, i));
  }
#undef kCount

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

// Makes an invocation record for the given array of tags, passed as a variant
// for convenience.
static value_t make_invocation_record(runtime_t *runtime, variant_t *variant) {
  TRY_DEF(argument_vector, build_invocation_record_vector(runtime, C(variant)));
  return new_heap_invocation_record(runtime, afFreeze, argument_vector);
}

TEST(method, make_invocation_record) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t record = make_invocation_record(runtime, vArray(vStr("z"), vStr("x"),
      vStr("y")));
  ASSERT_VAREQ(vStr("x"), get_invocation_record_tag_at(record, 0));
  ASSERT_VAREQ(vStr("y"), get_invocation_record_tag_at(record, 1));
  ASSERT_VAREQ(vStr("z"), get_invocation_record_tag_at(record, 2));
  ASSERT_EQ(1, get_invocation_record_offset_at(record, 0));
  ASSERT_EQ(0, get_invocation_record_offset_at(record, 1));
  ASSERT_EQ(2, get_invocation_record_offset_at(record, 2));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

TEST(method, record_with_stack) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t stack = new_heap_stack(runtime, 16);
  frame_t frame;
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3));
  value_t record = make_invocation_record(runtime, vArray(vStr("b"), vStr("c"),
      vStr("a")));
  frame_push_value(&frame, new_integer(7));
  frame_push_value(&frame, new_integer(8));
  frame_push_value(&frame, new_integer(9));
  ASSERT_VAREQ(vInt(9), get_invocation_record_argument_at(record, &frame, 0));
  ASSERT_VAREQ(vInt(7), get_invocation_record_argument_at(record, &frame, 1));
  ASSERT_VAREQ(vInt(8), get_invocation_record_argument_at(record, &frame, 2));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

// Description of a parameter used for testing.
typedef struct {
  value_t guard;
  bool is_optional;
  variant_t *tags;
} test_param_t;

// Packs its arguments into a test_param_t struct so they can be passed on to
// make_signature. This is all a bit of a mess but building complex literals is
// tough in C.
#define PARAM(guard, is_optional, tags) ((test_param_t) {guard, is_optional, tags})

// Collects a set of parameters into an array that can be passed to
// make_signature.
#define PARAMS(N, values) ((test_param_t[(N) + 1]) {values, PARAM(new_integer(0), false, vMarker)})

// Make a signature object out of the given input.
static value_t make_signature(runtime_t *runtime, bool allow_extra, test_param_t *params) {
  size_t param_count = 0;
  size_t mandatory_count = 0;
  size_t tag_count = 0;
  // Loop over the parameters, stop at the first non-array which will be the end
  // marker added by the PARAMS macro. First we just collect some information,
  // then we'll build the signature.
  for (size_t i = 0; !variant_is_marker(params[i].tags); i++) {
    test_param_t test_param = params[i];
    param_count++;
    if (!test_param.is_optional)
      mandatory_count++;
    tag_count += test_param.tags->value.as_array.length;
  }
  // Create an array with pairs of values, the first entry of which is the tag
  // and the second is the parameter.
  TRY_DEF(param_vector, new_heap_pair_array(runtime, tag_count));
  // Loop over all the tags, t being the tag index across the whole signature.
  size_t t = 0;
  for (size_t i = 0; !variant_is_marker(params[i].tags); i++) {
    test_param_t test_param = params[i];
    TRY_DEF(tags_array, C(test_param.tags));
    size_t param_tag_count = get_array_length(tags_array);
    TRY_DEF(param, new_heap_parameter(runtime, afFreeze, test_param.guard,
        ROOT(runtime, empty_array), test_param.is_optional, i));
    for (size_t j = 0; j < param_tag_count; j++, t++) {
      TRY_DEF(tag, get_array_at(tags_array, j));
      set_pair_array_first_at(param_vector, t, tag);
      set_pair_array_second_at(param_vector, t, param);
    }
  }
  co_sort_pair_array(param_vector);
  return new_heap_signature(runtime, afFreeze, param_vector, param_count,
      mandatory_count, allow_extra);
}

TEST(method, make_signature) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t s0 = make_signature(runtime,
      false,
      PARAMS(2,
          PARAM(any_guard, false, vArray(vInt(0))) o
          PARAM(any_guard, false, vArray(vInt(1)))));
  ASSERT_EQ(2, get_signature_tag_count(s0));
  ASSERT_VAREQ(vInt(0), get_signature_tag_at(s0, 0));
  ASSERT_VAREQ(vInt(1), get_signature_tag_at(s0, 1));

  value_t s1 = make_signature(runtime,
      false,
      PARAMS(3,
          PARAM(any_guard, false, vArray(vInt(6))) o
          PARAM(any_guard, false, vArray(vInt(3))) o
          PARAM(any_guard, false, vArray(vInt(7)))));
  ASSERT_EQ(3, get_signature_tag_count(s1));
  ASSERT_VAREQ(vInt(3), get_signature_tag_at(s1, 0));
  ASSERT_VAREQ(vInt(6), get_signature_tag_at(s1, 1));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s1, 2));

  value_t s2 = make_signature(runtime,
      false,
      PARAMS(3,
          PARAM(any_guard, false, vArray(vInt(9), vInt(11))) o
          PARAM(any_guard, false, vArray(vInt(13))) o
          PARAM(any_guard, false, vArray(vInt(15), vInt(7), vInt(27)))));
  ASSERT_EQ(6, get_signature_tag_count(s2));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s2, 0));
  ASSERT_VAREQ(vInt(9), get_signature_tag_at(s2, 1));
  ASSERT_VAREQ(vInt(11), get_signature_tag_at(s2, 2));
  ASSERT_VAREQ(vInt(13), get_signature_tag_at(s2, 3));
  ASSERT_VAREQ(vInt(15), get_signature_tag_at(s2, 4));
  ASSERT_VAREQ(vInt(27), get_signature_tag_at(s2, 5));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

// Description of an argument used in testing.
typedef struct {
  variant_t *tag;
  variant_t *value;
  bool is_valid;
} test_argument_t;

// Packs its arguments into a test_argument_t struct.
#define ARG(tag, value) ((test_argument_t) {tag, value, true})

// Collects a set of arguments into an array.
#define ARGS(N, args) ((test_argument_t[(N) + 1]) {args, ((test_argument_t) {vInt(0), vInt(0), false})})

// Collects a list of positive ints into an array.
#define INTS(N, args) ((int64_t[(N) + 1]) {args, -1})

// Attempts to do a match and checks that the outcome is as expected. If the
// expected offsets are NULL offsets won't be checked.
void assert_match_with_offsets(runtime_t *runtime, match_result_t expected_result,
    int64_t *expected_offsets, value_t signature, test_argument_t *args) {
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
    set_array_at(tags, i, C(arg.tag));
    frame_push_value(&frame, C(arg.value));
  }
  value_t vector = build_invocation_record_vector(runtime, tags);
  value_t record = new_heap_invocation_record(runtime, afFreeze, vector);
  static const size_t kLength = 16;
  score_t scores[16];
  size_t offsets[16];
  // Reset the offsets to a recognizable value (the size_t max).
  memset(offsets, 0xFF, kLength * sizeof(size_t));
  match_info_t match_info;
  match_info_init(&match_info, scores, offsets, kLength);
  match_result_t result;
  ASSERT_SUCCESS(match_signature(runtime, signature, record, &frame,
      new_integer(0), &match_info, &result));
  ASSERT_EQ(expected_result, result);
  if (expected_offsets != NULL) {
    for (size_t i = 0; i < arg_count; i++)
      ASSERT_EQ(expected_offsets[i], offsets[i]);
  }
}

// Attempts to do a match and checks that the result is as expected, ignoring
// the offsets and scores.
void assert_match(runtime_t *runtime, match_result_t expected, value_t signature,
    test_argument_t *args) {
  assert_match_with_offsets(runtime, expected, NULL, signature, args);
}

TEST(method, simple_matching) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t sig = make_signature(runtime,
      false,
      PARAMS(2,
          PARAM(any_guard, false, vArray(vInt(0))) o
          PARAM(any_guard, false, vArray(vInt(1)))));

  test_argument_t empty[1] = {{vInt(0), vInt(0), false}};
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar"))));
  assert_match(runtime, mrUnexpectedArgument, sig, ARGS(3,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar")) o
      ARG(vInt(2), vStr("baz"))));
  assert_match(runtime, mrMissingArgument, sig, ARGS(1,
      ARG(vInt(0), vStr("foo"))));
  assert_match(runtime, mrMissingArgument, sig, ARGS(1,
      ARG(vInt(1), vStr("bar"))));
  assert_match(runtime, mrUnexpectedArgument, sig, ARGS(1,
      ARG(vInt(2), vStr("baz"))));
  assert_match(runtime, mrMissingArgument, sig, empty);

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

TEST(method, simple_guard_matching) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t foo = C(vStr("foo"));
  value_t guard = new_heap_guard(runtime, afFreeze, gtEq, foo);
  value_t sig = make_signature(runtime,
      false,
      PARAMS(2,
          PARAM(guard, false, vArray(vInt(0))) o
          PARAM(any_guard, false, vArray(vInt(1)))));

  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("boo"))));
  assert_match(runtime, mrGuardRejected, sig, ARGS(2,
      ARG(vInt(0), vStr("fop")) o
      ARG(vInt(1), vStr("boo"))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

TEST(method, multi_tag_matching) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t sig = make_signature(runtime,
      false,
      PARAMS(2,
          PARAM(any_guard, false, vArray(vInt(0) o vStr("x"))) o
          PARAM(any_guard, false, vArray(vInt(1) o vStr("y")))));

  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vStr("y"), vStr("bar"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(1), vStr("bar")) o
      ARG(vStr("x"), vStr("foo"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vStr("x"), vStr("foo")) o
      ARG(vStr("y"), vStr("bar"))));
  assert_match(runtime, mrRedundantArgument, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vStr("x"), vStr("foo"))));
  assert_match(runtime, mrRedundantArgument, sig, ARGS(2,
      ARG(vInt(1), vStr("bar")) o
      ARG(vStr("y"), vStr("bar"))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

TEST(method, extra_args) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t sig = make_signature(runtime,
      true,
      PARAMS(2,
          PARAM(any_guard, false, vArray(vInt(0), vStr("x"))) o
          PARAM(any_guard, false, vArray(vInt(1), vStr("y")))));

  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vStr("y"), vStr("bar"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vInt(1), vStr("bar")) o
      ARG(vStr("x"), vStr("foo"))));
  assert_match(runtime, mrMatch, sig, ARGS(2,
      ARG(vStr("x"), vStr("foo")) o
      ARG(vStr("y"), vStr("bar"))));
  assert_match(runtime, mrRedundantArgument, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vStr("x"), vStr("foo"))));
  assert_match(runtime, mrRedundantArgument, sig, ARGS(2,
      ARG(vInt(1), vStr("bar")) o
      ARG(vStr("y"), vStr("bar"))));
  assert_match(runtime, mrExtraMatch, sig, ARGS(3,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar")) o
      ARG(vInt(2), vStr("baz"))));
  assert_match(runtime, mrExtraMatch, sig, ARGS(4,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar")) o
      ARG(vInt(2), vStr("baz")) o
      ARG(vInt(3), vStr("quux"))));
  assert_match(runtime, mrMissingArgument, sig, ARGS(2,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(2), vStr("baz"))));
  assert_match(runtime, mrMissingArgument, sig, ARGS(2,
      ARG(vInt(1), vStr("foo")) o
      ARG(vInt(2), vStr("baz"))));
  assert_match(runtime, mrExtraMatch, sig, ARGS(3,
      ARG(vInt(0), vStr("foo")) o
      ARG(vInt(1), vStr("bar")) o
      ARG(vStr("z"), vStr("baz"))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

TEST(method, match_argument_map) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  value_t any_guard = ROOT(runtime, any_guard);
  value_t sig = make_signature(runtime,
      true,
      PARAMS(4,
          PARAM(any_guard, false, vArray(vInt(0), vStr("w"))) o
          PARAM(any_guard, false, vArray(vInt(1), vStr("z"))) o
          PARAM(any_guard, false, vArray(vInt(2), vStr("y"))) o
          PARAM(any_guard, false, vArray(vInt(3), vStr("x")))));

  // Evaluation order. We'll cycle through all permutations of this, starting
  // with the "obvious" order.
  int64_t es[4];
  for (size_t i = 0; i < 4; i++)
    es[i] = i;

  // String tags, to try those as well. THis both tests having multiple tags
  // for each param and having the tags out of sort order.
  variant_t *ss[4] = {vStr("w"), vStr("z"), vStr("y"), vStr("x")};

  do {
    // Argument map which is the inverse of the evaluation order (since we're
    // going from parameters to stack offset) as well as reversed (since the
    // stack is accessed from the top, the top element at offset 0 having been
    // evaluated last).
    int64_t as[4];
    as[es[3]] = 0;
    as[es[2]] = 1;
    as[es[1]] = 2;
    as[es[0]] = 3;
    // Integer tags.
    assert_match_with_offsets(runtime, mrMatch,
        INTS(4, as[0] o as[1] o as[2] o as[3]), sig,
        ARGS(4,
            ARG(vInt(es[0]), vInt(96)) o
            ARG(vInt(es[1]), vInt(97)) o
            ARG(vInt(es[2]), vInt(98)) o
            ARG(vInt(es[3]), vInt(99))));
    // String tags.
    assert_match_with_offsets(runtime, mrMatch,
        INTS(4, as[0] o as[1] o as[2] o as[3]), sig,
        ARGS(4,
            ARG(ss[es[0]], vInt(104)) o
            ARG(ss[es[1]], vInt(103)) o
            ARG(ss[es[2]], vInt(102)) o
            ARG(ss[es[3]], vInt(101))));
  } while (advance_lexical_permutation(es, 4));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

// Description of a score used in testing.
typedef struct {
  score_t value;
  bool is_valid;
} test_score_t;

// Expands to a valid test score struct with the given value.
#define SCORE(V) ((test_score_t) {(V), true})

// Expands to a list of scores with the given values, terminated by a score with
// the is_valid flag false.
#define SCORES(N, V) ((test_score_t[(N + 1)]) {V, {0, false}})

// Test that joining the given target and source yield the expected result and
// scores stored in the target array.
static void test_join(join_status_t status, test_score_t *expected,
    test_score_t *test_target, test_score_t *test_source) {
  score_t source[16];
  score_t source_before[16];
  score_t target[16];
  size_t length = 0;
  for (size_t i = 0; expected[i].is_valid; i++) {
    source[i] = source_before[i] = test_source[i].value;
    target[i] = test_target[i].value;
    length++;
  }
  join_status_t found = join_score_vectors(target, source, length);
  ASSERT_EQ(status, found);
  for (size_t i = 0; i < length; i++) {
    ASSERT_EQ(expected[i].value, target[i]);
    ASSERT_EQ(source_before[i], source[i]);
  }
}

TEST(method, join) {
  test_score_t empty[1] = {{0, false}};
  test_join(jsEqual,
      empty,
      empty,
      empty);
  test_join(jsEqual,
      SCORES(1, SCORE(1)),
      SCORES(1, SCORE(1)),
      SCORES(1, SCORE(1)));
  test_join(jsAmbiguous,
      SCORES(2, SCORE(0) o SCORE(0)),
      SCORES(2, SCORE(0) o SCORE(1)),
      SCORES(2, SCORE(1) o SCORE(0)));
  test_join(jsBetter,
      SCORES(2, SCORE(1) o SCORE(2)),
      SCORES(2, SCORE(2) o SCORE(3)),
      SCORES(2, SCORE(1) o SCORE(2)));
  test_join(jsBetter,
      SCORES(2, SCORE(0) o SCORE(0)),
      SCORES(2, SCORE(5) o SCORE(5)),
      SCORES(2, SCORE(0) o SCORE(0)));
  test_join(jsWorse,
      SCORES(2, SCORE(1) o SCORE(2)),
      SCORES(2, SCORE(1) o SCORE(2)),
      SCORES(2, SCORE(2) o SCORE(3)));
  test_join(jsWorse,
      SCORES(2, SCORE(0) o SCORE(0)),
      SCORES(2, SCORE(0) o SCORE(0)),
      SCORES(2, SCORE(5) o SCORE(5)));
}

static void test_lookup(runtime_t *runtime, value_t expected, value_t first,
    value_t second, value_t third, value_t space) {
  value_t stack = new_heap_stack(runtime, 16);
  value_t vector = new_heap_pair_array(runtime, 3);
  frame_t frame;
  push_stack_frame(runtime, stack, &frame, 3);
  value_t values[3] = {first, second, third};
  for (size_t i = 0; i < 3; i++) {
    set_pair_array_first_at(vector, i, new_integer(i));
    set_pair_array_second_at(vector, i, new_integer(2 - i));
    frame_push_value(&frame, values[i]);
  }
  value_t record = new_heap_invocation_record(runtime, afFreeze, vector);
  value_t arg_map;
  value_t method = lookup_methodspace_method(runtime, space, record, &frame,
      &arg_map);
  ASSERT_VALEQ(expected, method);
}

TEST(method, dense_perfect_lookup) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  // Protocols and inheritance hierarchy.
  value_t a_p = new_heap_type(runtime, afFreeze, C(vStr("A")));
  value_t b_p = new_heap_type(runtime, afFreeze, C(vStr("B")));
  value_t c_p = new_heap_type(runtime, afFreeze, C(vStr("C")));
  value_t d_p = new_heap_type(runtime, afFreeze, C(vStr("D")));
  value_t space = new_heap_methodspace(runtime);
  // D <: C <: B <: A <: Object
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, d_p, c_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, c_p, b_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, b_p, a_p));

  // Guards.
  value_t a_g = new_heap_guard(runtime, afFreeze, gtIs, a_p);
  value_t b_g = new_heap_guard(runtime, afFreeze, gtIs, b_p);
  value_t c_g = new_heap_guard(runtime, afFreeze, gtIs, c_p);
  value_t d_g = new_heap_guard(runtime, afFreeze, gtIs, d_p);
  value_t guards[4] = {a_g, b_g, c_g, d_g};

  // Instances
  value_t a = new_instance_of(runtime, a_p);
  value_t b = new_instance_of(runtime, b_p);
  value_t c = new_instance_of(runtime, c_p);
  value_t d = new_instance_of(runtime, d_p);
  value_t values[4] = {a, b, c, d};

  value_t dummy_code = new_heap_code_block(runtime,
      new_heap_blob(runtime, 0),
      ROOT(runtime, empty_array),
      0);
  // Build a method for each combination of parameter types.
  value_t methods[4][4][4];
  for (size_t first = 0; first < 4; first++) {
    for (size_t second = 0; second < 4; second++) {
      for (size_t third = 0; third < 4; third++) {
        value_t signature = make_signature(runtime, false, PARAMS(3,
            PARAM(guards[first], false, vArray(vInt(0))) o
            PARAM(guards[second], false, vArray(vInt(1))) o
            PARAM(guards[third], false, vArray(vInt(2)))));
        value_t method = new_heap_method(runtime, afFreeze, signature,
            nothing(), dummy_code, nothing());
        add_methodspace_method(runtime, space, method);
        methods[first][second][third] = method;
      }
    }
  }

  // Try a lookup for each type of argument.
  for (size_t first = 0; first < 4; first++) {
    for (size_t second = 0; second < 4; second++) {
      for (size_t third = 0; third < 4; third++) {
        value_t expected = methods[first][second][third];
        test_lookup(runtime, expected, values[first], values[second],
            values[third], space);
      }
    }
  }

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

// Shorthand for testing how an operation prints.
#define CHECK_OP_PRINT(EXPECTED, OP) do {                                      \
  value_t op = (OP);                                                           \
  value_to_string_t to_string;                                                 \
  ASSERT_C_STREQ(EXPECTED, value_to_string(&to_string, op));                   \
  dispose_value_to_string(&to_string);                                         \
} while (false)

// Shorthand for creating an op with the given type and value.
#define OP(otType, vValue)                                                     \
  new_heap_operation(runtime, afFreeze, otType, C(vValue))

TEST(method, operation_printing) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  CHECK_OP_PRINT("()", OP(otCall, vNull()));
  CHECK_OP_PRINT("[]", OP(otIndex, vNull()));
  CHECK_OP_PRINT(".+()", OP(otInfix, vStr("+")));
  CHECK_OP_PRINT(".foo()", OP(otInfix, vStr("foo")));
  CHECK_OP_PRINT("!()", OP(otPrefix, vStr("!")));
  CHECK_OP_PRINT("blah()", OP(otPrefix, vStr("blah")));
  CHECK_OP_PRINT(".+", OP(otProperty, vStr("+")));
  CHECK_OP_PRINT(".foo", OP(otProperty, vStr("foo")));
  CHECK_OP_PRINT("()!", OP(otSuffix, vStr("!")));
  CHECK_OP_PRINT("()blah", OP(otSuffix, vStr("blah")));

  CHECK_OP_PRINT("():=", OP(otAssign, vValue(OP(otCall, vNull()))));
  CHECK_OP_PRINT("[]:=", OP(otAssign, vValue(OP(otIndex, vNull()))));
  CHECK_OP_PRINT(".foo():=", OP(otAssign, vValue(OP(otInfix, vStr("foo")))));
  CHECK_OP_PRINT("!():=", OP(otAssign, vValue(OP(otPrefix, vStr("!")))));
  CHECK_OP_PRINT(".foo:=", OP(otAssign, vValue(OP(otProperty, vStr("foo")))));
  CHECK_OP_PRINT("()!:=", OP(otAssign, vValue(OP(otSuffix, vStr("!")))));

  // Okay this is just ridiculous.
  CHECK_OP_PRINT(".foo:=:=", OP(otAssign, vValue(OP(otAssign,
      vValue(OP(otProperty, vStr("foo")))))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}
