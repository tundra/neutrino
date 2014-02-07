// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "check.h"
#include "log.h"
#include "runtime-inl.h"
#include "safe-inl.h"
#include "try-inl.h"
#include "value-inl.h"


// --- R o o t s ---

TRIVIAL_PRINT_ON_IMPL(Roots, roots);

static value_t create_stack_bottom_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  TRY(assembler_emit_stack_bottom(&assm));
  blob_t blob;
  short_buffer_flush(&assm.code, &blob);
  TRY_DEF(bytecode, new_heap_blob_with_data(runtime, &blob));
  assembler_dispose(&assm);
  return new_heap_code_block(runtime, bytecode, ROOT(runtime, empty_array), 1);
}

static value_t create_stack_piece_bottom_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  TRY(assembler_emit_stack_piece_bottom(&assm));
  blob_t blob;
  short_buffer_flush(&assm.code, &blob);
  TRY_DEF(bytecode, new_heap_blob_with_data(runtime, &blob));
  assembler_dispose(&assm);
  return new_heap_code_block(runtime, bytecode, ROOT(runtime, empty_array), 1);
}

value_t roots_init(value_t roots, runtime_t *runtime) {
  // The modal meta-roots are tricky because the species relationship between
  // them is circular.
  TRY_DEF(fluid_meta, new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior,
      vmFluid, rk_fluid_species_species));
  TRY_DEF(mutable_meta, new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior,
      vmMutable, rk_fluid_species_species));
  TRY_DEF(frozen_meta, new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior,
      vmFrozen, rk_fluid_species_species));
  TRY_DEF(deep_frozen_meta, new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior,
      vmDeepFrozen, rk_fluid_species_species));
  set_object_header(fluid_meta, mutable_meta);
  set_object_header(mutable_meta, mutable_meta);
  set_object_header(frozen_meta, mutable_meta);
  set_object_header(deep_frozen_meta, mutable_meta);
  RAW_ROOT(roots, fluid_species_species) = fluid_meta;
  RAW_ROOT(roots, mutable_species_species) = mutable_meta;
  RAW_ROOT(roots, frozen_species_species) = frozen_meta;
  RAW_ROOT(roots, deep_frozen_species_species) = deep_frozen_meta;

  // Generate initialization for the other compact species.
#define __CREATE_COMPACT_SPECIES__(Family, family) \
  TRY_SET(RAW_ROOT(roots, family##_species), new_heap_compact_species(         \
      runtime, &k##Family##Behavior));
#define __CREATE_MODAL_SPECIES__(Family, family)                               \
  TRY_SET(RAW_ROOT(roots, fluid_##family##_species),new_heap_modal_species(    \
      runtime, &k##Family##Behavior, vmFluid, rk_fluid_##family##_species));   \
  TRY_SET(RAW_ROOT(roots, mutable_##family##_species), new_heap_modal_species( \
      runtime, &k##Family##Behavior, vmMutable, rk_fluid_##family##_species)); \
  TRY_SET(RAW_ROOT(roots, frozen_##family##_species), new_heap_modal_species(  \
      runtime, &k##Family##Behavior, vmFrozen, rk_fluid_##family##_species));  \
  TRY_SET(RAW_ROOT(roots, deep_frozen_##family##_species), new_heap_modal_species(\
      runtime, &k##Family##Behavior, vmDeepFrozen, rk_fluid_##family##_species));
#define __CREATE_OTHER_SPECIES__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)\
  MD(__CREATE_MODAL_SPECIES__(Family, family),__CREATE_COMPACT_SPECIES__(Family, family))
  ENUM_OTHER_OBJECT_FAMILIES(__CREATE_OTHER_SPECIES__)
#undef __CREATE_OTHER_SPECIES__
#undef __CREATE_COMPACT_SPECIES__
#undef __CREATE_MODAL_SPECIES__

  // At this point we'll have created the root species so we can set its header.
  CHECK_EQ("roots already initialized", vdInteger, get_value_domain(get_object_header(roots)));
  set_object_species(roots, RAW_ROOT(roots, mutable_roots_species));

  // Generates code for initializing a string table entry.
#define __CREATE_STRING_TABLE_ENTRY__(name, value)                             \
  do {                                                                         \
    string_t contents;                                                         \
    string_init(&contents, value);                                             \
    TRY_SET(RAW_RSTR(roots, name), new_heap_string(runtime, &contents));       \
  } while (false);
  ENUM_STRING_TABLE(__CREATE_STRING_TABLE_ENTRY__)
#undef __CREATE_STRING_TABLE_ENTRY__

  // Initialize singletons first since we need those to create more complex
  // values below.
  TRY_DEF(empty_array, new_heap_array(runtime, 0));
  RAW_ROOT(roots, empty_array) = empty_array;
  TRY_SET(RAW_ROOT(roots, empty_array_buffer), new_heap_array_buffer(runtime, 0));
  TRY_SET(RAW_ROOT(roots, empty_path), new_heap_path(runtime, afFreeze, nothing(),
      nothing()));
  TRY_SET(RAW_ROOT(roots, any_guard), new_heap_guard(runtime, afFreeze, gtAny, null()));
  TRY_DEF(empty_type, new_heap_type(runtime, afFreeze, nothing(), null()));
  TRY(validate_deep_frozen(runtime, empty_type, NULL));
  TRY_SET(RAW_ROOT(roots, empty_instance_species),
      new_heap_instance_species(runtime, empty_type, nothing()));
  TRY_SET(RAW_ROOT(roots, subject_key), new_heap_key(runtime, RAW_RSTR(roots, subject)));
  TRY_SET(RAW_ROOT(roots, selector_key), new_heap_key(runtime, RAW_RSTR(roots, selector)));
  TRY_SET(RAW_ROOT(roots, ctrino_methodspace), new_heap_methodspace(runtime));
  TRY_SET(RAW_ROOT(roots, builtin_impls), new_heap_id_hash_map(runtime, 256));
  TRY_SET(RAW_ROOT(roots, op_call), new_heap_operation(runtime, afFreeze, otCall, null()));
  TRY_SET(RAW_ROOT(roots, ctrino), new_heap_ctrino(runtime));
  TRY_SET(RAW_ROOT(roots, stack_bottom_code_block),
      create_stack_bottom_code_block(runtime));
  TRY_SET(RAW_ROOT(roots, stack_piece_bottom_code_block),
      create_stack_piece_bottom_code_block(runtime));

  // Generate initialization for the per-family types.
  value_t core_type_origin = get_ambience_present_core_fragment_redirect();
#define __CREATE_TYPE__(Name, name) do {                                       \
  string_t __display_name_str__;                                               \
  string_init(&__display_name_str__, #Name);                                   \
  TRY_DEF(__display_name__, new_heap_string(runtime, &__display_name_str__));  \
  TRY_SET(RAW_ROOT(roots, name##_type), new_heap_type(runtime, afFreeze,       \
      core_type_origin, __display_name__));                                    \
} while (false);
  __CREATE_TYPE__(Integer, integer);
#define __CREATE_FAMILY_TYPE_OPT__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)\
  SR(__CREATE_TYPE__(Family, family),)
  ENUM_OBJECT_FAMILIES(__CREATE_FAMILY_TYPE_OPT__)
#undef __CREATE_FAMILY_TYPE_OPT__

  // Generate initialization for the per-phylum types.
#define __CREATE_PHYLUM_TYPE__(Phylum, phylum, CM, SR)                         \
  SR(__CREATE_TYPE__(Phylum, phylum),)
  ENUM_CUSTOM_TAGGED_PHYLUMS(__CREATE_PHYLUM_TYPE__)
#undef __CREATE_PHYLUM_TYPE__

#undef __CREATE_TYPE__


  TRY_DEF(plankton_environment, new_heap_id_hash_map(runtime, 16));
  init_plankton_core_factories(plankton_environment, runtime);
  init_plankton_syntax_factories(plankton_environment, runtime);
  RAW_ROOT(roots, plankton_environment) = plankton_environment;

  return success();
}

// Check that the condition holds, otherwise check fail with a validation error.
#define VALIDATE_CHECK_TRUE(EXPR)                                              \
COND_CHECK_TRUE("validation", ccValidationFailed, EXPR)

// Check that A and B are equal, otherwise check fail with a validation error.
#define VALIDATE_CHECK_EQ(A, B)                                                \
COND_CHECK_EQ("validation", ccValidationFailed, A, B)

value_t roots_validate(value_t roots) {
  // Checks whether the argument is within the specified family, otherwise
  // signals a validation failure.
  #define VALIDATE_OBJECT(ofFamily, value) do {                                \
    VALIDATE_CHECK_TRUE(in_family(ofFamily, (value)));                         \
    TRY(object_validate(value));                                               \
  } while (false)

  // Checks that the given value is a species with the specified instance
  // family.
  #define VALIDATE_SPECIES(ofFamily, value) do {                               \
    VALIDATE_OBJECT(ofSpecies, value);                                         \
    VALIDATE_CHECK_EQ(get_species_instance_family(value), ofFamily);           \
    TRY(object_validate(value));                                               \
  } while (false)

  // Checks all the species that belong to the given modal family.
  #define VALIDATE_ALL_MODAL_SPECIES(ofFamily, family)                                              \
    VALIDATE_MODAL_SPECIES(ofFamily, vmFluid, RAW_ROOT(roots, fluid_##family##_species));           \
    VALIDATE_MODAL_SPECIES(ofFamily, vmMutable, RAW_ROOT(roots, mutable_##family##_species));       \
    VALIDATE_MODAL_SPECIES(ofFamily, vmFrozen, RAW_ROOT(roots, frozen_##family##_species));         \
    VALIDATE_MODAL_SPECIES(ofFamily, vmDeepFrozen, RAW_ROOT(roots, deep_frozen_##family##_species))

  // Checks that the given value is a modal species for the given family and in
  // the given mode.
  #define VALIDATE_MODAL_SPECIES(ofFamily, vmMode, value) do {                 \
    VALIDATE(get_species_division(value) == sdModal);                          \
    VALIDATE(get_modal_species_mode(value) == vmMode);                         \
    VALIDATE_SPECIES(ofFamily, value);                                         \
  } while (false)

  // Generate validation for species.
#define __VALIDATE_PER_FAMILY_FIELDS__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW)\
  MD(                                                                          \
    VALIDATE_ALL_MODAL_SPECIES(of##Family, family),                            \
    VALIDATE_SPECIES(of##Family, RAW_ROOT(roots, family##_species)));          \
  SR(                                                                          \
    VALIDATE_OBJECT(ofType, RAW_ROOT(roots, family##_type));,                  \
    )
  ENUM_OBJECT_FAMILIES(__VALIDATE_PER_FAMILY_FIELDS__)
#undef __VALIDATE_PER_FAMILY_FIELDS__

  // Generate validation for phylums.
#define __VALIDATE_PER_PHYLUM_FIELDS__(Phylum, phylum, CM, SR)                 \
  SR(                                                                          \
    VALIDATE_OBJECT(ofType, RAW_ROOT(roots, phylum##_type));,                  \
    )
  ENUM_CUSTOM_TAGGED_PHYLUMS(__VALIDATE_PER_PHYLUM_FIELDS__)
#undef __VALIDATE_PER_PHYLUM_FIELDS__

  // Validate singletons manually.
  VALIDATE_OBJECT(ofArray, RAW_ROOT(roots, empty_array));
  VALIDATE_OBJECT(ofArrayBuffer, RAW_ROOT(roots, empty_array_buffer));
  VALIDATE_CHECK_EQ(0, get_array_buffer_length(RAW_ROOT(roots, empty_array_buffer)));
  VALIDATE_OBJECT(ofPath, RAW_ROOT(roots, empty_path));
  VALIDATE_CHECK_TRUE(is_path_empty(RAW_ROOT(roots, empty_path)));
  VALIDATE_OBJECT(ofGuard, RAW_ROOT(roots, any_guard));
  VALIDATE_CHECK_EQ(gtAny, get_guard_type(RAW_ROOT(roots, any_guard)));
  VALIDATE_OBJECT(ofType, RAW_ROOT(roots, integer_type));
  VALIDATE_OBJECT(ofSpecies, RAW_ROOT(roots, empty_instance_species));
  VALIDATE_OBJECT(ofKey, RAW_ROOT(roots, subject_key));
  VALIDATE_CHECK_EQ(0, get_key_id(RAW_ROOT(roots, subject_key)));
  VALIDATE_OBJECT(ofKey, RAW_ROOT(roots, selector_key));
  VALIDATE_CHECK_EQ(1, get_key_id(RAW_ROOT(roots, selector_key)));
  VALIDATE_OBJECT(ofMethodspace, RAW_ROOT(roots, ctrino_methodspace));
  VALIDATE_OBJECT(ofIdHashMap, RAW_ROOT(roots, builtin_impls));
  VALIDATE_OBJECT(ofOperation, RAW_ROOT(roots, op_call));
  VALIDATE_CHECK_EQ(otCall, get_operation_type(RAW_ROOT(roots, op_call)));

#define __VALIDATE_STRING_TABLE_ENTRY__(name, value) VALIDATE_OBJECT(ofString, RAW_RSTR(roots, name));
  ENUM_STRING_TABLE(__VALIDATE_STRING_TABLE_ENTRY__)
#undef __VALIDATE_STRING_TABLE_ENTRY__

  #undef VALIDATE_TYPE
  #undef VALIDATE_SPECIES
  return success();
}

value_t ensure_roots_owned_values_frozen(runtime_t *runtime, value_t self) {
  // Freeze *all* the things!
  value_field_iter_t iter;
  value_field_iter_init(&iter, self);
  value_t *field = NULL;
  while (value_field_iter_next(&iter, &field))
    TRY(ensure_frozen(runtime, *field));
  return success();
}


// --- M u t a b l e   r o o t s ---

TRIVIAL_PRINT_ON_IMPL(MutableRoots, mutable_roots);

value_t mutable_roots_validate(value_t self) {
  VALIDATE_FAMILY(ofMutableRoots, self);
  VALIDATE_OBJECT(ofArgumentMapTrie, RAW_MROOT(self, argument_map_trie_root));
  return success();
}

value_t ensure_mutable_roots_owned_values_frozen(runtime_t *runtime, value_t self) {
  // Why would you freeze the mutable roots -- they're supposed to be mutable!
  UNREACHABLE("freezing the mutable roots");
  return new_condition(ccWat);
}

void gc_fuzzer_init(gc_fuzzer_t *fuzzer, size_t min_freq, size_t mean_freq,
    size_t seed) {
  CHECK_REL("min frequency must be nonzero", min_freq, >, 0);
  CHECK_REL("mean frequency must be nonzero", mean_freq, >, 0);
  // It's best if we can vary the min frequency freely without breaking anything
  // so rather than assert that the mean is larger we just adjust it if we have
  // to.
  if (mean_freq <= min_freq)
    mean_freq = min_freq + 1;
  pseudo_random_init(&fuzzer->random, seed);
  fuzzer->min_freq = min_freq;
  fuzzer->spread = (mean_freq - min_freq) * 2;
  fuzzer->remaining = 0;
  fuzzer->is_enabled = true;
  gc_fuzzer_tick(fuzzer);
}

bool gc_fuzzer_tick(gc_fuzzer_t *fuzzer) {
  if (!fuzzer->is_enabled)
    return false;
  if (fuzzer->remaining == 0) {
    // This is where we fail. First, generate a new remaining tick count.
    fuzzer->remaining = pseudo_random_next(&fuzzer->random, fuzzer->spread) +
        fuzzer->min_freq;
    return true;
  } else {
    fuzzer->remaining--;
    return false;
  }
}

value_t new_runtime(runtime_config_t *config, runtime_t **runtime_out) {
  memory_block_t memory = allocator_default_malloc(sizeof(runtime_t));
  CHECK_EQ("wrong runtime_t memory size", sizeof(runtime_t), memory.size);
  runtime_t *runtime = (runtime_t*) memory.memory;
  TRY(runtime_init(runtime, config));
  *runtime_out = runtime;
  return success();
}

value_t delete_runtime(runtime_t *runtime) {
  TRY(runtime_dispose(runtime));
  allocator_default_free(new_memory_block(runtime, sizeof(runtime_t)));
  return success();
}

// The least number of allocations between forced allocation failures.
static const size_t kGcFuzzerMinFrequency = 64;

// Perform "hard" initialization, the stuff where the runtime isn't fully
// consistent yet.
static value_t runtime_hard_init(runtime_t *runtime, const runtime_config_t *config) {
  // Initialize the heap and roots. After this the runtime is sort-of ready to
  // be used.
  TRY(heap_init(&runtime->heap, config));
  TRY_SET(runtime->roots, new_heap_uninitialized_roots(runtime));
  TRY(roots_init(runtime->roots, runtime));
  TRY_SET(runtime->mutable_roots, new_heap_mutable_roots(runtime));
  // Check that everything looks sane.
  return runtime_validate(runtime);
}

// Perform "soft" initialization, the stuff where we're starting to rely on the
// runtime being fully functional.
static value_t runtime_soft_init(runtime_t *runtime) {
  TRY_DEF(module_loader, new_heap_empty_module_loader(runtime));
  runtime->module_loader = runtime_protect_value(runtime, module_loader);
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  E_BEGIN_TRY_FINALLY();
    safe_value_t s_ctrino_methodspace = protect(pool,
        ROOT(runtime, ctrino_methodspace));
    E_TRY(add_ctrino_builtin_methods(runtime, s_ctrino_methodspace));
    safe_value_t s_builtin_impls = protect(pool,
        ROOT(runtime, builtin_impls));
    E_TRY(add_builtin_implementations(runtime, s_builtin_impls));
    E_TRY(init_plankton_environment_mapping(&runtime->plankton_mapping, runtime));
    E_RETURN(runtime_validate(runtime));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

// Freeze the runtime such that any state that can be shared is deep frozen.
static value_t runtime_freeze_shared_state(runtime_t *runtime) {
  value_t roots = runtime->roots;

  // The roots object must be deep frozen.
  TRY(ensure_frozen(runtime, roots));

  value_t offender = whatever();
  TRY_DEF(froze, try_validate_deep_frozen(runtime, roots, &offender));
  if (!get_boolean_value(froze)) {
    ERROR("Could not freeze the roots object; offender: %v", offender);
    return new_not_deep_frozen_condition();
  }

  return success();
}

value_t runtime_init(runtime_t *runtime, const runtime_config_t *config) {
  if (config == NULL)
    config = runtime_config_get_default();
  // First reset all the fields to a well-defined value.
  runtime_clear(runtime);
  TRY(runtime_hard_init(runtime, config));
  TRY(runtime_soft_init(runtime));
  TRY(runtime_freeze_shared_state(runtime));
  // Set up gc fuzzing. For now do this after the initialization to exempt that
  // from being fuzzed. Longer term (probably after this has been rewritten) we
  // want more of this to be gc safe.
  if (config->gc_fuzz_freq > 0) {
    memory_block_t memory = allocator_default_malloc(
        sizeof(gc_fuzzer_t));
    runtime->gc_fuzzer = (gc_fuzzer_t*) memory.memory;
    gc_fuzzer_init(runtime->gc_fuzzer, kGcFuzzerMinFrequency,
        config->gc_fuzz_freq, config->gc_fuzz_seed);
  }
  return success();
}

// Adaptor function for passing object validate as a value callback.
static value_t runtime_validate_object(value_t value, value_callback_t *self) {
  CHECK_DOMAIN(vdObject, value);
  return object_validate(value);
}

value_t runtime_validate(runtime_t *runtime) {
  TRY(heap_validate(&runtime->heap));
  value_callback_t validate_callback;
  value_callback_init(&validate_callback, runtime_validate_object, NULL);
  TRY(heap_for_each_object(&runtime->heap, &validate_callback));
  return success();
}

// A record of an object that needs to be fixed up post-migration.
typedef struct {
  // The new object that we're migrating to. All fields have already been
  // migrated and the object will be fully functional at the time of the fixup.
  value_t new_object;
  // The old object that is about to be discarded but which is intact except
  // that the header has been overwritten by a forward pointer. This object
  // will not be used in any way after this so it can also just be used as a
  // block of memory.
  value_t old_object;
} pending_fixup_t;

// A record of all the fixups to perform after migration.
typedef struct {
  // The capacity of the array.
  size_t capacity;
  // The number of entries used in the array.
  size_t length;
  // The fixups.
  pending_fixup_t *fixups;
  // The memory where the fixups are stored.
  memory_block_t memory;
} pending_fixup_worklist_t;

// Initialize a worklist. This doesn't allocate any storage, that only happens
// when elements are added to the list.
static void pending_fixup_worklist_init(pending_fixup_worklist_t *worklist) {
  worklist->capacity = 0;
  worklist->length = 0;
  worklist->fixups = NULL;
  worklist->memory = memory_block_empty();
}

// Add a new fixup to the list. This returns a value such that it can return
// a condition if the system runs out of memory.
static value_t pending_fixup_worklist_add(pending_fixup_worklist_t *worklist,
    pending_fixup_t *fixup) {
  if (worklist->capacity == worklist->length) {
    // There's no room for the new element so increase capacity.
    size_t old_capacity = worklist->capacity;
    size_t new_capacity = (old_capacity == 0) ? 16 : 2 * old_capacity;
    memory_block_t new_memory = allocator_default_malloc(
        new_capacity * sizeof(pending_fixup_t));
    pending_fixup_t *new_fixups = (pending_fixup_t*) new_memory.memory;
    if (old_capacity > 0) {
      // Copy the previous data over to the new backing store.
      memcpy(new_fixups, worklist->fixups, worklist->length * sizeof(pending_fixup_t));
      // Free the old backing store.
      allocator_default_free(worklist->memory);
    }
    worklist->fixups = new_fixups;
    worklist->capacity = new_capacity;
    worklist->memory = new_memory;
  }
  worklist->fixups[worklist->length++] = *fixup;
  return success();
}

// Dispose the given worklist.
static void pending_fixup_worklist_dispose(pending_fixup_worklist_t *worklist) {
  if (!memory_block_is_empty(worklist->memory)) {
    allocator_default_free(worklist->memory);
    worklist->memory = memory_block_empty();
    worklist->fixups = NULL;
  }
}

// State maintained during garbage collection.
typedef struct {
  // The runtime we're collecting.
  runtime_t *runtime;
  // List of objects to post-process after migration.
  pending_fixup_worklist_t pending_fixups;
} garbage_collection_state_t;

// Initializes a garbage collection state object.
static void garbage_collection_state_init(garbage_collection_state_t *state,
    runtime_t *runtime) {
  state->runtime = runtime;
  pending_fixup_worklist_init(&state->pending_fixups);
}

// Disposes a garbage collection state object.
static void garbage_collection_state_dispose(garbage_collection_state_t *state) {
  pending_fixup_worklist_dispose(&state->pending_fixups);
}

// Allocates memory in to-space for the given object and copies it raw into that
// memory, leaving fields unmigrated.
static value_t migrate_object_shallow(value_t object, space_t *space) {
  // Ask the object to describe its layout.
  object_layout_t layout;
  get_object_layout(object, &layout);
  // Allocate new room for the object.
  address_t source = get_object_address(object);
  address_t target = NULL;
  bool alloc_succeeded = space_try_alloc(space, layout.size, &target);
  CHECK_TRUE("clone alloc failed", alloc_succeeded);
  // Do a raw copy of the object to the target.
  memcpy(target, source, layout.size);
  // Tag the new location as an object and return it.
  return new_object(target);
}

// Returns true if the given object needs to apply a fixup after migration.
static bool needs_post_migrate_fixup(value_t old_object) {
  return get_object_family_behavior_unchecked(old_object)->post_migrate_fixup != NULL;
}

// Callback that migrates an object from from to to space, if it hasn't been
// migrated already.
static value_t migrate_field_shallow(value_t *field, field_callback_t *callback) {
//  runtime_t *runtime = field_callback_get_data(callback);
  value_t old_object = *field;
  // If this is not a heap object there's nothing to do.
  if (get_value_domain(old_object) != vdObject)
    return success();
  // Check if this object has already been moved.
  value_t old_header = get_object_header(old_object);
  value_t new_object;
  if (get_value_domain(old_header) == vdMovedObject) {
    // This object has already been moved and the header points to the new
    // location so we just get out the location of the migrated object and update
    // the field.
    new_object = get_moved_object_target(old_header);
  } else {
    // The header indicates that this object hasn't been moved yet. First make
    // a raw clone of the object in to-space.
    CHECK_DOMAIN(vdObject, old_header);
    garbage_collection_state_t *state =
        (garbage_collection_state_t*) field_callback_get_data(callback);
    // Check with the object whether it needs post processing. This is the last
    // time the object is intact so it's the last point we can call methods on
    // it to find out.
    bool needs_fixup = needs_post_migrate_fixup(old_object);
    new_object = migrate_object_shallow(old_object, &state->runtime->heap.to_space);
    CHECK_DOMAIN(vdObject, new_object);
    // Now that we know where the new object is going to be we can schedule the
    // fixup if necessary.
    if (needs_fixup) {
      pending_fixup_t fixup = {new_object, old_object};
      TRY(pending_fixup_worklist_add(&state->pending_fixups, &fixup));
    }
    // Point the old object to the new one so we know to use the new clone
    // instead of ever cloning it again.
    value_t forward_pointer = new_moved_object(new_object);
    set_object_header(old_object, forward_pointer);
    // At this point the cloned object still needs some work to update the
    // fields but we rely on traversing the heap to do that eventually.
  }
  *field = new_object;
  return success();
}

// Applies a post-migration fixup scheduled when migrating the given object.
static void apply_fixup(runtime_t *runtime, value_t new_object, value_t old_object) {
  family_behavior_t *behavior = get_object_family_behavior_unchecked(new_object);
  (behavior->post_migrate_fixup)(runtime, new_object, old_object);
}

// Perform any fixups that have been scheduled during object migration.
static void runtime_apply_fixups(garbage_collection_state_t *state) {
  pending_fixup_worklist_t *worklist = &state->pending_fixups;
  for (size_t i = 0; i < worklist->length; i++) {
    pending_fixup_t *fixup = &worklist->fixups[i];
    apply_fixup(state->runtime, fixup->new_object, fixup->old_object);
  }
}

value_t runtime_garbage_collect(runtime_t *runtime) {
  // Validate that everything's healthy before we start.
  TRY(runtime_validate(runtime));
  // Create to-space and swap it in, making the current to-space into from-space.
  TRY(heap_prepare_garbage_collection(&runtime->heap));
  // Initialize the state we'll maintain during collection.
  garbage_collection_state_t state;
  garbage_collection_state_init(&state, runtime);
  // Create the migrator callback that will be used to migrate objects from from
  // to to space.
  field_callback_t migrate_shallow_callback;
  field_callback_init(&migrate_shallow_callback, migrate_field_shallow, &state);
  // Shallow migration of all the roots.
  TRY(field_callback_call(&migrate_shallow_callback, &runtime->roots));
  TRY(field_callback_call(&migrate_shallow_callback, &runtime->mutable_roots));
  // Shallow migration of everything currently stored in to-space which, since
  // we keep going until all objects have been migrated, effectively makes a deep
  // migration.
  TRY(heap_for_each_field(&runtime->heap, &migrate_shallow_callback));
  // At this point everything has been migrated so we can run the fixups and
  // then we're done with the state.
  runtime_apply_fixups(&state);
  garbage_collection_state_dispose(&state);
  // Now everything has been migrated so we can throw away from-space.
  TRY(heap_complete_garbage_collection(&runtime->heap));
  // Validate that everything's still healthy.
  return runtime_validate(runtime);
}

void runtime_clear(runtime_t *runtime) {
  runtime->next_key_index = 0;
  runtime->gc_fuzzer = NULL;
  runtime->roots = whatever();
  runtime->mutable_roots = whatever();
  runtime->plankton_mapping.data = NULL;
  runtime->plankton_mapping.function = NULL;
  runtime->module_loader = empty_safe_value();
}

value_t runtime_dispose(runtime_t *runtime) {
  TRY(runtime_validate(runtime));
  dispose_safe_value(runtime, runtime->module_loader);
  heap_dispose(&runtime->heap);
  if (runtime->gc_fuzzer != NULL) {
    allocator_default_free(new_memory_block(runtime->gc_fuzzer, sizeof(gc_fuzzer_t)));
    runtime->gc_fuzzer = NULL;
  }
  return success();
}

safe_value_t runtime_protect_value(runtime_t *runtime, value_t value) {
  if (get_value_domain(value) == vdObject) {
    object_tracker_t *gc_safe = heap_new_object_tracker(&runtime->heap, value);
    return object_tracker_to_safe_value(gc_safe);
  } else {
    return protect_immediate(value);
  }
}

value_t runtime_plankton_deserialize(runtime_t *runtime, value_t blob) {
  return plankton_deserialize(runtime, &runtime->plankton_mapping, blob);
}

value_t safe_runtime_plankton_deserialize(runtime_t *runtime, safe_value_t blob) {
  RETRY_ONCE_IMPL(runtime, runtime_plankton_deserialize(runtime, deref(blob)));
}

void dispose_safe_value(runtime_t *runtime, safe_value_t s_value) {
  if (!safe_value_is_immediate(s_value)) {
    object_tracker_t *gc_safe = safe_value_to_object_tracker(s_value);
    heap_dispose_object_tracker(&runtime->heap, gc_safe);
  }
}

void runtime_toggle_fuzzing(runtime_t *runtime, bool enable) {
  gc_fuzzer_t *fuzzer = runtime->gc_fuzzer;
  if (fuzzer == NULL)
    return;
  CHECK_EQ("invalid fuzz toggle", !enable, fuzzer->is_enabled);
  fuzzer->is_enabled = enable;
}

value_t get_modal_species_sibling_with_mode(runtime_t *runtime, value_t species,
    value_mode_t mode) {
  CHECK_DIVISION(sdModal, species);
  root_key_t base_root = (root_key_t) get_modal_species_base_root(species);
  root_key_t mode_root = (root_key_t) (base_root + mode - vmFluid);
  value_t result = get_roots_entry_at(runtime->roots, mode_root);
  CHECK_EQ("incorrect sibling mode", mode, get_modal_species_mode(result));
  CHECK_EQ("incorrect sibling family", get_species_instance_family(species),
      get_species_instance_family(result));
  return result;
}

value_t runtime_get_builtin_implementation(runtime_t *runtime, value_t name) {
  value_t builtins = ROOT(runtime, builtin_impls);
  value_t impl = get_id_hash_map_at(builtins, name);
  if (is_condition(ccNotFound, impl)) {
    WARN("Unknown builtin %v", name);
    return new_unknown_builtin_condition();
  } else {
    return impl;
  }
}
