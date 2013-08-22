// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "check.h"
#include "runtime.h"
#include "value-inl.h"


value_t roots_init(roots_t *roots, runtime_t *runtime) {
  // The meta-root is tricky because it is its own species. So we set it up in
  // two steps.
  TRY_DEF(meta, new_heap_compact_species_unchecked(runtime, ofSpecies, &kSpeciesBehavior));
  set_object_header(meta, meta);
  roots->species_species = meta;

  // Generate initialization for the other compact species.
#define __CREATE_COMPACT_SPECIES__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)\
  TRY_SET(roots->family##_species, new_heap_compact_species(runtime, of##Family, &k##Family##Behavior));
  ENUM_COMPACT_OBJECT_FAMILIES(__CREATE_COMPACT_SPECIES__)
#undef __CREATE_COMPACT_SPECIES__

  // Initialize singletons first since we need those to create more complex
  // values below.
  TRY_SET(roots->null, new_heap_null(runtime));
  TRY_SET(roots->nothing, new_heap_nothing(runtime));
  TRY_SET(roots->thrue, new_heap_boolean(runtime, true));
  TRY_SET(roots->fahlse, new_heap_boolean(runtime, false));
  TRY_SET(roots->empty_array, new_heap_array(runtime, 0));
  TRY_SET(roots->empty_array_buffer, new_heap_array_buffer(runtime, 0));
  TRY_SET(roots->any_guard, new_heap_guard(runtime, gtAny, roots->null));
  TRY_SET(roots->integer_protocol, new_heap_protocol(runtime, roots->null));
  TRY_DEF(empty_protocol, new_heap_protocol(runtime, roots->null));
  TRY_SET(roots->empty_instance_species,
      new_heap_instance_species(runtime, empty_protocol));

  // Generate initialization for the per-family protocols.
#define __CREATE_FAMILY_PROTOCOL__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)\
  SUR(TRY_SET(roots->family##_protocol, new_heap_protocol(runtime, roots->null));,)
  ENUM_OBJECT_FAMILIES(__CREATE_FAMILY_PROTOCOL__)
#undef __CREATE_FAMILY_PROTOCOL__

  // Generates code for initializing a string table entry.
#define __CREATE_STRING_TABLE_ENTRY__(name, value)                             \
  do {                                                                         \
    string_t contents;                                                         \
    string_init(&contents, value);                                             \
    TRY_SET(roots->string_table.name, new_heap_string(runtime, &contents));    \
  } while (false);
  ENUM_STRING_TABLE(__CREATE_STRING_TABLE_ENTRY__)
#undef __CREATE_STRING_TABLE_ENTRY__

  TRY_DEF(syntax_factories, new_heap_id_hash_map(runtime, 16));
  init_syntax_factory_map(syntax_factories, runtime);
  roots->syntax_factories = syntax_factories;

  return success();
}

// Clears the field argument to a well-defined dummy value.
static value_t clear_field(value_t *field, field_callback_t *callback) {
  *field = success();
  return success();
}

void roots_clear(roots_t *roots) {
  field_callback_t clear_callback;
  field_callback_init(&clear_callback, clear_field, NULL);
  roots_for_each_field(roots, &clear_callback);
}

// Check that the condition holds, otherwise check fail with a validation error.
#define VALIDATE_CHECK_TRUE(EXPR)                                              \
SIG_CHECK_TRUE("validation", scValidationFailed, EXPR)

// Check that A and B are equal, otherwise check fail with a validation error.
#define VALIDATE_CHECK_EQ(A, B)                                                \
SIG_CHECK_EQ("validation", scValidationFailed, A, B)

value_t roots_validate(roots_t *roots) {
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

  // Generate validation for species.
#define __VALIDATE_PER_FAMILY_FIELDS__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)\
  VALIDATE_SPECIES(of##Family, roots->family##_species);                       \
  SUR(VALIDATE_OBJECT(ofProtocol, roots->family##_protocol);,)
  ENUM_OBJECT_FAMILIES(__VALIDATE_PER_FAMILY_FIELDS__)
#undef __VALIDATE_PER_FAMILY_FIELDS__

  // Validate singletons manually.
  VALIDATE_OBJECT(ofNull, roots->null);
  VALIDATE_OBJECT(ofNothing, roots->nothing);
  VALIDATE_OBJECT(ofBoolean, roots->thrue);
  VALIDATE_OBJECT(ofBoolean, roots->fahlse);
  VALIDATE_OBJECT(ofArray, roots->empty_array);
  VALIDATE_OBJECT(ofArrayBuffer, roots->empty_array_buffer);
  VALIDATE_CHECK_EQ(0, get_array_buffer_length(roots->empty_array_buffer));
  VALIDATE_OBJECT(ofGuard, roots->any_guard);
  VALIDATE_CHECK_EQ(gtAny, get_guard_type(roots->any_guard));
  VALIDATE_OBJECT(ofProtocol, roots->integer_protocol);
  VALIDATE_OBJECT(ofSpecies, roots->empty_instance_species);

#define __VALIDATE_STRING_TABLE_ENTRY__(name, value) VALIDATE_OBJECT(ofString, roots->string_table.name);
  ENUM_STRING_TABLE(__VALIDATE_STRING_TABLE_ENTRY__)
#undef __VALIDATE_STRING_TABLE_ENTRY__

  #undef VALIDATE_TYPE
  #undef VALIDATE_SPECIES
  return success();
}

value_t roots_for_each_field(roots_t *roots, field_callback_t *callback) {
  // Generate code for visiting the species.
#define __VISIT_PER_FAMILY_FIELDS__(Family, family, CMP, CID, CNT, SUR, NOL, FIX)\
  TRY(field_callback_call(callback, &roots->family##_species));                \
  SUR(TRY(field_callback_call(callback, &roots->family##_protocol));,)
  ENUM_OBJECT_FAMILIES(__VISIT_PER_FAMILY_FIELDS__)
#undef __VISIT_PER_FAMILY_FIELDS__

  // Clear the singletons manually.
  TRY(field_callback_call(callback, &roots->syntax_factories));
  TRY(field_callback_call(callback, &roots->null));
  TRY(field_callback_call(callback, &roots->nothing));
  TRY(field_callback_call(callback, &roots->thrue));
  TRY(field_callback_call(callback, &roots->fahlse));
  TRY(field_callback_call(callback, &roots->empty_array));
  TRY(field_callback_call(callback, &roots->empty_array_buffer));
  TRY(field_callback_call(callback, &roots->any_guard));
  TRY(field_callback_call(callback, &roots->integer_protocol));
  TRY(field_callback_call(callback, &roots->empty_instance_species));

  // Generate code for visiting the string table.
#define __VISIT_STRING_TABLE_ENTRY__(name, value)                              \
  TRY(field_callback_call(callback, &roots->string_table.name));
  ENUM_STRING_TABLE(__VISIT_STRING_TABLE_ENTRY__)
#undef __VISIT_STRING_TABLE_ENTRY__

  return success();
}

value_t runtime_init(runtime_t *runtime, space_config_t *config) {
  // First reset all the fields to a well-defined value.
  runtime_clear(runtime);
  TRY(heap_init(&runtime->heap, config));
  TRY(roots_init(&runtime->roots, runtime));
  return runtime_validate(runtime);
}

// Adaptor function for passing object validate as a value callback.
static value_t runtime_validate_object(value_t value, value_callback_t *self) {
  return object_validate(value);
}

value_t runtime_validate(runtime_t *runtime) {
  TRY(heap_validate(&runtime->heap));
  TRY(roots_validate(&runtime->roots));
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
} pending_fixup_worklist_t;

// Initialize a worklist. This doesn't allocate any storage, that only happens
// when elements are added to the list.
static void pending_fixup_worklist_init(pending_fixup_worklist_t *worklist) {
  worklist->capacity = 0;
  worklist->length = 0;
  worklist->fixups = NULL;
}

// Add a new fixup to the list. This returns a value such that it can return
// a signal if the system runs out of memory.
static value_t pending_fixup_worklist_add(pending_fixup_worklist_t *worklist,
    pending_fixup_t *fixup) {
  if (worklist->capacity == worklist->length) {
    // There's no room for the new element so increase capacity.
    size_t old_capacity = worklist->capacity;
    size_t new_capacity = (old_capacity == 0) ? 16 : 2 * old_capacity;
    allocator_t alloc;
    init_system_allocator(&alloc);
    pending_fixup_t *new_fixups = (pending_fixup_t*) allocator_malloc(&alloc,
        new_capacity * sizeof(pending_fixup_t));
    if (old_capacity > 0) {
      // Copy the previous data over to the new backing store.
      memcpy(new_fixups, worklist->fixups, worklist->length * sizeof(pending_fixup_t));
      // Free the old backing store.
      allocator_free(&alloc, (address_t) worklist->fixups);
    }
    worklist->fixups = new_fixups;
    worklist->capacity = new_capacity;
  }
  worklist->fixups[worklist->length++] = *fixup;
  return success();
}

// Dispose the given worklist.
static void pending_fixup_worklist_dispose(pending_fixup_worklist_t *worklist) {
  if (worklist->capacity > 0) {
    allocator_t alloc;
    init_system_allocator(&alloc);
    allocator_free(&alloc, (address_t) worklist->fixups);
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
    garbage_collection_state_t *state = field_callback_get_data(callback);
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
  TRY(roots_for_each_field(&runtime->roots, &migrate_shallow_callback));
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
  roots_clear(&runtime->roots);
}

value_t runtime_dispose(runtime_t *runtime) {
  TRY(runtime_validate(runtime));
  heap_dispose(&runtime->heap);
  return success();
}

value_t runtime_null(runtime_t *runtime) {
  return runtime->roots.null;
}

value_t runtime_nothing(runtime_t *runtime) {
  return runtime->roots.nothing;
}

value_t runtime_bool(runtime_t *runtime, bool which) {
  return which ? runtime->roots.thrue : runtime->roots.fahlse;
}

gc_safe_t *runtime_new_gc_safe(runtime_t *runtime, value_t value) {
  return heap_new_gc_safe(&runtime->heap, value);
}

void runtime_dispose_gc_safe(runtime_t *runtime, gc_safe_t *gc_safe) {
  heap_dispose_gc_safe(&runtime->heap, gc_safe);
}
