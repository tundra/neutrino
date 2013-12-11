// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "bind.h"
#include "test.h"

value_t expand_variant_to_unbound_module(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(fields, expand_variant_to_array(runtime, value));
  value_t path = get_array_at(fields, 0);
  value_t fragments = get_array_at(fields, 1);
  return new_heap_unbound_module(runtime, path, fragments);
}

// Creates a variant that expands to an unbound module with the given path and
// fragments.
#define vUnboundModule(path, ...) vVariant(                                    \
  expand_variant_to_unbound_module,                                            \
  vArrayPayload(path, vArray(__VA_ARGS__)))

value_t expand_variant_to_unbound_fragment(runtime_t *runtime, variant_value_t *value) {
  TRY_DEF(fields, expand_variant_to_array(runtime, value));
  value_t stage = get_array_at(fields, 0);
  value_t imports = get_array_at(fields, 1);
  return new_heap_unbound_module_fragment(runtime, stage, imports,
      ROOT(runtime, empty_array));
}

// Creates a variant that expands to an unbound fragment with the given stage
// and imports.
#define vUnboundFragment(stage, imports) vVariant(                             \
  expand_variant_to_unbound_fragment,                                          \
  vArrayPayload(stage, imports))

// Shorthands for general variants.
#define E vEmptyArray()
#define EB vEmptyArrayBuffer()
#define A(...) vArray(__VA_ARGS__)
#define B(...) vArrayBuffer(__VA_ARGS__)
#define I(S, N) vIdentifier(vStageOffset(S), vPath(vStr(#N)))

// Shorthands for creating dependency calculation input.
#define MOD(N, ...) vUnboundModule(vPath(vStr(#N)), __VA_ARGS__)
#define FRG(S, I) vUnboundFragment(vStageOffset(S), I)
#define IMP(N) vPath(vStr(#N))

// Macros for creating expected dependency calculation output.
#define DEP(I, E) I, E
#define MDEP(N, ...) vPath(vStr(#N)), __VA_ARGS__
#define FDEP(S, I) vStageOffset(S), I

// Given a map, returns a pair array of the map entries sorted by key.
static value_t sort_and_flatten_map(runtime_t *runtime, value_t map) {
  TRY_DEF(pairs, new_heap_pair_array(runtime, get_id_hash_map_size(map)));
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, map);
  for (size_t i = 0; id_hash_map_iter_advance(&iter); i++) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    set_pair_array_first_at(pairs, i, key);
    set_pair_array_second_at(pairs, i, value);
  }
  co_sort_pair_array(pairs);
  return pairs;
}

static void test_import_map(runtime_t *runtime, variant_t *expected,
    variant_t *modules) {
  binding_context_t context;
  binding_context_init(&context, runtime);
  value_t deps = build_fragment_entry_map(&context, C(modules));
  // Flatten the map so we can compare it deterministically.
  value_t flat_deps = sort_and_flatten_map(runtime, deps);
  for (size_t mi = 0; mi < get_pair_array_length(flat_deps); mi++) {
    value_t map = get_pair_array_second_at(flat_deps, mi);
    value_t flat = sort_and_flatten_map(runtime, map);
    set_pair_array_second_at(flat_deps, mi, flat);
    for (size_t fi = 0; fi < get_pair_array_length(flat); fi++) {
      value_t entry = get_pair_array_second_at(flat, fi);
      value_t imports = get_array_at(entry, 1);
      set_pair_array_second_at(flat, fi, imports);
    }
  }
  ASSERT_VAREQ(expected, flat_deps);
}

TEST(bind, dependency_map) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  // Two stages, 0 and -1, yield 0 -> -1.
  test_import_map(runtime,
      A(MDEP(root, A(
          FDEP(-1, EB),
          FDEP( 0, EB)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E))));
  // Three stages, 0, -1, and -2, yield 0 -> -1, -1 -> -2.
  test_import_map(runtime,
      A(MDEP(root, A(
          FDEP(-2, EB),
          FDEP(-1, EB),
          FDEP( 0, EB)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E),
          FRG(-2, E))));
  // Simple import dependencies.
  test_import_map(runtime,
      A(MDEP(root, A(
          FDEP(0, B(I(0, other))))),
        MDEP(other, A(
          FDEP(0, EB)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E))));
  // Not quite so simple import dependencies.
  test_import_map(runtime,
      A(MDEP(root, A(
          FDEP(-1, B(I(-1, other))),
          FDEP( 0, B(I( 0, other))))),
        MDEP(other, A(
          FDEP(-1, EB),
          FDEP( 0, EB)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));
  // Deep transitive import dependencies
  test_import_map(runtime,
      A(MDEP(x, A(
          FDEP(-1, B(I(-1, y))),
          FDEP( 0, B(I( 0, y))))),
        MDEP(y, A(
          FDEP(-1, B(I(-1, z))),
          FDEP( 0, B(I( 0, z))))),
        MDEP(z, A(
          FDEP(-1, EB),
          FDEP( 0, EB))),
        MDEP(root, A(
          FDEP(-1, B(I(-1, x))),
          FDEP( 0, B(I( 0, x)))))),
      B(MOD(root,
          FRG( 0, A(IMP(x)))),
        MOD(x,
          FRG( 0, A(IMP(y)))),
        MOD(y,
          FRG( 0, A(IMP(z)))),
        MOD(z,
          FRG( 0, E),
          FRG(-1, E))));
  // Stage shifting imports.
  test_import_map(runtime,
      A(MDEP(root, A(
          FDEP(-2, B(I(-1, other))),
          FDEP(-1, B(I( 0, other))))),
        MDEP(other, A(
          FDEP(-1, EB),
          FDEP( 0, EB)))),
      B(MOD(root,
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}

static void test_load_order(runtime_t *runtime, variant_t *expected,
    variant_t *modules) {
  binding_context_t context;
  binding_context_init(&context, runtime);
  build_fragment_entry_map(&context, C(modules));
  value_t schedule = build_binding_schedule(&context);
  ASSERT_VAREQ(expected, schedule);
}

TEST(bind, load_order) {
  CREATE_RUNTIME();
  CREATE_VARIANT_CONTAINER();

  // Stages within the same module.
  test_load_order(runtime,
      B(I(0, root)),
      B(MOD(root,
          FRG( 0, E))));
  test_load_order(runtime,
      B(I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-2, root), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG(-1, E),
          FRG( 0, E),
          FRG(-2, E))));

  // Present imports. For some of these there's more than one way to resolve
  // the dependencies and the solution will depend on the otherwise irrelevant
  // lexical ordering of the names of the modules.
  test_load_order(runtime,
      B(I(0, other), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(other)))),
        MOD(other,
          FRG(0, E))));
  test_load_order(runtime,
      B(I(-1, root), I(0, other), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(other))),
          FRG(-1, E)),
        MOD(other,
          FRG(0, E))));
  test_load_order(runtime,
      B(I(-1, other), I(-1, root), I(0, other), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(other))),
          FRG(-1, E)),
        MOD(other,
          FRG(0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-2, other), I(-2, root), I(-1, other), I(-1, root), I(0, other), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(other))),
          FRG(-1, E)),
        MOD(other,
          FRG(0, E),
          FRG(-1, E),
          FRG(-2, E))));
  test_load_order(runtime,
      B(I(-2, other), I(-2, root), I(-1, other), I(-1, root), I(0, other), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(other))),
          FRG(-1, E),
          FRG(-2, E)),
        MOD(other,
          FRG(0, E),
          FRG(-1, E),
          FRG(-2, E))));
  // Here the outcome depends particularly on the lexical ordering.
  test_load_order(runtime,
      B(I(-1, s), I(-1, t), I(-1, root), I(0, s), I(0, t), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(s), IMP(t))),
          FRG(-1, E)),
        MOD(s,
          FRG(0, E),
          FRG(-1, E)),
        MOD(t,
          FRG(0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-1, b), I(-1, a), I(-1, root), I(0, b), I(0, a), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(a))),
          FRG(-1, E)),
        MOD(a,
          FRG(0, A(IMP(b))),
          FRG(-1, E)),
        MOD(b,
          FRG(0, E),
          FRG(-1, E))));

  // Here we have $root <- $a <- $b and consequently @root <- @a <- @b, except
  // that there is no @a so what we'll actually get is @root <- @b.
  test_load_order(runtime,
      B(I(-1, b), I(-1, a), I(-1, root), I(0, b), I(0, a), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(a))),
          FRG(-1, E)),
        MOD(a,
          FRG(0, A(IMP(b)))),
        MOD(b,
          FRG(0, E),
          FRG(-1, E))));

  // Past imports
  test_load_order(runtime,
      B(I(0, other), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG(0, E))));
  test_load_order(runtime,
      B(I(-1, other), I(-2, root), I(0, other), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG(0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-1, b), I(-1, a), I(-2, root), I(0, b), I(0, a), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(a)))),
        MOD(a,
          FRG(0, A(IMP(b))),
          FRG(-1, E)),
        MOD(b,
          FRG( 0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-1, b), I(-2, a), I(-3, root), I(0, b), I(-1, a), I(-2, root),
          I(0, a), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(a)))),
        MOD(a,
          FRG(0, E),
          FRG(-1, A(IMP(b)))),
        MOD(b,
          FRG( 0, E),
          FRG(-1, E))));

  DISPOSE_VARIANT_CONTAINER();
  DISPOSE_RUNTIME();
}
