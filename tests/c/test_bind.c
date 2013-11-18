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
#define vUnboundModule(path, ...) vBuild(                                      \
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
#define vUnboundFragment(stage, imports) vBuild(                               \
  expand_variant_to_unbound_fragment,                                          \
  vArrayPayload(stage, imports))

// Shorthands for general variants.
#define E vEmptyArray()
#define A(...) vArray(__VA_ARGS__)
#define B(...) vArrayBuffer(__VA_ARGS__)
#define I(S, N) vIdentifier(vInt(S), vPath(vStr(#N)))

// Shorthands for creating dependency calculation input.
#define MOD(N, ...) vUnboundModule(vPath(vStr(#N)), __VA_ARGS__)
#define FRG(S, I) vUnboundFragment(vInt(S), I)
#define IMP(N) vPath(vStr(#N))

// Macros for creating expected dependency calculation output.
#define MDEP(P, ...) vPath(vStr(#P)), vArray(__VA_ARGS__)
#define FDEP(S, ...) vInt(S), vArrayBuffer(__VA_ARGS__)

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

static void test_dependency_map(runtime_t *runtime, variant_t expected,
    variant_t modules) {
  // Flatten the map.
  value_t deps = build_fragment_dependency_map(runtime, C(modules));
  value_t flat_deps = sort_and_flatten_map(runtime, deps);
  for (size_t i = 0; i < get_pair_array_length(flat_deps); i++) {
    // Flatten the stage dependency maps.
    value_t map = get_pair_array_second_at(flat_deps, i);
    value_t flat_map = sort_and_flatten_map(runtime, map);
    set_pair_array_second_at(flat_deps, i, flat_map);
  }
  ASSERT_VAREQ(expected, flat_deps);
}

TEST(bind, dependency_map) {
  CREATE_RUNTIME();

  // Two stages, 0 and -1, yield 0 -> -1.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP( 0, I(-1, root)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E))));

  // Three stages, 0, -1, and -2, yield 0 -> -1, -1 -> -2.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-1, I(-2, root)),
          FDEP( 0, I(-1, root)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E),
          FRG(-2, E))));

  // Simple import dependencies.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP( 0, I(0, other)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E))));

  // Not quite so simple import dependencies.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-1, I(-1, other)),
          FDEP( 0, I( 0, other))),
        MDEP(other,
          FDEP( 0, I(-1, other)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));

  // Stage shifting imports.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-2, I(-1, other)),
          FDEP(-1, I( 0, other))),
        MDEP(other,
          FDEP( 0, I(-1, other)))),
      B(MOD(root,
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));

  DISPOSE_RUNTIME();
}

static void test_load_order(runtime_t *runtime, variant_t expected,
    variant_t modules) {
  value_t modules_val = C(modules);
  value_t deps = build_fragment_dependency_map(runtime, modules_val);
  value_t schedule = build_bind_schedule(runtime, modules_val, deps);
  ASSERT_VAREQ(expected, schedule);
}

TEST(bind, load_order) {
  CREATE_RUNTIME();

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
  // ordering of the modules in the input array buffer.
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
      B(I(-2, other), I(-1, other), I(-1, root), I(0, other), I(0, root)),
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
  // Here the outcome depends particularly on the input ordering.
  test_load_order(runtime,
      B(I(-1, a), I(0, a), I(-1, b), I(-1, root), I(0, b), I(0, root)),
      B(MOD(root,
          FRG(0, A(IMP(a), IMP(b))),
          FRG(-1, E)),
        MOD(a,
          FRG(0, E),
          FRG(-1, E)),
        MOD(b,
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

  // Past imports
  test_load_order(runtime,
      B(I(0, other), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG(0, E))));
  test_load_order(runtime,
      B(I(-1, other), I(0, other), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG(0, E),
          FRG(-1, E))));
  test_load_order(runtime,
      B(I(-1, b), I(-1, a), I(0, b), I(0, a), I(-1, root), I(0, root)),
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
      B(I(-1, b), I(0, b), I(-1, a), I(0, a), I(-1, root), I(0, root)),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, A(IMP(a)))),
        MOD(a,
          FRG(0, E),
          FRG(-1, A(IMP(b)))),
        MOD(b,
          FRG( 0, E),
          FRG(-1, E))));
  DISPOSE_RUNTIME();
}
