//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "ctrino.h"
#include "derived.h"
#include "format.h"
#include "freeze.h"
#include "io.h"
#include "runtime-inl.h"
#include "safe-inl.h"
#include "tagged-inl.h"
#include "try-inl.h"
#include "utils/check.h"
#include "utils/lifetime.h"
#include "utils/log.h"
#include "utils/ook-inl.h"
#include "value-inl.h"

// --- R o o t s ---

TRIVIAL_PRINT_ON_IMPL(Roots, roots);

// Creates the code block object that lives at the bottom of a stack and takes
// care of returning the value back from the interpreter and protects against
// returning through the bottom of the stack.
static value_t create_stack_bottom_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  TRY(assembler_emit_stack_bottom(&assm));
  blob_t blob = short_buffer_flush(&assm.code);
  TRY_DEF(bytecode, new_heap_blob_with_data(runtime, blob));
  assembler_dispose(&assm);
  size_t high_water_mark = 1 + kStackBarrierSize;
  return new_heap_code_block(runtime, bytecode, ROOT(runtime, empty_array),
      high_water_mark);
}

// Creates the code block object that gets executed when returning across stack
// pieces.
static value_t create_stack_piece_bottom_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  TRY(assembler_emit_stack_piece_bottom(&assm));
  blob_t blob = short_buffer_flush(&assm.code);
  TRY_DEF(bytecode, new_heap_blob_with_data(runtime, blob));
  assembler_dispose(&assm);
  return new_heap_code_block(runtime, bytecode, ROOT(runtime, empty_array), 1);
}

// Creates a code block that holds just a single return instruction.
static value_t create_return_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  // This is going to be used for frames that have various amounts of state on
  // the stack so avoid checking the stack height.
  TRY(assembler_emit_unchecked_return(&assm));
  blob_t blob = short_buffer_flush(&assm.code);
  TRY_DEF(bytecode, new_heap_blob_with_data(runtime, blob));
  assembler_dispose(&assm);
  return new_heap_code_block(runtime, bytecode, ROOT(runtime, empty_array), 1);
}

// Creates a code block that invokes the call method on an object.
static value_t create_call_thunk_code_block(runtime_t *runtime) {
  assembler_t assm;
  TRY(assembler_init_stripped_down(&assm, runtime));
  TRY(assembler_emit_load_raw_argument(&assm, 0));
  TRY(assembler_emit_push(&assm, ROOT(runtime, op_call)));
  TRY(assembler_emit_push(&assm, transport_sync()));
  TRY_DEF(tag_array, new_heap_pair_array(runtime, kImplicitArgumentCount));
  set_pair_array_first_at(tag_array, 0, ROOT(runtime, subject_key));
  set_pair_array_second_at(tag_array, 0, new_integer(2));
  set_pair_array_first_at(tag_array, 1, ROOT(runtime, selector_key));
  set_pair_array_second_at(tag_array, 1, new_integer(1));
  set_pair_array_first_at(tag_array, 2, ROOT(runtime, transport_key));
  set_pair_array_second_at(tag_array, 2, new_integer(0));
  TRY_DEF(tags, new_heap_call_tags(runtime, afFreeze, tag_array));
  TRY(assembler_emit_invocation(&assm, nothing(), tags, nothing()));
  TRY(assembler_emit_slap(&assm, kImplicitArgumentCount));
  TRY(assembler_emit_return(&assm));
  TRY_DEF(result, assembler_flush(&assm));
  assembler_dispose(&assm);
  return result;
}

// Populates the special imports map.
static value_t init_special_imports(runtime_t *runtime, value_t imports) {
  TRY_DEF(ctrino,
      new_c_object(runtime, ROOT(runtime, ctrino_factory), blob_new(NULL, 0), new_value_array(NULL, 0)));
  TRY(set_id_hash_map_at(runtime, imports, RSTR(runtime, ctrino), ctrino));
  TRY_DEF(time, new_native_remote(runtime, native_remote_time()));
  TRY(set_id_hash_map_at(runtime, imports, RSTR(runtime, time), time));
  return success();
}

// Populates the special imports map with services using the configurable
// service installer.
static value_t init_services(runtime_t *runtime, value_t imports,
    const extended_runtime_config_t *config) {
  unary_callback_t *install_hook = config->service_install_hook;
  if (install_hook == NULL)
    return success();
  service_install_hook_context_t context = {runtime, imports};
  TRY(o2v(unary_callback_call(install_hook, p2o(&context))));
  return success();
}

// Install the service described by the given descriptor in the given context.
value_t service_hook_add_service(service_install_hook_context_t *context,
    service_descriptor_t *service) {
  TRY_DEF(namespace_name, import_pton_variant(context->runtime,
      service->namespace_name));
  TRY_DEF(obj, new_native_remote(context->runtime, service));
  TRY(set_id_hash_map_at(context->runtime, context->imports, namespace_name, obj));
  return success();
}

// Creates an array of invocation records of the form
//
//   {%subject: N-1, %selector: N-2, %is_async: N-3, 0: N-4, ...}
//
// for up to kMaxSize entries (including the subject and selector). These are
// used for instance for raising signals from within builtins.
static value_t create_escape_records(runtime_t *runtime) {
  static const size_t kMaxSize = 5;
  TRY_DEF(result, new_heap_array(runtime, kMaxSize + 1));
  for (size_t i = kImplicitArgumentCount; i <= kMaxSize; i++) {
    // Build the entries manually. The indexes are somewhat fiddly because they
    // are counted backwards from the size-1.
    TRY_DEF(entries, new_heap_pair_array(runtime, i));
    set_pair_array_first_at(entries, 0, ROOT(runtime, subject_key));
    set_pair_array_second_at(entries, 0, new_integer(i - 1));
    set_pair_array_first_at(entries, 1, ROOT(runtime, selector_key));
    set_pair_array_second_at(entries, 1, new_integer(i - 2));
    set_pair_array_first_at(entries, 2, ROOT(runtime, transport_key));
    set_pair_array_second_at(entries, 2, new_integer(i - 3));
    for (size_t j = kImplicitArgumentCount; j < i; j++) {
      set_pair_array_first_at(entries, j, new_integer(j - kImplicitArgumentCount));
      set_pair_array_second_at(entries, j, new_integer(i - j - 1));
    }
    CHECK_TRUE("escape record not sorted", is_pair_array_sorted(entries));
    TRY_DEF(tags, new_heap_call_tags(runtime, afFreeze, entries));
    set_array_at(result, i, tags);
  }
  return result;
}

// Create the methodspace that holds all the built-in methods provided by the
// runtime.
static value_t apply_ctrino_plugin(runtime_t *runtime, value_t methodspace) {
  TRY_DEF(ctrino_factory, create_ctrino_factory(runtime, methodspace));
  return ctrino_factory;
}

static value_t apply_custom_plugins(runtime_t *runtime,
    const extended_runtime_config_t *config, value_t methodspace) {
  if (config->base.plugin_count == 0)
    return ROOT(runtime, empty_array);
  TRY_DEF(result, new_heap_array(runtime, config->base.plugin_count));
  for (size_t i = 0; i < config->base.plugin_count; i++) {
    const c_object_info_t *plugin = (const c_object_info_t*) config->base.plugins[i];
    if (plugin == NULL)
      continue;
    TRY_DEF(factory, new_c_object_factory(runtime, plugin, methodspace));
    set_array_at(result, i, factory);
  }
  return result;
}

// Information about a family passed when iterating over the types.
typedef struct {
  heap_object_family_t family;
  family_behavior_t *behavior;
  root_key_t root_key;
  const char *camel_name;
} family_info_t;

typedef value_t (family_callback_t)(family_info_t *info, void *data);

// Invokes the given callback for each modal species. The key given is the key
// for the fluid species.
static value_t roots_for_each_modal_species(family_callback_t callback, void *data) {
#define __CALL_CALLBACK__(Family, family) do {                                 \
  family_info_t info = {of##Family, &k##Family##Behavior, rk_fluid_##family##_species, #Family}; \
  TRY(callback(&info, data));                                                  \
} while (false)
#define __GEN_FAMILY_CLAUSE__(Family, family, MD, SR, MINOR, N)                \
    MD(__CALL_CALLBACK__(Family, family);,)
  ENUM_HEAP_OBJECT_FAMILIES(__GEN_FAMILY_CLAUSE__)
#undef __GEN_FAMILY_CLAUSE__
#undef __CALL_CALLBACK__
  return success();
}

// Invokes the given callback for each compact species. The key given is the key
// for the family's species.
static value_t roots_for_each_compact_species(family_callback_t callback, void *data) {
#define __CALL_CALLBACK__(Family, family) do {                                 \
  family_info_t info = {of##Family, &k##Family##Behavior, rk_##family##_species, #Family}; \
  TRY(callback(&info, data));                                                  \
} while (false)
#define __GEN_FAMILY_CLAUSE__(Family, family, MD, SR, MINOR, N)                \
    MD(,__CALL_CALLBACK__(Family, family);)
  ENUM_HEAP_OBJECT_FAMILIES(__GEN_FAMILY_CLAUSE__)
#undef __GEN_FAMILY_CLAUSE__
#undef __CALL_CALLBACK__
  return success();
}

// Invokes the given callback for each surface type. The key given is the key
// for the family's type.
static value_t roots_for_each_family_type(family_callback_t callback, void *data) {
#define __CALL_CALLBACK__(Family, family) do {                                 \
  family_info_t info = {of##Family, &k##Family##Behavior, rk_##family##_type, #Family}; \
  TRY(callback(&info, data));                                                  \
} while (false)
#define __GEN_FAMILY_CLAUSE__(Family, family, MD, SR, MINOR, N)                \
  SR(__CALL_CALLBACK__(Family, family);,)
  ENUM_HEAP_OBJECT_FAMILIES(__GEN_FAMILY_CLAUSE__)
#undef __GEN_FAMILY_CLAUSE__
#undef __CALL_CALLBACK__
  return success();
}

// Information about a phylum passed when iterating over the types.
typedef struct {
  custom_tagged_phylum_t phylum;
  phylum_behavior_t *behavior;
  root_key_t root_key;
  const char *camel_name;
} phylum_info_t;

typedef value_t (phylum_callback_t)(phylum_info_t *info, void *data);

// Invokes the given callback for each surface type. The key given is the key
// for the family's type.
static value_t roots_for_each_phylum_type(phylum_callback_t callback, void *data) {
#define __CALL_CALLBACK__(Phylum, phylum) do {                                 \
  phylum_info_t info = {tp##Phylum, &k##Phylum##PhylumBehavior, rk_##phylum##_type, #Phylum}; \
  TRY(callback(&info, data));                                                  \
} while (false)
#define __GEN_PHYLUM_CLAUSE__(Family, family, SR, MINOR, N)                    \
  SR(__CALL_CALLBACK__(Family, family);,)
  ENUM_CUSTOM_TAGGED_PHYLUMS(__GEN_PHYLUM_CLAUSE__)
#undef __GEN_PHYLUM_CLAUSE__
#undef __CALL_CALLBACK__
  return success();
}

// Given a root key for a modal species, and a desired mode, returns the root
// key for the species the same species in the desired mode.
static root_key_t get_modal_key_from_fluid(root_key_t fluid_key,
    value_mode_t mode) {
  return (root_key_t) (fluid_key + mode - vmFluid);
}

// Data passed into the species constructor callbacks.
typedef struct {
  value_t roots;
  runtime_t *runtime;
} init_roots_data_t;

static value_t roots_create_modal_species_callback(family_info_t *info, void *raw_data) {
  init_roots_data_t *data = (init_roots_data_t*) raw_data;
  root_key_t fluid_key = info->root_key;
  TRY_SET(*access_roots_entry_at(data->roots, fluid_key),
      new_heap_modal_species(data->runtime, info->behavior, vmFluid, fluid_key));
  root_key_t mutable_key = get_modal_key_from_fluid(fluid_key, vmMutable);
  TRY_SET(*access_roots_entry_at(data->roots, mutable_key),
      new_heap_modal_species(data->runtime, info->behavior, vmMutable, fluid_key));
  root_key_t frozen_key = get_modal_key_from_fluid(fluid_key, vmFrozen);
  TRY_SET(*access_roots_entry_at(data->roots, frozen_key),
      new_heap_modal_species(data->runtime, info->behavior, vmFrozen, fluid_key));
  root_key_t deep_frozen_key = get_modal_key_from_fluid(fluid_key, vmDeepFrozen);
  TRY_SET(*access_roots_entry_at(data->roots, deep_frozen_key),
      new_heap_modal_species(data->runtime, info->behavior, vmDeepFrozen, fluid_key));
  return success();
}

static value_t roots_create_compact_species_callback(family_info_t *info, void *raw_data) {
  init_roots_data_t *data = (init_roots_data_t*) raw_data;
  TRY_SET(*access_roots_entry_at(data->roots, info->root_key),
      new_heap_compact_species(data->runtime, info->behavior));
  return success();
}

static value_t roots_create_type(runtime_t *runtime, value_t roots,
    const char *camel_name, root_key_t type_key) {
  value_t display_name = new_heap_utf8(runtime, new_c_string(camel_name));
  TRY_SET(*access_roots_entry_at(roots, type_key), new_heap_type(runtime,
      afFreeze, display_name));
  return success();
}

static value_t roots_create_family_type_callback(family_info_t *info, void *raw_data) {
  init_roots_data_t *data = (init_roots_data_t*) raw_data;
  return roots_create_type(data->runtime, data->roots, info->camel_name, info->root_key);
}

static value_t roots_create_phylum_type_callback(phylum_info_t *info, void *raw_data) {
  init_roots_data_t *data = (init_roots_data_t*) raw_data;
  return roots_create_type(data->runtime, data->roots, info->camel_name, info->root_key);
}

value_t roots_init(value_t roots, const extended_runtime_config_t *config,
    runtime_t *runtime) {
  // The modal meta-roots are tricky because the species relationship between
  // them is circular.
  TRY_DEF(fluid_meta,
      new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior, vmFluid,
          rk_fluid_species_species));
  TRY_DEF(mutable_meta,
      new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior, vmMutable,
          rk_fluid_species_species));
  TRY_DEF(frozen_meta,
      new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior, vmFrozen,
          rk_fluid_species_species));
  TRY_DEF(deep_frozen_meta,
      new_heap_modal_species_unchecked(runtime, &kSpeciesBehavior, vmDeepFrozen,
          rk_fluid_species_species));
  set_heap_object_header(fluid_meta, mutable_meta);
  set_heap_object_header(mutable_meta, mutable_meta);
  set_heap_object_header(frozen_meta, mutable_meta);
  set_heap_object_header(deep_frozen_meta, mutable_meta);
  RAW_ROOT(roots, fluid_species_species) = fluid_meta;
  RAW_ROOT(roots, mutable_species_species) = mutable_meta;
  RAW_ROOT(roots, frozen_species_species) = frozen_meta;
  RAW_ROOT(roots, deep_frozen_species_species) = deep_frozen_meta;

  init_roots_data_t data = {roots, runtime};
  TRY(roots_for_each_modal_species(roots_create_modal_species_callback, &data));
  TRY(roots_for_each_compact_species(roots_create_compact_species_callback, &data));

  // At this point we'll have created the root species so we can set its header.
  CHECK_EQ("roots already initialized", vdInteger,
      get_value_domain(get_heap_object_header(roots)));
  set_heap_object_species(roots, RAW_ROOT(roots, mutable_roots_species));

  // Generates code for initializing a string table entry.
#define __CREATE_STRING_TABLE_ENTRY__(name, value)                             \
  do {                                                                         \
    TRY_SET(RAW_RSTR(roots, name), new_heap_utf8(runtime, new_c_string(value))); \
  } while (false);
  ENUM_STRING_TABLE(__CREATE_STRING_TABLE_ENTRY__)
#undef __CREATE_STRING_TABLE_ENTRY__

  // Ditto selector table entry.
#define __CREATE_SELECTOR_TABLE_ENTRY__(name, value)                           \
  do {                                                                         \
    TRY_DEF(string, new_heap_utf8(runtime, new_c_string(value)));            \
    TRY_SET(RAW_RSEL(roots, name), new_heap_operation(runtime, afFreeze,       \
        otInfix, string));                                                     \
  } while (false);
  ENUM_SELECTOR_TABLE(__CREATE_SELECTOR_TABLE_ENTRY__)
#undef __CREATE_SELECTOR_TABLE_ENTRY__

  // Initialize singletons first since we need those to create more complex
  // values below.
  TRY_DEF(empty_array, new_heap_array(runtime, 0));
  RAW_ROOT(roots, empty_array) = empty_array;
  TRY_DEF(array_of_zero, new_heap_array(runtime, 1));
  set_array_at(array_of_zero, 0, new_integer(0));
  RAW_ROOT(roots, array_of_zero) = array_of_zero;
  TRY_DEF(empty_blob, new_heap_blob(runtime, 0, afFreeze));
  TRY_SET(RAW_ROOT(roots, empty_code_block),
      new_heap_code_block(runtime, empty_blob, empty_array, 0));
  TRY_SET(RAW_ROOT(roots, empty_array_buffer),
      new_heap_array_buffer(runtime, 0));
  TRY_SET(RAW_ROOT(roots, empty_path),
      new_heap_path(runtime, afFreeze, nothing(), nothing()));
  TRY_SET(RAW_ROOT(roots, any_guard),
      new_heap_guard(runtime, afFreeze, gtAny, null()));
  TRY_DEF(empty_type, new_heap_type(runtime, afFreeze, null()));
  TRY(validate_deep_frozen(runtime, empty_type, NULL));
  TRY_SET(RAW_ROOT(roots, empty_instance_species),
      new_heap_instance_species(runtime, empty_type, nothing(), vmMutable));
  TRY_SET(RAW_ROOT(roots, subject_key),
      new_heap_key(runtime, RAW_RSTR(roots, subject)));
  TRY_SET(RAW_ROOT(roots, selector_key),
      new_heap_key(runtime, RAW_RSTR(roots, selector)));
  TRY_SET(RAW_ROOT(roots, transport_key),
      new_heap_key(runtime, RAW_RSTR(roots, transport)));
  TRY_SET(RAW_ROOT(roots, builtin_impls), new_heap_id_hash_map(runtime, 256));
  TRY_SET(RAW_ROOT(roots, op_call),
      new_heap_operation(runtime, afFreeze, otCall, null()));
  TRY_SET(RAW_ROOT(roots, stack_bottom_code_block),
      create_stack_bottom_code_block(runtime));
  TRY_SET(RAW_ROOT(roots, stack_piece_bottom_code_block),
      create_stack_piece_bottom_code_block(runtime));
  TRY_SET(RAW_ROOT(roots, return_code_block),
      create_return_code_block(runtime));
  TRY_SET(RAW_ROOT(roots, call_thunk_code_block),
      create_call_thunk_code_block(runtime));
  TRY_SET(RAW_ROOT(roots, escape_records), create_escape_records(runtime));
  TRY_SET(RAW_ROOT(roots, special_imports), new_heap_id_hash_map(runtime, 256));
  TRY_SET(RAW_ROOT(roots, subject_key_array),
      new_heap_array_with_contents(runtime, afFreeze, new_value_array(&RAW_ROOT(roots, subject_key), 1)));
  TRY_SET(RAW_ROOT(roots, selector_key_array),
      new_heap_array_with_contents(runtime, afFreeze, new_value_array(&RAW_ROOT(roots, selector_key), 1)));
  TRY_SET(RAW_ROOT(roots, transport_key_array),
      new_heap_array_with_contents(runtime, afFreeze, new_value_array(&RAW_ROOT(roots, transport_key), 1)));

  TRY(roots_for_each_family_type(roots_create_family_type_callback, &data));
  TRY(roots_create_type(runtime, roots, "Integer", rk_integer_type));

  TRY_DEF(builtin_methodspace, new_heap_methodspace(runtime, nothing()));
  RAW_ROOT(roots, builtin_methodspace) = builtin_methodspace;

  // The native methodspace may need some of the types above so only initialize
  // it after they have been created.
  TRY_SET(RAW_ROOT(roots, ctrino_factory),
      apply_ctrino_plugin(runtime, builtin_methodspace));
  TRY_SET(RAW_ROOT(roots, plugin_factories),
      apply_custom_plugins(runtime, config, builtin_methodspace));

  TRY(ensure_frozen(runtime, builtin_methodspace));

  TRY(roots_for_each_phylum_type(roots_create_phylum_type_callback, &data));

  TRY_DEF(plankton_factories, new_heap_id_hash_map(runtime, 16));
  init_plankton_core_factories(plankton_factories, runtime);
  init_plankton_syntax_factories(plankton_factories, runtime);
  RAW_ROOT(roots, plankton_factories) = plankton_factories;

  return success();
}

// Check that the condition holds, otherwise check fail with a validation error.
#define VALIDATE_CHECK_TRUE(EXPR)                                              \
COND_CHECK_TRUE("validation", ccValidationFailed, EXPR)

// Check that A and B are equal, otherwise check fail with a validation error.
#define VALIDATE_CHECK_EQ(A, B)                                                \
COND_CHECK_EQ("validation", ccValidationFailed, A, B)

// Checks whether the argument is within the specified family, otherwise
// signals a validation failure.
#define VALIDATE_HEAP_OBJECT(ofFamily, value) do {                             \
  VALIDATE_CHECK_TRUE(in_family(ofFamily, (value)));                           \
  TRY(heap_object_validate(value));                                            \
} while (false)

// Checks that the given value is a species with the specified instance
// family.
#define VALIDATE_SPECIES(ofFamily, value) do {                                 \
  VALIDATE_HEAP_OBJECT(ofSpecies, value);                                      \
  VALIDATE_CHECK_EQ(get_species_instance_family(value), ofFamily);             \
  TRY(heap_object_validate(value));                                            \
} while (false)

static value_t roots_validate_modal_species(value_t roots,
    heap_object_family_t family, value_mode_t mode, root_key_t key) {
  value_t species = get_roots_entry_at(roots, key);
  VALIDATE(get_species_division(species) == sdModal);
  VALIDATE(get_modal_species_mode(species) == mode);
  VALIDATE_SPECIES(family, species);
  return success();
}

// Data passed through when validating species.
typedef struct {
  value_t roots;
} validate_species_data_t;

static value_t roots_validate_modal_species_callback(family_info_t *info, void *data) {
  value_t roots = ((validate_species_data_t*) data)->roots;
  root_key_t fluid_key = info->root_key;
  TRY(roots_validate_modal_species(roots, info->family, vmFluid, fluid_key));
  root_key_t mutable_key = get_modal_key_from_fluid(fluid_key, vmMutable);
  TRY(roots_validate_modal_species(roots, info->family, vmMutable, mutable_key));
  root_key_t frozen_key = get_modal_key_from_fluid(fluid_key, vmFrozen);
  TRY(roots_validate_modal_species(roots, info->family, vmFrozen, frozen_key));
  root_key_t deep_frozen_key = get_modal_key_from_fluid(fluid_key, vmDeepFrozen);
  TRY(roots_validate_modal_species(roots, info->family, vmDeepFrozen, deep_frozen_key));
  return success();
}

static value_t roots_validate_surface_family_callback(family_info_t *info, void *data) {
  value_t roots = ((validate_species_data_t*) data)->roots;
  VALIDATE_HEAP_OBJECT(ofType, get_roots_entry_at(roots, info->root_key));
  return success();
}

static value_t roots_validate_surface_phylum_callback(phylum_info_t *info, void *data) {
  value_t roots = ((validate_species_data_t*) data)->roots;
  VALIDATE_HEAP_OBJECT(ofType, get_roots_entry_at(roots, info->root_key));
  return success();
}

// This function is split up into multiple parts to make compilation faster,
// it really strains the compilers otherwise.
value_t roots_validate(value_t roots) {
  validate_species_data_t data = {roots};
  TRY(roots_for_each_modal_species(roots_validate_modal_species_callback, &data));
  TRY(roots_for_each_family_type(roots_validate_surface_family_callback, &data));
  TRY(roots_for_each_phylum_type(roots_validate_surface_phylum_callback, &data));

  // Validate singletons manually.
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, empty_array));
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, array_of_zero));
  VALIDATE_CHECK_EQ(1, get_array_length(RAW_ROOT(roots, array_of_zero)));
  VALIDATE_HEAP_OBJECT(ofArrayBuffer, RAW_ROOT(roots, empty_array_buffer));
  VALIDATE_CHECK_EQ(0,
      get_array_buffer_length(RAW_ROOT(roots, empty_array_buffer)));
  VALIDATE_HEAP_OBJECT(ofPath, RAW_ROOT(roots, empty_path));
  VALIDATE_CHECK_TRUE(is_path_empty(RAW_ROOT(roots, empty_path)));
  VALIDATE_HEAP_OBJECT(ofGuard, RAW_ROOT(roots, any_guard));
  VALIDATE_CHECK_EQ(gtAny, get_guard_type(RAW_ROOT(roots, any_guard)));
  VALIDATE_HEAP_OBJECT(ofType, RAW_ROOT(roots, integer_type));
  VALIDATE_HEAP_OBJECT(ofSpecies, RAW_ROOT(roots, empty_instance_species));
  VALIDATE_HEAP_OBJECT(ofMethodspace, RAW_ROOT(roots, builtin_methodspace));
  VALIDATE_HEAP_OBJECT(ofKey, RAW_ROOT(roots, subject_key));
  VALIDATE_CHECK_EQ(kSubjectKeyId, get_key_id(RAW_ROOT(roots, subject_key)));
  VALIDATE_HEAP_OBJECT(ofKey, RAW_ROOT(roots, selector_key));
  VALIDATE_CHECK_EQ(kSelectorKeyId, get_key_id(RAW_ROOT(roots, selector_key)));
  VALIDATE_HEAP_OBJECT(ofKey, RAW_ROOT(roots, transport_key));
  VALIDATE_CHECK_EQ(kTransportKeyId, get_key_id(RAW_ROOT(roots, transport_key)));
  VALIDATE_HEAP_OBJECT(ofIdHashMap, RAW_ROOT(roots, builtin_impls));
  VALIDATE_HEAP_OBJECT(ofOperation, RAW_ROOT(roots, op_call));
  VALIDATE_CHECK_EQ(otCall, get_operation_type(RAW_ROOT(roots, op_call)));
  VALIDATE_HEAP_OBJECT(ofCodeBlock,
      RAW_ROOT(roots, stack_piece_bottom_code_block));
  VALIDATE_HEAP_OBJECT(ofCodeBlock, RAW_ROOT(roots, stack_bottom_code_block));
  VALIDATE_HEAP_OBJECT(ofCodeBlock, RAW_ROOT(roots, call_thunk_code_block));
  VALIDATE_HEAP_OBJECT(ofCodeBlock, RAW_ROOT(roots, return_code_block));
  VALIDATE_HEAP_OBJECT(ofCodeBlock, RAW_ROOT(roots, empty_code_block));
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, escape_records));
  VALIDATE_HEAP_OBJECT(ofIdHashMap, RAW_ROOT(roots, special_imports));
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, plugin_factories));

  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, subject_key_array));
  VALIDATE_CHECK_EQ(1, get_array_length(RAW_ROOT(roots, subject_key_array)));
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, selector_key_array));
  VALIDATE_CHECK_EQ(1, get_array_length(RAW_ROOT(roots, selector_key_array)));
  VALIDATE_HEAP_OBJECT(ofArray, RAW_ROOT(roots, transport_key_array));
  VALIDATE_CHECK_EQ(1, get_array_length(RAW_ROOT(roots, transport_key_array)));

#define __VALIDATE_STRING_TABLE_ENTRY__(name, value) VALIDATE_HEAP_OBJECT(ofUtf8, RAW_RSTR(roots, name));
  ENUM_STRING_TABLE(__VALIDATE_STRING_TABLE_ENTRY__)
#undef __VALIDATE_STRING_TABLE_ENTRY__

#define __VALIDATE_SELECTOR_TABLE_ENTRY__(name, value) VALIDATE_HEAP_OBJECT(ofOperation, RAW_RSEL(roots, name));
  ENUM_SELECTOR_TABLE(__VALIDATE_SELECTOR_TABLE_ENTRY__)
#undef __VALIDATE_SELECTOR_TABLE_ENTRY__

#undef VALIDATE_TYPE
#undef VALIDATE_SPECIES
  return success();
}

value_t ensure_roots_owned_values_frozen(runtime_t *runtime, value_t self) {
  // Freeze *all* the things!
  value_field_iter_t iter;
  value_field_iter_init(&iter, self);
  value_field_t field = value_field_empty();
  while (value_field_iter_next(&iter, &field))
    TRY(ensure_frozen(runtime, *field.ptr));
  return success();
}

// --- M u t a b l e   r o o t s ---

TRIVIAL_PRINT_ON_IMPL(MutableRoots, mutable_roots);

value_t mutable_roots_validate(value_t self) {
  VALIDATE_FAMILY(ofMutableRoots, self);
  VALIDATE_HEAP_OBJECT(ofArgumentMapTrie,
      RAW_MROOT(self, argument_map_trie_root));
  return success();
}

value_t ensure_mutable_roots_owned_values_frozen(runtime_t *runtime,
    value_t self) {
  // Why would you freeze the mutable roots -- they're supposed to be mutable!
  UNREACHABLE("freezing the mutable roots");
  return new_condition(ccWat);
}

void gc_fuzzer_init(gc_fuzzer_t *fuzzer, uint32_t min_freq, uint32_t mean_freq,
    uint32_t seed) {
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
    fuzzer->remaining = pseudo_random_next(&fuzzer->random, fuzzer->spread)
        + fuzzer->min_freq;
    return true;
  } else {
    fuzzer->remaining--;
    return false;
  }
}

value_t new_runtime(extended_runtime_config_t *config, runtime_t **runtime_out) {
  runtime_t *runtime = allocator_default_malloc_struct(runtime_t);
  value_t result = runtime_init(runtime, config);
  if (is_condition(result)) {
    // Something went wrong initializing the runtime so watch out when disposing
    // it.
    delete_runtime(runtime, dfMayBeInvalid);
    *runtime_out = NULL;
  } else {
    *runtime_out = runtime;
  }
  return result;
}

value_t delete_runtime(runtime_t *runtime, uint32_t flags) {
  if (runtime == NULL)
    // Because new_runtime may yield a NULL it's simpler to allow it here too
    // so you don't need to test explicitly for that possibility, you can just
    // delete in any case.
    return success();
  TRY(runtime_dispose(runtime, flags));
  allocator_default_free_struct(runtime_t, runtime);
  return success();
}

// The least number of allocations between forced allocation failures.
static const uint32_t kGcFuzzerMinFrequency = 64;

// Perform "hard" initialization, the stuff where the runtime isn't fully
// consistent yet.
static value_t runtime_hard_init(runtime_t *runtime,
    const extended_runtime_config_t *config) {
  // Initialize the heap and roots. After this the runtime is sort-of ready to
  // be used.
  TRY(heap_init(&runtime->heap, config));
  TRY_SET(runtime->roots, new_heap_uninitialized_roots(runtime));
  TRY(roots_init(runtime->roots, config, runtime));
  TRY_SET(runtime->mutable_roots, new_heap_mutable_roots(runtime));
  // Check that everything looks sane.
  return runtime_validate(runtime, nothing());
}

// Perform "soft" initialization, the stuff where we're starting to rely on the
// runtime being fully functional.
static value_t runtime_soft_init(runtime_t *runtime,
    const extended_runtime_config_t *config) {
  TRY_DEF(module_loader, new_heap_empty_module_loader(runtime));
  runtime->s_module_loader = runtime_protect_value(runtime, module_loader);
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  TRY_FINALLY {
    safe_value_t s_builtin_impls = protect(pool, ROOT(runtime, builtin_impls));
    E_TRY(add_builtin_implementations(runtime, s_builtin_impls));
    E_TRY(init_special_imports(runtime, ROOT(runtime, special_imports)));
    E_TRY(init_services(runtime, ROOT(runtime, special_imports), config));
    E_RETURN(runtime_validate(runtime, nothing()));
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

// Freeze the runtime such that any state that can be shared is deep frozen.
static value_t runtime_freeze_shared_state(runtime_t *runtime) {
  value_t roots = runtime->roots;

  // The roots object must be deep frozen.
  TRY(ensure_frozen(runtime, roots));

  value_t offender = whatever();
  if (!try_validate_deep_frozen(runtime, roots, &offender)) {
    ERROR("Could not freeze the roots object; offender: %v", offender);
    return new_not_deep_frozen_condition();
  }

  return success();
}

value_t runtime_init(runtime_t *runtime,
    const extended_runtime_config_t *config) {
  CHECK_TRUE("no default lifetime", lifetime_get_default() != NULL);
  runtime_ensure_static_inits_run();
  ensure_neutrino_format_handlers_registered();
  if (config == NULL)
    config = extended_runtime_config_get_default();
  // First reset all the fields to a well-defined value.
  runtime_clear(runtime);
  event_sequence_init(&runtime->debug_events, 256);
  // Set up the file system pointer. Null means default to the native system.
  runtime->file_system = config->base.file_system;
  if (runtime->file_system == NULL)
    runtime->file_system = file_system_native();
  runtime->system_time = config->base.system_time;
  if (runtime->system_time == NULL)
    runtime->system_time = real_time_clock_system();
  runtime->random = tinymt64_construct(tinymt64_params_default(),
      config->base.random_seed);
  TRY(runtime_hard_init(runtime, config));
  TRY(runtime_soft_init(runtime, config));
  TRY(runtime_freeze_shared_state(runtime));
  TRY(runtime_validate(runtime, nothing()));
  // Set up gc fuzzing. For now do this after the initialization to exempt that
  // from being fuzzed. Longer term (probably after this has been rewritten) we
  // want more of this to be gc safe.
  if (config->base.gc_fuzz_freq > 0) {
    runtime->gc_fuzzer = allocator_default_malloc_struct(gc_fuzzer_t);
    gc_fuzzer_init(runtime->gc_fuzzer, kGcFuzzerMinFrequency,
        config->base.gc_fuzz_freq, config->base.gc_fuzz_seed);
  }
  return success();
}

runtime_observer_t runtime_observer_empty() {
  runtime_observer_t result;
  memset(&result, 0, sizeof(runtime_observer_t));
  return result;
}

void runtime_push_observer(runtime_t *runtime, runtime_observer_t *observer) {
  CHECK_TRUE("observer already registered", observer->prev == NULL);
  observer->prev = runtime->top_observer;
  runtime->top_observer = observer;
}

void runtime_pop_observer(runtime_t *runtime, runtime_observer_t *observer) {
  CHECK_PTREQ("not top observer", runtime->top_observer, observer);
  runtime->top_observer = observer->prev;
}

IMPLEMENTATION(value_validator_o, value_visitor_o);

// Adaptor function for passing object validate as a value visitor.
static value_t value_validator_visit(value_visitor_o *self, value_t value) {
  switch (get_value_domain(value)) {
  case vdHeapObject:
  case vdDerivedObject:
    return value_validate(value);
  default:
    CHECK_TRUE("validating non-object", is_nothing(value));
    return success();
  }
}

VTABLE(value_validator_o, value_visitor_o) { value_validator_visit };

value_t runtime_validate(runtime_t *runtime, value_t cause) {
  TRY(heap_validate(&runtime->heap));
  value_visitor_o visitor;
  VTABLE_INIT(value_validator_o, &visitor);
  TRY(heap_for_each_object(&runtime->heap, &visitor));
  return success();
}

// A record of an object that needs to be fixed up post-migration.
typedef struct {
  // The new object that we're migrating to. All fields have already been
  // migrated and the object will be fully functional at the time of the fixup.
  value_t new_heap_object;
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
  blob_t memory;
} pending_fixup_worklist_t;

// Initialize a worklist. This doesn't allocate any storage, that only happens
// when elements are added to the list.
static void pending_fixup_worklist_init(pending_fixup_worklist_t *worklist) {
  worklist->capacity = 0;
  worklist->length = 0;
  worklist->fixups = NULL;
  worklist->memory = blob_empty();
}

// Add a new fixup to the list. This returns a value such that it can return
// a condition if the system runs out of memory.
static value_t pending_fixup_worklist_add(pending_fixup_worklist_t *worklist,
    pending_fixup_t *fixup) {
  if (worklist->capacity == worklist->length) {
    // There's no room for the new element so increase capacity.
    size_t old_capacity = worklist->capacity;
    size_t new_capacity = (old_capacity == 0) ? 16 : 2 * old_capacity;
    blob_t new_memory = allocator_default_malloc(
        new_capacity * sizeof(pending_fixup_t));
    pending_fixup_t *new_fixups = (pending_fixup_t*) new_memory.start;
    if (old_capacity > 0) {
      // Copy the previous data over to the new backing store.
      memcpy(new_fixups, worklist->fixups,
          worklist->length * sizeof(pending_fixup_t));
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
  if (!blob_is_empty(worklist->memory)) {
    allocator_default_free(worklist->memory);
    worklist->memory = blob_empty();
    worklist->fixups = NULL;
  }
}

IMPLEMENTATION(garbage_collection_state_o, field_visitor_o);

// State maintained during garbage collection. Also functions as a field visitor
// such that it's easy to traverse objects.
struct garbage_collection_state_o {
  IMPLEMENTATION_HEADER(garbage_collection_state_o, field_visitor_o)
  ;
  // The runtime we're collecting.
  runtime_t *runtime;
  // List of objects to post-process after migration.
  pending_fixup_worklist_t pending_fixups;
};

// Initializes a garbage collection state object.
static garbage_collection_state_o garbage_collection_state_new(
    runtime_t *runtime) {
  garbage_collection_state_o result;
  result.runtime = runtime;
  VTABLE_INIT(garbage_collection_state_o, UPCAST(&result));
  pending_fixup_worklist_init(&result.pending_fixups);
  return result;
}

// Disposes a garbage collection state object.
static void garbage_collection_state_dispose(garbage_collection_state_o *self) {
  pending_fixup_worklist_dispose(&self->pending_fixups);
}

static value_t migrate_object_shallow(value_t object, space_t *space) {
  // Ask the object to describe its layout.
  heap_object_layout_t layout;
  get_heap_object_layout(object, &layout);
  // Allocate new room for the object.
  address_t source = get_heap_object_address(object);
  address_t target = NULL;
  bool alloc_succeeded = space_try_alloc(space, layout.size, &target);
  CHECK_TRUE("clone alloc failed", alloc_succeeded);
  // Do a raw copy of the object to the target.
  memcpy(target, source, layout.size);
  // Tag the new location as an object and return it.
  return new_heap_object(target);
}

// Returns true if the given object needs to apply a fixup after migration.
static bool needs_post_migrate_fixup(value_t old_object) {
  return get_heap_object_family_behavior_unchecked(old_object)->post_migrate_fixup
      != NULL;
}

/// ## Field migration

// Records that a given child value is kept alive by the given parent. Both the
// parent and child are assumed to be in to-space.
static void record_backpointer(heap_t *heap, value_t parent, value_t child) {
  CHECK_FALSE("no backspace", blob_is_empty(heap->backpointer_space));
  space_t to_space = heap->to_space;
  address_t child_addr = get_heap_object_address(child);
  CHECK_REL("outside to space", (address_arith_t) child_addr, <, (address_arith_t) to_space.limit);
  CHECK_REL("outside to space", (address_arith_t) to_space.start, <=, (address_arith_t) child_addr);
  size_t offset = child_addr - to_space.start;
  address_t backpointer = ((address_t) heap->backpointer_space.start) + offset;
  *((value_t*) backpointer) = parent;
}

// Ensures that the given object has a clone in to-space, returning a pointer to
// it. If there is no pre-existing clone a shallow one will be created.
static value_t ensure_heap_object_migrated(garbage_collection_state_o *self,
    value_t parent, value_t old_object) {
  // Check if this object has already been moved.
  value_t old_header = get_heap_object_header(old_object);
  if (get_value_domain(old_header) == vdMovedObject) {
    // This object has already been moved and the header points to the new
    // location so we just get out the location of the migrated object and update
    // the field.
    return get_moved_object_target(old_header);
  } else {
    // The header indicates that this object hasn't been moved yet. First make
    // a raw clone of the object in to-space.
    CHECK_DOMAIN(vdHeapObject, old_header);
    // Check with the object whether it needs post processing. This is the last
    // time the object is intact so it's the last point we can call methods on
    // it to find out.
    CHECK_TRUE("migrating clone",
        space_contains(&self->runtime->heap.from_space, get_heap_object_address(old_object)));
    bool needs_fixup = needs_post_migrate_fixup(old_object);
    value_t new_object = migrate_object_shallow(old_object,
        &self->runtime->heap.to_space);
    CHECK_DOMAIN(vdHeapObject, new_object);
    if (!blob_is_empty(self->runtime->heap.backpointer_space))
      record_backpointer(&self->runtime->heap, parent, new_object);
    // Now that we know where the new object is going to be we can schedule the
    // fixup if necessary.
    if (needs_fixup) {
      pending_fixup_t fixup = { new_object, old_object };
      TRY(pending_fixup_worklist_add(&self->pending_fixups, &fixup));
    }
    // Point the old object to the new one so we know to use the new clone
    // instead of ever cloning it again.
    value_t forward_pointer = new_moved_object(new_object);
    set_heap_object_header(old_object, forward_pointer);
    // At this point the cloned object still needs some work to update the
    // fields but we rely on traversing the heap to do that eventually.
    return new_object;
  }
}

// Returns a new derived object pointer identical to the given one except that
// it points to the new clone of the host rather than the old one.
static value_t migrate_derived_object(garbage_collection_state_o *self,
    value_t parent, value_t old_derived) {
  // Ensure that the host has been migrated.
  value_t old_host = get_derived_object_host(old_derived);
  value_t new_host = ensure_heap_object_migrated(self, parent, old_host);
  // Calculate the new address derived from the new host.
  value_t anchor = get_derived_object_anchor(old_derived);
  size_t host_offset = (size_t) get_derived_object_anchor_host_offset(anchor);
  address_t new_addr = get_heap_object_address(new_host) + host_offset;
  return new_derived_object(new_addr);
}

// Callback that migrates an object from from to to space, if it hasn't been
// migrated already.
static value_t migrate_field_shallow(field_visitor_o *super_self,
    value_field_t field) {
  garbage_collection_state_o *self = DOWNCAST(garbage_collection_state_o,
      super_self);
  value_t old_value = *field.ptr;
  // If this is not a heap object there's nothing to do.
  value_domain_t domain = get_value_domain(old_value);
  if (domain == vdHeapObject) {
    TRY_SET(*field.ptr, ensure_heap_object_migrated(self, field.parent, old_value));
  } else if (domain == vdDerivedObject) {
    TRY_SET(*field.ptr, migrate_derived_object(self, field.parent, old_value));
  }
  return success();
}

VTABLE(garbage_collection_state_o, field_visitor_o) { migrate_field_shallow };

// Applies a post-migration fixup scheduled when migrating the given object.
static void apply_fixup(runtime_t *runtime, value_t new_heap_object,
    value_t old_object) {
  family_behavior_t *behavior = get_heap_object_family_behavior_unchecked(
      new_heap_object);
  (behavior->post_migrate_fixup)(runtime, new_heap_object, old_object);
}

// Perform any fixups that have been scheduled during object migration.
static void runtime_apply_fixups(garbage_collection_state_o *self) {
  pending_fixup_worklist_t *worklist = &self->pending_fixups;
  for (size_t i = 0; i < worklist->length; i++) {
    pending_fixup_t *fixup = &worklist->fixups[i];
    apply_fixup(self->runtime, fixup->new_heap_object, fixup->old_object);
  }
}

// Works the opposite way from offsetof: yields a member based on a base pointer
// and an offset. So memberof(M, o, offsetof(T, m)) should be equivalent to
// o->m assuming the type of o is T and the type of o->m is M.
#define memberof(TYPE, BASE, OFFSET) (*((TYPE*) (((byte_t*) (BASE)) + (OFFSET))))

static void runtime_notify_observers(runtime_t *runtime, size_t field_offset) {
  for (runtime_observer_t *obs = runtime->top_observer; obs != NULL; obs = obs->prev) {
    unary_callback_t *callback = memberof(unary_callback_t*, obs, field_offset);
    unary_callback_call(callback, p2o(runtime));
  }
}

value_t runtime_garbage_collect(runtime_t *runtime) {
  // Validate that everything's healthy before we start.
  TRY(runtime_validate(runtime, nothing()));
  // Create to-space and swap it in, making the current to-space into from-space.
  TRY(heap_prepare_garbage_collection(&runtime->heap));
  // Initialize the state we'll maintain during collection.
  garbage_collection_state_o state = garbage_collection_state_new(runtime);
  field_visitor_o *visitor = UPCAST(&state);
  // Shallow migration of all the roots.
  TRY(field_visitor_visit(visitor, value_field_new(nothing(), &runtime->roots)));
  TRY(field_visitor_visit(visitor, value_field_new(nothing(), &runtime->mutable_roots)));
  // Shallow migration of everything currently stored in non-weak references in
  // to-space which, since we keep going until all objects have been migrated,
  // effectively makes a deep migration.
  TRY(heap_for_each_field(&runtime->heap, visitor, false));
  // Update the state of the heap's object trackers.
  TRY(heap_post_process_object_trackers(&runtime->heap));
  // At this point everything has been migrated so we can run the fixups and
  // then we're done with the state.
  runtime_apply_fixups(&state);
  garbage_collection_state_dispose(&state);
  // Now everything has been migrated so we can throw away from-space.
  TRY(heap_complete_garbage_collection(&runtime->heap));
  // Validate that everything's still healthy.
  TRY(runtime_validate(runtime, nothing()));
  // Notify any observers.
  runtime_notify_observers(runtime, offsetof(runtime_observer_t, on_gc_done));
  return success();
}

void runtime_clear(runtime_t *runtime) {
  runtime->next_key_index = 0;
  runtime->gc_fuzzer = NULL;
  runtime->roots = whatever();
  runtime->mutable_roots = whatever();
  runtime->s_module_loader = empty_safe_value();
  runtime->top_observer = NULL;
  runtime->io_engine = NULL;
  runtime->next_job_serial = 0;
}

// Perform any pre-processing we need to do before releasing the runtime.
static value_t runtime_prepare_dispose(runtime_t *runtime, uint32_t flags) {
  bool require_collection = heap_collect_before_dispose(&runtime->heap);
  if ((flags & dfMayBeInvalid) != 0) {
    // If the runtime may be invalid we can't very well run a gc so we just skip
    // that, which may cause problems if a collection is required before
    // disposal.
    if (require_collection)
      WARN("Disposing possibly invalid runtime without running required collection");
    return success();
  } else if (require_collection) {
    return runtime_garbage_collect(runtime);
  } else {
    return runtime_validate(runtime, nothing());
  }
}

value_t runtime_dispose(runtime_t *runtime, uint32_t flags) {
  // Destroying the io engine requires it to wrap the currently executing iops
  // up nicely which requires the heap to be intact, so we do that as the very
  // first thing.
  if (runtime->io_engine != NULL) {
    io_engine_destroy(runtime->io_engine);
    runtime->io_engine = NULL;
  }
  value_t result = runtime_prepare_dispose(runtime, flags);
  // If preparing fails we keep going and try to free the allocated memory.
  // This may be a bad idea but until there's some evidence one way or the other
  // let's do it this way.
  safe_value_destroy(runtime, runtime->s_module_loader);
  result = condition_and(result, heap_dispose(&runtime->heap));
  if (runtime->gc_fuzzer != NULL) {
    allocator_default_free_struct(gc_fuzzer_t, runtime->gc_fuzzer);
    runtime->gc_fuzzer = NULL;
  }
  if (!event_sequence_is_empty(&runtime->debug_events)) {
    event_sequence_dump(&runtime->debug_events,
        file_system_stderr(runtime->file_system));
  }
  event_sequence_dispose(&runtime->debug_events);
  return result;
}

void runtime_record_debug_event(runtime_t *runtime, const char *tag, void *payload) {
  event_sequence_record(&runtime->debug_events, tag, payload);
}

bool value_is_immediate(value_t value) {
  switch (get_value_domain(value)) {
  case vdHeapObject:
  case vdDerivedObject:
    return false;
  default:
    return true;
  }
}

safe_value_t runtime_protect_value_with_flags(runtime_t *runtime, value_t value,
    uint32_t safe_flags, protect_value_data_t *data) {
  if (value_is_immediate(value)) {
    return protect_immediate(value);
  } else {
    object_tracker_t *gc_safe = heap_new_heap_object_tracker(&runtime->heap,
        value, safe_flags, data);
    return object_tracker_to_safe_value(gc_safe);
  }

}

safe_value_t runtime_protect_value(runtime_t *runtime, value_t value) {
  return runtime_protect_value_with_flags(runtime, value, tfNone, NULL);
}

static value_t new_object_instance(object_factory_t *factory,
    runtime_t *runtime, value_t header) {
  return new_heap_object_with_type(runtime, header);
}

static value_t set_object_instance_fields(object_factory_t *factory,
    runtime_t *runtime, value_t header, value_t object, value_t fields) {
  return set_heap_object_contents(runtime, object, header, fields);
}

object_factory_t runtime_default_object_factory() {
  return new_object_factory(new_object_instance, set_object_instance_fields,
      NULL);
}


value_t safe_runtime_plankton_deserialize_blob(runtime_t *runtime, safe_value_t blob) {
  object_factory_t factory = runtime_default_object_factory();
  RETRY_ONCE_IMPL(runtime, plankton_deserialize_blob(runtime, &factory, blob));
}

void safe_value_destroy(runtime_t *runtime, safe_value_t s_value) {
  if (!safe_value_is_immediate(s_value)) {
    object_tracker_t *gc_safe = safe_value_to_object_tracker(s_value);
    heap_destroy_object_tracker(&runtime->heap, gc_safe);
  }
}

bool runtime_toggle_fuzzing(runtime_t *runtime, bool enable) {
  gc_fuzzer_t *fuzzer = runtime->gc_fuzzer;
  if (fuzzer == NULL)
    return false;
  bool result = fuzzer->is_enabled;
  fuzzer->is_enabled = enable;
  return result;
}

value_t runtime_prepare_retry_after_heap_exhausted(runtime_t *runtime,
    value_t condition) {
  TRY(runtime_garbage_collect(runtime));
  return new_boolean(runtime_toggle_fuzzing(runtime, false));
}

void runtime_complete_retry_after_heap_exhausted(runtime_t *runtime, value_t recall) {
  runtime_toggle_fuzzing(runtime, get_boolean_value(recall));
}

value_t get_modal_species_sibling_with_mode(runtime_t *runtime, value_t species,
    value_mode_t mode) {
  CHECK_DIVISION(sdModal, species);
  root_key_t base_root = (root_key_t) get_modal_species_base_root(species);
  root_key_t mode_root = get_modal_key_from_fluid(base_root, mode);
  value_t result = get_roots_entry_at(runtime->roots, mode_root);
  CHECK_EQ("incorrect sibling mode", mode, get_modal_species_mode(result));
  CHECK_EQ("incorrect sibling family", get_species_instance_family(species),
      get_species_instance_family(result));
  return result;
}

value_t runtime_get_builtin_implementation(runtime_t *runtime, value_t name) {
  value_t builtins = ROOT(runtime, builtin_impls);
  value_t impl = get_id_hash_map_at(builtins, name);
  if (in_condition_cause(ccNotFound, impl)) {
    WARN("Unknown builtin %v", name);
    return new_unknown_builtin_condition();
  } else {
    return impl;
  }
}

value_t get_runtime_plugin_factory_at(runtime_t *runtime, size_t index) {
  return get_array_at(ROOT(runtime, plugin_factories), index);
}

io_engine_t *runtime_get_io_engine(runtime_t *runtime) {
  if (runtime->io_engine == NULL)
    runtime->io_engine = io_engine_new();
  return runtime->io_engine;
}

io_engine_t *runtime_peek_io_engine(runtime_t *runtime) {
  return runtime->io_engine;
}

value_t runtime_load_library_from_stream(runtime_t *runtime,
    in_stream_t *stream, value_t display_name) {
  CREATE_SAFE_VALUE_POOL(runtime, 2, pool);
  TRY_FINALLY {
    E_S_TRY_DEF(s_data, protect(pool, read_stream_to_blob(runtime, stream)));
    E_S_TRY_DEF(s_library, protect(pool, safe_runtime_plankton_deserialize_blob(
        runtime, s_data)));
    if (!in_family(ofLibrary, deref(s_library)))
      return new_invalid_input_condition();
    set_library_display_name(deref(s_library), display_name);
    // Load all the modules from the library into this module loader.
    value_t loader = deref(runtime->s_module_loader);
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, get_library_modules(deref(s_library)));
    while (id_hash_map_iter_advance(&iter)) {
      value_t key;
      value_t value;
      id_hash_map_iter_get_current(&iter, &key, &value);
      E_TRY(set_id_hash_map_at(runtime, get_module_loader_modules(loader), key,
          value));
    }
    E_RETURN(success());
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

// Assemble a module that can be executed from unbound modules in the module
// loader.
static value_t assemble_module(value_t ambience, value_t unbound_module) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  runtime_t *runtime = get_ambience_runtime(ambience);
  TRY_DEF(module, build_bound_module(ambience, unbound_module));
  value_t methodspace = get_ambience_methodspace(ambience);
  TRY(ensure_frozen(runtime, methodspace));
  return get_module_fragment_at(module, present_stage());
}

value_t safe_runtime_execute_syntax(runtime_t *runtime, safe_value_t s_program) {
  CHECK_FAMILY(ofProgramAst, deref(s_program));
  TRY_DEF(ambience, new_heap_ambience(runtime));
  CREATE_SAFE_VALUE_POOL(runtime, 4, pool);
  safe_value_t s_ambience = protect(pool, ambience);
  // Forward declare these to avoid msvc complaining.
  safe_value_t s_module, s_entry_point;
  TRY_FINALLY {
    value_t unbound_module = get_program_ast_module(deref(s_program));
    E_TRY_DEF(module, assemble_module(deref(s_ambience), unbound_module));
    s_module = protect(pool, module);
    s_entry_point = protect(pool, get_program_ast_entry_point(deref(s_program)));
    E_TRY_DEF(code_block,
        safe_compile_expression(runtime, s_entry_point, s_module,
            scope_get_bottom()));
    E_RETURN(run_code_block(s_ambience, protect(pool, code_block)));
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

// Have the modules we depend on had their static initializers run?
static bool has_run_static_inits = false;

static void runtime_run_static_inits() {
  // The way this is called should ensure that it is never called more than
  // once.
  CHECK_FALSE("static initialization already run", has_run_static_inits);
  has_run_static_inits = true;
  run_plugin_static_init();
}

// The once-stuff is tricky to factor into a platform-independent api, plus
// it's not really something that should be used elsewhere, so it's just
// inlined here.
#ifdef IS_MSVC
// Include this as late as possible.
#  include "c/winhdr.h"
#  define callback_marker CALLBACK
#  define init_once_t INIT_ONCE
#  define kInitOnceInit INIT_ONCE_STATIC_INIT
#else
#  define callback_marker
#  define init_once_t pthread_once_t
#  define bool_t bool
#  define kInitOnceInit PTHREAD_ONCE_INIT
#endif

// Trampoline used to adapt runtime_run_static_inits to the windows InitOnce
// api.
static bool_t callback_marker runtime_run_static_inits_msvc(
    init_once_t* init_once, void* param, void** context) {
  runtime_run_static_inits();
  return true;
}

void runtime_ensure_static_inits_run() {
  static init_once_t once = kInitOnceInit;
#ifdef IS_MSVC
  InitOnceExecuteOnce(&once, runtime_run_static_inits_msvc, NULL, NULL);
#else
  // Include a dummy usage to avoid unused-function errors.
  USE(runtime_run_static_inits_msvc(NULL, NULL, NULL));
  pthread_once(&once, runtime_run_static_inits);
#endif
}
