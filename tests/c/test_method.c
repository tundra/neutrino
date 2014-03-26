//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "ook.h"
#include "runtime.h"
#include "tagged-inl.h"
#include "test.h"
#include "try-inl.h"

// Checks that scoring value against guard gives a match iff is_match is true.
#define ASSERT_MATCH(is_match, guard, value) do {                              \
  value_t match;                                                               \
  frame_sigmap_input_o lookup_input = frame_sigmap_input_new(ambience, nothing(), NULL); \
  sigmap_input_o *input = UPCAST(UPCAST(&lookup_input));                       \
  ASSERT_SUCCESS(guard_match(guard, value, input, space, &match));             \
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
  value_t p1 = new_heap_type(runtime, afFreeze, nothing(), null());
  value_t p2 = new_heap_type(runtime, afFreeze, nothing(), null());
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, p1, p2));
  value_t p3 = new_heap_type(runtime, afFreeze, nothing(), null());
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, p2, p3));
  value_t p4 = new_heap_type(runtime, afFreeze, nothing(), null());
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

  value_t s_str_p = new_heap_type(runtime, afFreeze, nothing(), null());
  value_t obj_p = new_heap_type(runtime, afFreeze, nothing(), null());
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

  string_t x_str = new_string("x");
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
  value_t score_a;                                                             \
  frame_sigmap_input_o lookup_input = frame_sigmap_input_new(ambience, nothing(), NULL); \
  sigmap_input_o *input = UPCAST(UPCAST(&lookup_input));                       \
  ASSERT_SUCCESS(guard_match(GA, VA, input, space, &score_a));                 \
  value_t score_b;                                                             \
  ASSERT_SUCCESS(guard_match(GB, VB, input, space, &score_b));                 \
  ASSERT_TRUE(compare_tagged_scores(score_a, score_b) REL 0);                  \
} while (false)

TEST(method, is_score) {
  CREATE_RUNTIME();

  value_t s_str_p = new_heap_type(runtime, afFreeze, nothing(), null());
  value_t obj_p = new_heap_type(runtime, afFreeze, nothing(), null());
  value_t str_p = ROOT(runtime, string_type);
  value_t space = new_heap_methodspace(runtime);
  // s-str <: str <: obj
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, str_p, obj_p));
  ASSERT_SUCCESS(add_methodspace_inheritance(runtime, space, s_str_p, str_p));

  string_t x_str = new_string("x");
  value_t x = new_heap_string(runtime, &x_str);
  value_t s_str = new_instance_of(runtime, s_str_p);

  value_t is_x = new_heap_guard(runtime, afFreeze, gtEq, x);
  value_t is_obj = new_heap_guard(runtime, afFreeze, gtIs, obj_p);
  value_t is_str = new_heap_guard(runtime, afFreeze, gtIs, str_p);
  value_t is_s_str = new_heap_guard(runtime, afFreeze, gtIs, s_str_p);

  ASSERT_COMPARE(is_str, x, >, is_obj, x);
  ASSERT_COMPARE(is_x, x, >, is_str, x);
  ASSERT_COMPARE(is_str, s_str, >, is_obj, s_str);
  ASSERT_COMPARE(is_s_str, s_str, >, is_str, s_str);

  DISPOSE_RUNTIME();
}

TEST(method, multi_score) {
  CREATE_RUNTIME();

  value_t int_str_p = new_heap_type(runtime, afFreeze, nothing(), null());
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

TEST(method, call_tags) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

#define kCount 8
  variant_t *raw_array = vArray(vInt(7), vInt(6), vInt(5), vInt(4), vInt(3),
      vInt(2), vInt(1), vInt(0));
  value_t entries = build_call_tags_entries(runtime, C(raw_array));
  value_t tags = new_heap_call_tags(runtime, afFreeze, entries);
  ASSERT_EQ(kCount, get_call_tags_entry_count(tags));
  for (size_t i = 0; i < kCount; i++) {
    ASSERT_VALEQ(new_integer(i), get_call_tags_tag_at(tags, i));
    ASSERT_EQ(i, get_call_tags_offset_at(tags, i));
  }
#undef kCount

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

// Makes a call tags object for the given array of tags, passed as a variant
// for convenience.
static value_t make_call_tags(runtime_t *runtime, variant_t *variant) {
  TRY_DEF(entries, build_call_tags_entries(runtime, C(variant)));
  return new_heap_call_tags(runtime, afFreeze, entries);
}

TEST(method, make_call_tags) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t tags = make_call_tags(runtime, vArray(vStr("z"), vStr("x"),
      vStr("y")));
  ASSERT_VAREQ(vStr("x"), get_call_tags_tag_at(tags, 0));
  ASSERT_VAREQ(vStr("y"), get_call_tags_tag_at(tags, 1));
  ASSERT_VAREQ(vStr("z"), get_call_tags_tag_at(tags, 2));
  ASSERT_EQ(1, get_call_tags_offset_at(tags, 0));
  ASSERT_EQ(0, get_call_tags_offset_at(tags, 1));
  ASSERT_EQ(2, get_call_tags_offset_at(tags, 2));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, call_tags_with_stack) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t stack = new_heap_stack(runtime, 24);
  frame_t frame = open_stack(stack);
  ASSERT_SUCCESS(push_stack_frame(runtime, stack, &frame, 3, null()));
  value_t tags = make_call_tags(runtime, vArray(vStr("b"), vStr("c"),
      vStr("a")));
  frame_push_value(&frame, new_integer(7));
  frame_push_value(&frame, new_integer(8));
  frame_push_value(&frame, new_integer(9));
  ASSERT_VAREQ(vInt(9), frame_get_pending_argument_at(&frame, tags, 0));
  ASSERT_VAREQ(vInt(7), frame_get_pending_argument_at(&frame, tags, 1));
  ASSERT_VAREQ(vInt(8), frame_get_pending_argument_at(&frame, tags, 2));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, make_signature) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  value_t s0 = C(vSignature(
      false,
      vParameter(
          any_guard,
          false,
          vInt(0)),
      vParameter(
          any_guard,
          false,
          vInt(1))));

  ASSERT_EQ(2, get_signature_tag_count(s0));
  ASSERT_VAREQ(vInt(0), get_signature_tag_at(s0, 0));
  ASSERT_VAREQ(vInt(1), get_signature_tag_at(s0, 1));

  value_t s1 = C(vSignature(
      false,
      vParameter(
          any_guard,
          false,
          vInt(6)),
      vParameter(
          any_guard,
          false,
          vInt(3)),
      vParameter(
          any_guard,
          false,
          vInt(7))));
  ASSERT_EQ(3, get_signature_tag_count(s1));
  ASSERT_VAREQ(vInt(3), get_signature_tag_at(s1, 0));
  ASSERT_VAREQ(vInt(6), get_signature_tag_at(s1, 1));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s1, 2));

  value_t s2 = C(vSignature(
      false,
      vParameter(
          any_guard,
          false,
          vInt(9),
          vInt(11)),
      vParameter(
          any_guard,
          false,
          vInt(13)),
      vParameter(
          any_guard,
          false,
          vInt(15),
          vInt(7),
          vInt(27))));
  ASSERT_EQ(6, get_signature_tag_count(s2));
  ASSERT_VAREQ(vInt(7), get_signature_tag_at(s2, 0));
  ASSERT_VAREQ(vInt(9), get_signature_tag_at(s2, 1));
  ASSERT_VAREQ(vInt(11), get_signature_tag_at(s2, 2));
  ASSERT_VAREQ(vInt(13), get_signature_tag_at(s2, 3));
  ASSERT_VAREQ(vInt(15), get_signature_tag_at(s2, 4));
  ASSERT_VAREQ(vInt(27), get_signature_tag_at(s2, 5));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

#define vPair(F, S) vVariant(expand_variant_to_array, vArrayPayload(F, S))

#define vCallData(...) vVariant(expand_variant_to_call_data, vArrayPayload(__VA_ARGS__))

// Collects a set of arguments into an array.
#define ARGS(N, ...)                                                           \
  ARENA_ARRAY(&__test_arena__, test_argument_t*, (N) + 1, __VA_ARGS__, NULL)

// Attempts to do a match and checks that the outcome is as expected. If the
// expected offsets are NULL offsets won't be checked.
void assert_match_with_offsets(value_t ambience, match_result_t expected_result,
    int64_t *expected_offsets, value_t signature, variant_t *args_var) {
  runtime_t *runtime = get_ambience_runtime(ambience);
  value_t args = C(args_var);
  size_t arg_count = get_array_length(args);
  // Build a descriptor from the tags and a stack from the values.
  value_t tags = new_heap_array(runtime, arg_count);
  value_t stack = new_heap_stack(runtime, 24);
  frame_t frame = open_stack(stack);
  push_stack_frame(runtime, stack, &frame, arg_count, null());
  for (size_t i = 0; i < arg_count; i++) {
    value_t arg = get_array_at(args, i);
    set_array_at(tags, i, get_array_at(arg, 0));
    frame_push_value(&frame, get_array_at(arg, 1));
  }
  value_t entries = build_call_tags_entries(runtime, tags);
  value_t call_tags = new_heap_call_tags(runtime, afFreeze, entries);
  static const size_t kLength = 16;
  value_t scores[16];
  size_t offsets[16];
  // Reset the offsets to a recognizable value (the size_t max).
  memset(offsets, 0xFF, kLength * sizeof(size_t));
  match_info_t match_info;
  match_info_init(&match_info, scores, offsets, kLength);
  match_result_t result = __mrNone__;
  frame_sigmap_input_o input = frame_sigmap_input_new(ambience, call_tags, &frame);
  ASSERT_SUCCESS(match_signature(signature, UPCAST(UPCAST(&input)), nothing(),
      &match_info, &result));
  ASSERT_EQ(expected_result, result);
  if (expected_offsets != NULL) {
    for (size_t i = 0; i < arg_count; i++)
      ASSERT_EQ(expected_offsets[i], offsets[i]);
  }
  if (expected_result == mrGuardRejected)
    // Only test tag matching in the cases where the result doesn't depend on
    // how the guards match.
    return;
  result = __mrNone__;
  ASSERT_SUCCESS(match_signature_tags(signature, call_tags, &result));
  ASSERT_EQ(expected_result, result);
}

// Attempts to do a match and checks that the result is as expected, ignoring
// the offsets and scores.
void assert_match(value_t ambience, match_result_t expected, value_t signature,
    variant_t *args) {
  assert_match_with_offsets(ambience, expected, NULL, signature, args);
}

TEST(method, simple_matching) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  value_t sig = C(vSignature(
      false,
      vParameter(
          any_guard,
          false,
          vInt(0)),
      vParameter(
          any_guard,
          false,
          vInt(1))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar"))));
  assert_match(ambience, mrUnexpectedArgument, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar")),
      vPair(vInt(2), vStr("baz"))));
  assert_match(ambience, mrMissingArgument, sig, vArray(
      vPair(vInt(0), vStr("foo"))));
  assert_match(ambience, mrMissingArgument, sig, vArray(
      vPair(vInt(1), vStr("bar"))));
  assert_match(ambience, mrMissingArgument, sig, vArray(
      vPair(vInt(2), vStr("baz"))));
  assert_match(ambience, mrMissingArgument, sig, vEmptyArray());

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, simple_guard_matching) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  variant_t *guard = vGuard(gtEq, vStr("foo"));
  value_t sig = C(vSignature(
      false,
      vParameter(
          guard,
          false,
          vInt(0)),
      vParameter(
          any_guard,
          false,
          vInt(1))));

  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("boo"))));
  assert_match(ambience, mrGuardRejected, sig, vArray(
      vPair(vInt(0), vStr("fop")),
      vPair(vInt(1), vStr("boo"))));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, multi_tag_matching) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  value_t sig = C(vSignature(
      false,
      vParameter(
          any_guard,
          false,
          vInt(0),
          vStr("x")),
      vParameter(
          any_guard,
          false,
          vInt(1),
          vStr("y"))));

  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vStr("y"), vStr("bar"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(1), vStr("bar")),
      vPair(vStr("x"), vStr("foo"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vStr("x"), vStr("foo")),
      vPair(vStr("y"), vStr("bar"))));
  assert_match(ambience, mrRedundantArgument, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vStr("x"), vStr("foo"))));
  assert_match(ambience, mrRedundantArgument, sig, vArray(
      vPair(vInt(1), vStr("bar")),
      vPair(vStr("y"), vStr("bar"))));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, extra_args) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  value_t sig = C(vSignature(
      true,
      vParameter(
          any_guard,
          false,
          vInt(0),
          vStr("x")),
      vParameter(
          any_guard,
          false,
          vInt(1),
          vStr("y"))));

  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vStr("y"), vStr("bar"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vInt(1), vStr("bar")),
      vPair(vStr("x"), vStr("foo"))));
  assert_match(ambience, mrMatch, sig, vArray(
      vPair(vStr("x"), vStr("foo")),
      vPair(vStr("y"), vStr("bar"))));
  assert_match(ambience, mrRedundantArgument, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vStr("x"), vStr("foo"))));
  assert_match(ambience, mrRedundantArgument, sig, vArray(
      vPair(vInt(1), vStr("bar")),
      vPair(vStr("y"), vStr("bar"))));
  assert_match(ambience, mrExtraMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar")),
      vPair(vInt(2), vStr("baz"))));
  assert_match(ambience, mrExtraMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar")),
      vPair(vInt(2), vStr("baz")),
      vPair(vInt(3), vStr("quux"))));
  assert_match(ambience, mrMissingArgument, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(2), vStr("baz"))));
  assert_match(ambience, mrMissingArgument, sig, vArray(
      vPair(vInt(1), vStr("foo")),
      vPair(vInt(2), vStr("baz"))));
  assert_match(ambience, mrExtraMatch, sig, vArray(
      vPair(vInt(0), vStr("foo")),
      vPair(vInt(1), vStr("bar")),
      vPair(vStr("z"), vStr("baz"))));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, match_argument_map) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  variant_t *any_guard = vGuard(gtAny, vNull());
  value_t sig = C(vSignature(
      true,
      vParameter(
          any_guard,
          false,
          vInt(0),
          vStr("w")),
      vParameter(
          any_guard,
          false,
          vInt(1),
          vStr("z")),
      vParameter(
          any_guard,
          false,
          vInt(2),
          vStr("y")),
      vParameter(
          any_guard,
          false,
          vInt(3),
          vStr("x"))));

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
    assert_match_with_offsets(ambience, mrMatch, as, sig,
        vArray(
            vPair(vInt(es[0]), vInt(96)),
            vPair(vInt(es[1]), vInt(97)),
            vPair(vInt(es[2]), vInt(98)),
            vPair(vInt(es[3]), vInt(99))));
    // String tags.
    assert_match_with_offsets(ambience, mrMatch, as, sig,
        vArray(
            vPair(ss[es[0]], vInt(104)),
            vPair(ss[es[1]], vInt(103)),
            vPair(ss[es[2]], vInt(102)),
            vPair(ss[es[3]], vInt(101))));
  } while (advance_lexical_permutation(es, 4));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

// Allocates a new score pointer.
static value_t *new_score_ptr(test_arena_t *arena, uint32_t value) {
  value_t *result = ARENA_MALLOC(arena, value_t);
  *result = new_score(scEq, value);
  return result;
}

// Expands to a valid test score pointer with the given value.
#define SCORE(V) new_score_ptr(&__test_arena__, V)

// Expands to a list of scores with the given values, terminated by NULL.
#define SCORES(N, ...)                                                         \
  ARENA_ARRAY(&__test_arena__, value_t*, (N + 1), __VA_ARGS__, NULL)

// Test that joining the given target and source yield the expected result and
// scores stored in the target array.
static void test_join(join_status_t status, value_t **expected,
    value_t **test_target, value_t **test_source) {
  value_t source[16];
  value_t source_before[16];
  value_t target[16];
  size_t length = 0;
  for (size_t i = 0; expected[i] != NULL; i++) {
    source[i] = source_before[i] = *test_source[i];
    target[i] = *test_target[i];
    length++;
  }
  join_status_t found = join_score_vectors(target, source, length);
  ASSERT_EQ(status, found);
  for (size_t i = 0; i < length; i++) {
    ASSERT_SAME(*expected[i], target[i]);
    ASSERT_SAME(source_before[i], source[i]);
  }
}

TEST(method, join) {
  CREATE_TEST_ARENA();

  value_t *empty = NULL;
  test_join(jsEqual,
      &empty,
      &empty,
      &empty);
  test_join(jsEqual,
      SCORES(1, SCORE(1)),
      SCORES(1, SCORE(1)),
      SCORES(1, SCORE(1)));
  test_join(jsAmbiguous,
      SCORES(2, SCORE(0), SCORE(0)),
      SCORES(2, SCORE(0), SCORE(1)),
      SCORES(2, SCORE(1), SCORE(0)));
  test_join(jsBetter,
      SCORES(2, SCORE(1), SCORE(2)),
      SCORES(2, SCORE(2), SCORE(3)),
      SCORES(2, SCORE(1), SCORE(2)));
  test_join(jsBetter,
      SCORES(2, SCORE(0), SCORE(0)),
      SCORES(2, SCORE(5), SCORE(5)),
      SCORES(2, SCORE(0), SCORE(0)));
  test_join(jsWorse,
      SCORES(2, SCORE(1), SCORE(2)),
      SCORES(2, SCORE(1), SCORE(2)),
      SCORES(2, SCORE(2), SCORE(3)));
  test_join(jsWorse,
      SCORES(2, SCORE(0), SCORE(0)),
      SCORES(2, SCORE(0), SCORE(0)),
      SCORES(2, SCORE(5), SCORE(5)));

  DISPOSE_TEST_ARENA();
}

static void test_lookup(value_t ambience, value_t expected, value_t first,
    value_t second, value_t third, value_t space) {
  runtime_t *runtime = get_ambience_runtime(ambience);
  value_t stack = new_heap_stack(runtime, 24);
  value_t entries = new_heap_pair_array(runtime, 3);
  frame_t frame = open_stack(stack);
  push_stack_frame(runtime, stack, &frame, 3, null());
  value_t values[3] = {first, second, third};
  for (size_t i = 0; i < 3; i++) {
    set_pair_array_first_at(entries, i, new_integer(i));
    set_pair_array_second_at(entries, i, new_integer(2 - i));
    frame_push_value(&frame, values[i]);
  }
  value_t tags = new_heap_call_tags(runtime, afFreeze, entries);
  value_t arg_map;
  frame_sigmap_input_o input = frame_sigmap_input_new(ambience, tags, &frame);
  value_t method = lookup_methodspace_method(UPCAST(UPCAST(&input)), space,
      &arg_map);
  ASSERT_VALEQ(expected, method);
}

TEST(method, dense_perfect_lookup) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  // Protocols and inheritance hierarchy.
  value_t a_p = new_heap_type(runtime, afFreeze, nothing(), C(vStr("A")));
  value_t b_p = new_heap_type(runtime, afFreeze, nothing(), C(vStr("B")));
  value_t c_p = new_heap_type(runtime, afFreeze, nothing(), C(vStr("C")));
  value_t d_p = new_heap_type(runtime, afFreeze, nothing(), C(vStr("D")));
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
        value_t signature = C(vSignature(
            false,
            vParameter(
                vValue(guards[first]),
                false,
                vInt(0)),
            vParameter(
                vValue(guards[second]),
                false,
                vInt(1)),
            vParameter(
                vValue(guards[third]),
                false,
                vInt(2))));
        value_t method = new_heap_method(runtime, afFreeze, signature,
            nothing(), dummy_code, nothing(), new_flag_set(kFlagSetAllOff));
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
        test_lookup(ambience, expected, values[first], values[second],
            values[third], space);
      }
    }
  }

  DISPOSE_TEST_ARENA();
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
  CREATE_TEST_ARENA();

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

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}

TEST(method, tag_sorting) {
  CREATE_RUNTIME();

  value_t array = new_heap_array(runtime, 8);
  set_array_at(array, 0, new_integer(1));
  set_array_at(array, 1, new_integer(0));
  set_array_at(array, 2, null());
  set_array_at(array, 3, RSTR(runtime, value));
  set_array_at(array, 4, RSTR(runtime, key));
  set_array_at(array, 5, ROOT(runtime, empty_array));
  set_array_at(array, 6, ROOT(runtime, selector_key));
  set_array_at(array, 7, ROOT(runtime, subject_key));
  sort_array(array);
  ASSERT_SAME(ROOT(runtime, subject_key), get_array_at(array, 0));
  ASSERT_SAME(ROOT(runtime, selector_key), get_array_at(array, 1));
  ASSERT_SAME(ROOT(runtime, empty_array), get_array_at(array, 2));
  ASSERT_SAME(RSTR(runtime, key), get_array_at(array, 3));
  ASSERT_SAME(RSTR(runtime, value), get_array_at(array, 4));
  ASSERT_SAME(new_integer(0), get_array_at(array, 5));
  ASSERT_SAME(new_integer(1), get_array_at(array, 6));
  ASSERT_SAME(null(), get_array_at(array, 7));

  DISPOSE_RUNTIME();
}

TEST(method, call_tags_compare) {
  CREATE_RUNTIME();
  CREATE_TEST_ARENA();

  value_t r0 = make_call_tags(runtime, vArray(vStr("z"), vStr("x"),
      vStr("y")));
  value_t h0 = value_transient_identity_hash(r0);
  value_t r1 = make_call_tags(runtime, vArray(vStr("z"), vStr("x"),
      vStr("y")));
  value_t h1 = value_transient_identity_hash(r1);
  ASSERT_FALSE(is_same_value(r0, r1));
  ASSERT_TRUE(value_identity_compare(r0, r1));
  ASSERT_VALEQ(h0, h1);

  value_t r2 = make_call_tags(runtime, vArray(vStr("x"), vStr("z"),
      vStr("y")));
  value_t h2 = value_transient_identity_hash(r2);
  ASSERT_FALSE(value_identity_compare(r1, r2));
  ASSERT_FALSE(is_same_value(h1, h2));

  value_t r3 = make_call_tags(runtime, vArray(vStr("z"), vStr("x")));
  value_t h3 = value_transient_identity_hash(r3);
  ASSERT_FALSE(value_identity_compare(r1, r3));
  ASSERT_FALSE(value_identity_compare(r2, r3));
  ASSERT_FALSE(is_same_value(h1, h3));
  ASSERT_FALSE(is_same_value(h2, h3));

  value_t r4 = make_call_tags(runtime, vArray(vStr("x"), vStr("z"),
      vStr("y"), vStr("y")));
  value_t h4 = value_transient_identity_hash(r4);
  ASSERT_FALSE(value_identity_compare(r1, r4));
  ASSERT_FALSE(value_identity_compare(r2, r4));
  ASSERT_FALSE(value_identity_compare(r3, r4));
  ASSERT_FALSE(is_same_value(h1, h4));
  ASSERT_FALSE(is_same_value(h2, h4));
  ASSERT_FALSE(is_same_value(h3, h4));

  DISPOSE_TEST_ARENA();
  DISPOSE_RUNTIME();
}
