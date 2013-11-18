// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "bind.h"
#include "test.h"

value_t expand_variant_to_unbound_module(runtime_t *runtime, variant_t *variant) {
  TRY_DEF(fields, expand_variant_to_array(runtime, variant));
  value_t path = get_array_at(fields, 0);
  value_t fragments = get_array_at(fields, 1);
  return new_heap_unbound_module(runtime, path, fragments);
}

// Creates a variant that expands to an unbound module with the given path and
// fragments.
#define vUnboundModule(path, ...) vBuild(                                      \
  expand_variant_to_unbound_module,                                            \
  vArrayPayload(path, vArray(__VA_ARGS__)))

value_t expand_variant_to_unbound_fragment(runtime_t *runtime, variant_t *variant) {
  TRY_DEF(fields, expand_variant_to_array(runtime, variant));
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

// Shorthands for creating dependency calculation input.
#define MOD(N, ...) vUnboundModule(vPath(vStr(#N)), __VA_ARGS__)
#define FRG(S, I) vUnboundFragment(vInt(S), I)
#define IMP(N) vPath(vStr(#N))

// Macros for creating expected dependency calculation output.
#define MDEP(P, ...) vPath(vStr(#P)), vArray(__VA_ARGS__)
#define FDEP(S, ...) vInt(S), vArrayBuffer(__VA_ARGS__)
#define DEP(I, S) vArray(vPath(vStr(#I)), vInt(S))

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
          FDEP( 0, DEP(root, -1)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E))));

  // Three stages, 0, -1, and -2, yield 0 -> -1, -1 -> -2.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-1, DEP(root, -2)),
          FDEP( 0, DEP(root, -1)))),
      B(MOD(root,
          FRG( 0, E),
          FRG(-1, E),
          FRG(-2, E))));

  // Simple import dependencies.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP( 0, DEP(other, 0)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E))));

  // Not quite so simple import dependencies.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-1, DEP(other, -1)),
          FDEP( 0, DEP(other, 0))),
        MDEP(other,
          FDEP( 0, DEP(other, -1)))),
      B(MOD(root,
          FRG( 0, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));

  // Stage shifting imports.
  test_dependency_map(runtime,
      A(MDEP(root,
          FDEP(-2, DEP(other, -1)),
          FDEP(-1, DEP(other, 0))),
        MDEP(other,
          FDEP( 0, DEP(other, -1)))),
      B(MOD(root,
          FRG(-1, A(IMP(other)))),
        MOD(other,
          FRG( 0, E),
          FRG(-1, E))));

  DISPOSE_RUNTIME();
}
