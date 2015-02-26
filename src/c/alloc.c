//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "ctrino.h"
#include "derived.h"
#include "freeze.h"
#include "process.h"
#include "sync.h"
#include "tagged.h"
#include "try-inl.h"
#include "value-inl.h"
#include "utils/string-inl.h"


// --- B a s i c ---

// Run a couple of sanity checks before returning the value from a constructor.
// Returns a condition if the check fails, otherwise returns the given value.
static value_t post_create_sanity_check(value_t value, size_t size) {
  TRY(heap_object_validate(value));
  heap_object_layout_t layout;
  heap_object_layout_init(&layout);
  get_heap_object_layout(value, &layout);
  COND_CHECK_EQ("post create sanity", ccValidationFailed, layout.size, size);
  return value;
}

// Post-processes an allocation result appropriately based on the given set of
// allocation flags.
static value_t post_process_result(runtime_t *runtime, value_t result,
    alloc_flags_t flags) {
  if (flags == afFreeze)
    TRY(ensure_frozen(runtime, result));
  return success();
}

value_t new_heap_uninitialized_roots(runtime_t *runtime) {
  size_t size = kRootsSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, whatever()));
  for (size_t i = 0; i < kRootCount; i++)
    *access_heap_object_field(result, HEAP_OBJECT_FIELD_OFFSET(i)) = whatever();
  return result;
}

value_t new_heap_mutable_roots(runtime_t *runtime) {
  TRY_DEF(argument_map_trie_root, new_heap_argument_map_trie(runtime,
      ROOT(runtime, empty_array)));
  size_t size = kMutableRootsSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_mutable_roots_species)));
  RAW_MROOT(result, argument_map_trie_root) = argument_map_trie_root;
  return result;
}

value_t new_heap_utf8(runtime_t *runtime, utf8_t contents) {
  size_t size = calc_utf8_size(string_size(contents));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, utf8_species)));
  set_utf8_length(result, string_size(contents));
  string_copy_to(contents, get_utf8_chars(result), string_size(contents) + 1);
  return post_create_sanity_check(result, size);
}

value_t new_heap_ascii_string_view(runtime_t *runtime, value_t value) {
  size_t size = kAsciiStringViewSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, ascii_string_view_species)));
  init_frozen_ascii_string_view_value(result, value);
  return post_create_sanity_check(result, size);

}

value_t new_heap_blob(runtime_t *runtime, size_t length) {
  size_t size = calc_blob_size(length);
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, blob_species)));
  set_blob_length(result, length);
  blob_t data = get_blob_data(result);
  blob_fill(data, 0);
  return post_create_sanity_check(result, size);
}

value_t new_heap_blob_with_data(runtime_t *runtime, blob_t contents) {
  // Allocate the blob object to hold the data.
  TRY_DEF(blob, new_heap_blob(runtime, blob_byte_length(contents)));
  // Pull out the contents of the heap blob.
  blob_t blob_data = get_blob_data(blob);
  // Copy the contents into the heap blob.
  blob_copy_to(contents, blob_data);
  return blob;
}

value_t new_heap_instance_species(runtime_t *runtime, value_t primary,
    value_t manager, value_mode_t mode) {
  size_t size = kInstanceSpeciesSize;
  CHECK_FAMILY(ofType, primary);
  CHECK_FAMILY_OPT(ofInstanceManager, manager);
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_species_species)));
  set_species_instance_family(result, ofInstance);
  set_species_family_behavior(result, &kInstanceBehavior);
  set_species_division_behavior(result, &kInstanceSpeciesBehavior);
  set_instance_species_primary_type_field(result, primary);
  set_instance_species_manager(result, manager);
  set_instance_species_raw_mode(result, new_integer(mode));
  set_instance_species_derivatives(result, nothing());
  return post_create_sanity_check(result, size);
}

value_t clone_heap_instance_species(runtime_t *runtime, value_t original) {
  CHECK_DIVISION(sdInstance, original);
  return clone_heap_object(runtime, original);
}

value_t new_heap_compact_species(runtime_t *runtime, family_behavior_t *behavior) {
  size_t bytes = kCompactSpeciesSize;
  TRY_DEF(result, alloc_heap_object(runtime, bytes,
      ROOT(runtime, mutable_species_species)));
  set_species_instance_family(result, behavior->family);
  set_species_family_behavior(result, behavior);
  set_species_division_behavior(result, &kCompactSpeciesBehavior);
  return post_create_sanity_check(result, bytes);
}

value_t new_heap_modal_species_unchecked(runtime_t *runtime,
    family_behavior_t *behavior, value_mode_t mode, root_key_t base_root) {
  size_t size = kModalSpeciesSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_species_species)));
  set_species_instance_family(result, behavior->family);
  set_species_family_behavior(result, behavior);
  set_species_division_behavior(result, &kModalSpeciesBehavior);
  set_modal_species_mode(result, mode);
  set_modal_species_base_root(result, base_root);
  return result;
}

value_t new_heap_modal_species(runtime_t *runtime, family_behavior_t *behavior,
    value_mode_t mode, root_key_t base_root) {
  TRY_DEF(result, new_heap_modal_species_unchecked(runtime, behavior, mode, base_root));
  return post_create_sanity_check(result, kModalSpeciesSize);
}

value_t new_heap_array(runtime_t *runtime, size_t length) {
  size_t size = calc_array_size(length);
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_array_species)));
  set_array_length(result, length);
  for (size_t i = 0; i < length; i++)
    set_array_at(result, i, null());
  return post_create_sanity_check(result, size);
}

value_t new_heap_array_with_contents(runtime_t *runtime, alloc_flags_t flags,
    value_array_t contents) {
  TRY_DEF(result, new_heap_array(runtime, contents.length));
  for (size_t i = 0; i < contents.length; i++)
    set_array_at(result, i, contents.start[i]);
  TRY(post_process_result(runtime, result, flags));
  return result;
}

value_t new_heap_reference(runtime_t *runtime, value_t value) {
  size_t size = kReferenceSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_reference_species)));
  set_reference_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_pair(runtime_t *runtime, value_t e0, value_t e1) {
  TRY_DEF(result, new_heap_array(runtime, 2));
  set_array_at(result, 0, e0);
  set_array_at(result, 1, e1);
  TRY(ensure_frozen(runtime, result));
  return result;
}

value_t new_heap_triple(runtime_t *runtime, value_t e0, value_t e1,
    value_t e2) {
  TRY_DEF(result, new_heap_array(runtime, 3));
  set_array_at(result, 0, e0);
  set_array_at(result, 1, e1);
  set_array_at(result, 2, e2);
  TRY(ensure_frozen(runtime, result));
  return result;
}

value_t new_heap_pair_array(runtime_t *runtime, size_t length) {
  return new_heap_array(runtime, length << 1);
}

value_t new_heap_array_buffer(runtime_t *runtime, size_t initial_capacity) {
  size_t size = kArrayBufferSize;
  TRY_DEF(elements, new_heap_array(runtime, initial_capacity));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_array_buffer_species)));
  set_array_buffer_elements(result, elements);
  set_array_buffer_length(result, 0);
  return post_create_sanity_check(result, size);
}

value_t new_heap_fifo_buffer(runtime_t *runtime, size_t width,
    size_t initial_capacity) {
  size_t size = kFifoBufferSize;
  TRY_DEF(nodes, new_heap_array(runtime,
      get_fifo_buffer_nodes_length(width, initial_capacity)));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, fifo_buffer_species)));
  set_fifo_buffer_nodes(result, nodes);
  set_fifo_buffer_size(result, 0);
  set_fifo_buffer_width(result, width);
  set_fifo_buffer_next_at(result, kFifoBufferOccupiedRootOffset, kFifoBufferOccupiedRootOffset);
  set_fifo_buffer_prev_at(result, kFifoBufferOccupiedRootOffset, kFifoBufferOccupiedRootOffset);
  size_t first = kFifoBufferReservedNodeCount;
  size_t last = initial_capacity + kFifoBufferReservedNodeCount - 1;
  for (size_t i = first; i <= last; i++) {
    set_fifo_buffer_next_at(result, i, i + 1);
    set_fifo_buffer_prev_at(result, i, i - 1);
  }
  set_fifo_buffer_next_at(result, kFifoBufferFreeRootOffset, first);
  set_fifo_buffer_prev_at(result, first, kFifoBufferFreeRootOffset);
  set_fifo_buffer_prev_at(result, kFifoBufferFreeRootOffset, last);
  set_fifo_buffer_next_at(result, last, kFifoBufferFreeRootOffset);
  return post_create_sanity_check(result, size);
}

value_t new_heap_array_buffer_with_contents(runtime_t *runtime, value_t elements) {
  CHECK_FAMILY(ofArray, elements);
  size_t size = kArrayBufferSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_array_buffer_species)));
  set_array_buffer_elements(result, elements);
  set_array_buffer_length(result, get_array_length(elements));
  return post_create_sanity_check(result, size);
}

static value_t new_heap_id_hash_map_entry_array(runtime_t *runtime, size_t capacity) {
  return new_heap_array(runtime, capacity * kIdHashMapEntryFieldCount);
}

value_t new_heap_id_hash_map(runtime_t *runtime, size_t init_capacity) {
  CHECK_REL("invalid initial capacity", init_capacity, >, 0);
  TRY_DEF(entries, new_heap_id_hash_map_entry_array(runtime, init_capacity));
  size_t size = kIdHashMapSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_id_hash_map_species)));
  set_id_hash_map_entry_array(result, entries);
  set_id_hash_map_size(result, 0);
  set_id_hash_map_capacity(result, init_capacity);
  set_id_hash_map_occupied_count(result, 0);
  return post_create_sanity_check(result, size);
}

value_t new_heap_c_object_species(runtime_t *runtime, alloc_flags_t flags,
    const c_object_info_t *info, value_t type) {
  CHECK_FAMILY(ofType, type);
  size_t size = kCObjectSpeciesSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, mutable_species_species)));
  set_species_instance_family(result, ofCObject);
  set_species_family_behavior(result, &kCObjectBehavior);
  set_species_division_behavior(result, &kCObjectSpeciesBehavior);
  set_c_object_species_data_size(result, new_integer(info->layout.data_size));
  set_c_object_species_value_count(result, new_integer(info->layout.value_count));
  set_c_object_species_type(result, type);
  set_c_object_species_tag(result, info->tag);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_c_object(runtime_t *runtime, alloc_flags_t flags, value_t species,
    blob_t init_data, value_array_t init_values) {
  CHECK_DIVISION(sdCObject, species);
  c_object_layout_t info;
  get_c_object_species_layout_gc_tolerant(species, &info);
  CHECK_REL("too much data", init_data.size, <=, info.data_size);
  CHECK_REL("too many values", init_values.length, <=, info.value_count);
  size_t size = calc_c_object_size(&info);
  size_t aligned_data_size = align_size(kValueSize, info.data_size);
  TRY_DEF(result, alloc_heap_object(runtime, size, species));
  set_c_object_mode_unchecked(runtime, result, vmMutable);
  if (init_data.size < aligned_data_size) {
    // If the aligned backing array is larger than the initial data we clear the
    // whole thing to 0 to not have data lying around that hasn't been
    // deliberately set.
    blob_t aligned_data = new_blob(get_c_object_data_start(result), aligned_data_size);
    blob_fill(aligned_data, 0);
  }
  // Copy the initial data into the object. This time we'll use just the
  // requested part of the data.
  blob_t object_data = get_mutable_c_object_data(result);
  blob_copy_to(init_data, object_data);
  // Copy the initial values into the object.
  value_array_t object_values = get_mutable_c_object_values(result);
  if (init_values.length < info.value_count)
    value_array_fill(object_values, null());
  value_array_copy_to(&init_values, &object_values);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_key(runtime_t *runtime, value_t display_name) {
  size_t size = kKeySize;
  size_t id = runtime->next_key_index++;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_key_species)));
  set_key_id(result, id);
  set_key_display_name(result, display_name);
  return post_create_sanity_check(result, size);
}

value_t new_heap_instance(runtime_t *runtime, value_t species) {
  CHECK_DIVISION(sdInstance, species);
  TRY_DEF(fields, new_heap_id_hash_map(runtime, 16));
  size_t size = kInstanceSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, species));
  set_instance_fields(result, fields);
  return post_create_sanity_check(result, size);
}

value_t new_heap_instance_manager(runtime_t *runtime, value_t display_name) {
  size_t size = kInstanceManagerSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, instance_manager_species)));
  init_frozen_instance_manager_display_name(result, display_name);
  return post_create_sanity_check(result, size);
}

value_t new_heap_void_p(runtime_t *runtime, void *value) {
  size_t size = kVoidPSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, void_p_species)));
  set_void_p_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_factory(runtime_t *runtime, factory_new_instance_t *new_instance,
    factory_set_contents_t *set_contents, utf8_t name_str) {
  TRY_DEF(name, new_heap_utf8(runtime, name_str));
  TRY_DEF(new_instance_wrapper, new_heap_void_p(runtime, (void*) (intptr_t) new_instance));
  TRY_DEF(set_contents_wrapper, new_heap_void_p(runtime, (void*) (intptr_t) set_contents));
  size_t size = kFactorySize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, factory_species)));
  init_frozen_factory_new_instance(result, new_instance_wrapper);
  init_frozen_factory_set_contents(result, set_contents_wrapper);
  init_frozen_factory_name(result, name);
  return post_create_sanity_check(result, size);
}

value_t new_heap_code_block(runtime_t *runtime, value_t bytecode,
    value_t value_pool, size_t high_water_mark) {
  size_t size = kCodeBlockSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_code_block_species)));
  set_code_block_bytecode(result, bytecode);
  set_code_block_value_pool(result, value_pool);
  set_code_block_high_water_mark(result, high_water_mark);
  TRY(ensure_frozen(runtime, result));
  return post_create_sanity_check(result, size);
}

value_t new_heap_type(runtime_t *runtime, alloc_flags_t flags, value_t display_name) {
  size_t size = kTypeSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_type_species)));
  set_type_display_name(result, display_name);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_function(runtime_t *runtime, alloc_flags_t flags,
    value_t display_name) {
  size_t size = kFunctionSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_function_species)));
  set_function_display_name(result, display_name);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_argument_map_trie(runtime_t *runtime, value_t value) {
  CHECK_FAMILY(ofArray, value);
  TRY_DEF(children, new_heap_array_buffer(runtime, 2));
  size_t size = kArgumentMapTrieSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_argument_map_trie_species)));
  set_argument_map_trie_value(result, value);
  set_argument_map_trie_children(result, children);
  return post_create_sanity_check(result, size);
}

value_t new_heap_lambda(runtime_t *runtime, value_t methods, value_t captures) {
  size_t size = kLambdaSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_lambda_species)));
  set_lambda_methods(result, methods);
  set_lambda_captures(result, captures);
  return post_create_sanity_check(result, size);
}

value_t new_heap_block(runtime_t *runtime, value_t section) {
  size_t size = kBlockSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_block_species)));
  set_block_section(result, section);
  return post_create_sanity_check(result, size);
}

value_t new_heap_namespace(runtime_t *runtime, value_t value) {
  TRY_DEF(bindings, new_heap_id_hash_map(runtime, 16));
  size_t size = kNamespaceSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_namespace_species)));
  set_namespace_bindings(result, bindings);
  set_namespace_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_module_fragment(runtime_t *runtime, value_t stage, value_t path,
    value_t predecessor, value_t nspace, value_t methodspace, value_t imports) {
  CHECK_PHYLUM(tpStageOffset, stage);
  CHECK_FAMILY_OPT(ofPath, path);
  CHECK_FAMILY_OPT(ofModuleFragment, predecessor);
  CHECK_FAMILY_OPT(ofNamespace, nspace);
  CHECK_FAMILY_OPT(ofMethodspace, methodspace);
  CHECK_FAMILY_OPT(ofIdHashMap, imports);
  TRY_DEF(phrivate, new_heap_module_fragment_private(runtime, nothing()));
  size_t size = kModuleFragmentSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_module_fragment_species)));
  set_module_fragment_stage(result, stage);
  set_module_fragment_path(result, path);
  set_module_fragment_predecessor(result, predecessor);
  set_module_fragment_namespace(result, nspace);
  set_module_fragment_methodspace(result, methodspace);
  set_module_fragment_imports(result, imports);
  set_module_fragment_epoch(result, feUnbound);
  set_module_fragment_private(result, phrivate);
  set_module_fragment_private_owner(phrivate, result);
  return post_create_sanity_check(result, size);
}

value_t new_heap_module_fragment_private(runtime_t *runtime, value_t owner) {
  size_t size = kModuleFragmentPrivateSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
        ROOT(runtime, mutable_module_fragment_private_species)));
  set_module_fragment_private_owner(result, owner);
  return post_create_sanity_check(result, size);
}

value_t new_heap_empty_module(runtime_t *runtime, value_t path) {
  TRY_DEF(fragments, new_heap_array_buffer(runtime, 16));
  size_t size = kModuleSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_module_species)));
  set_module_path(result, path);
  set_module_fragments(result, fragments);
  return result;

}

value_t new_heap_operation(runtime_t *runtime, alloc_flags_t flags,
    operation_type_t type, value_t value) {
  size_t size = kOperationSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_operation_species)));
  set_operation_type(result, type);
  set_operation_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_path(runtime_t *runtime, alloc_flags_t flags, value_t head,
    value_t tail) {
  size_t size = kPathSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_path_species)));
  set_path_raw_head(result, head);
  set_path_raw_tail(result, tail);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_path_with_names(runtime_t *runtime, alloc_flags_t flags,
    value_t names, size_t offset) {
  size_t length = get_array_length(names);
  if (offset == length)
    return ROOT(runtime, empty_path);
  TRY_DEF(tail, new_heap_path_with_names(runtime, flags, names, offset + 1));
  value_t head = get_array_at(names, offset);
  value_t result = new_heap_path(runtime, afMutable, head, tail);
  TRY(post_process_result(runtime, result, flags));
  return result;
}

value_t new_heap_unknown(runtime_t *runtime, value_t header, value_t payload) {
  size_t size = kUnknownSize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, unknown_species)));
  set_unknown_header(result, header);
  set_unknown_payload(result, payload);
  return post_create_sanity_check(result, size);
}

value_t new_heap_empty_module_loader(runtime_t *runtime) {
  TRY_DEF(modules, new_heap_id_hash_map(runtime, 16));
  size_t size = kModuleLoaderSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, module_loader_species)));
  set_module_loader_modules(result, modules);
  return post_create_sanity_check(result, size);
}

value_t new_heap_unbound_module(runtime_t *runtime, value_t path, value_t fragments) {
  size_t size = kUnboundModuleSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, unbound_module_species)));
  set_unbound_module_path(result, path);
  set_unbound_module_fragments(result, fragments);
  return post_create_sanity_check(result, size);
}

value_t new_heap_unbound_module_fragment(runtime_t *runtime, value_t stage,
    value_t imports, value_t elements) {
  size_t size = kUnboundModuleFragmentSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, unbound_module_fragment_species)));
  set_unbound_module_fragment_stage(result, stage);
  set_unbound_module_fragment_imports(result, imports);
  set_unbound_module_fragment_elements(result, elements);
  return post_create_sanity_check(result, size);
}

value_t new_heap_library(runtime_t *runtime, value_t display_name, value_t modules) {
  size_t size = kLibrarySize;
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, library_species)));
  set_library_display_name(result, display_name);
  set_library_modules(result, modules);
  return post_create_sanity_check(result, size);
}

value_t new_heap_decimal_fraction(runtime_t *runtime, value_t numerator,
    value_t denominator, value_t precision) {
  size_t size = kDecimalFractionSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, decimal_fraction_species)));
  init_frozen_decimal_fraction_numerator(result, numerator);
  init_frozen_decimal_fraction_denominator(result, denominator);
  init_frozen_decimal_fraction_precision(result, precision);
  return post_create_sanity_check(result, size);
}

value_t new_heap_hard_field(runtime_t *runtime, value_t display_name) {
  size_t size = kHardFieldSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, hard_field_species)));
  init_frozen_hard_field_display_name(result, display_name);
  return post_create_sanity_check(result, size);
}

value_t new_heap_soft_field(runtime_t *runtime, value_t display_name) {
  size_t size = kSoftFieldSize;
  TRY_DEF(overlay, new_heap_id_hash_map(runtime, kSoftFieldOverlayMapInitialSize));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, soft_field_species)));
  set_soft_field_display_name(result, display_name);
  set_soft_field_overlay_map(result, overlay);
  return post_create_sanity_check(result, size);
}

value_t new_heap_ambience(runtime_t *runtime) {
  size_t size = kAmbienceSize;
  value_t native_methodspace = ROOT(runtime, builtin_methodspace);
  TRY_DEF(methodspace, new_heap_methodspace(runtime, native_methodspace));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, ambience_species)));
  set_ambience_runtime(result, runtime);
  set_ambience_methodspace(result, methodspace);
  return post_create_sanity_check(result, size);
}

value_t new_heap_freeze_cheat(runtime_t *runtime, value_t value) {
  size_t size = kFreezeCheatSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, freeze_cheat_species)));
  set_freeze_cheat_value(result, value);
  return post_create_sanity_check(result, size);
}

value_t new_heap_pending_promise(runtime_t *runtime) {
  size_t size = kPromiseSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, promise_species)));
  set_promise_state(result, promise_state_pending());
  set_promise_value(result, nothing());
  return post_create_sanity_check(result, size);
}

value_t new_heap_hash_source(runtime_t *runtime, uint64_t seed) {
  size_t size = hash_source_size();
  TRY_DEF(field, new_heap_soft_field(runtime, nothing()));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, hash_source_species)));
  hash_source_state_t *state = get_hash_source_state(result);
  state->twister = tinymt64_construct(tinymt64_params_default(), seed);
  state->next_serial = 0;
  set_hash_source_field(result, field);
  return post_create_sanity_check(result, size);
}

value_t new_heap_hash_oracle(runtime_t *runtime, value_t stream) {
  size_t size = kHashOracleSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_hash_oracle_species)));
  set_hash_oracle_source(result, stream);
  set_hash_oracle_limit(result, nothing());
  return post_create_sanity_check(result, size);
}


// --- P r o c e s s ---

value_t new_heap_stack_piece(runtime_t *runtime, size_t user_capacity,
    value_t previous, value_t stack) {
  CHECK_FAMILY_OPT(ofStackPiece, previous);
  CHECK_FAMILY_OPT(ofStack, stack);
  // Make room for the lid frame.
  size_t full_capacity = user_capacity + kFrameHeaderSize;
  size_t size = calc_stack_piece_size(full_capacity);
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, stack_piece_species)));
  set_stack_piece_capacity(result, new_integer(full_capacity));
  set_stack_piece_previous(result, previous);
  set_stack_piece_stack(result, stack);
  set_stack_piece_lid_frame_pointer(result, nothing());
  value_t *storage = get_stack_piece_storage(result);
  for (size_t i = 0; i < full_capacity; i++)
    storage[i] = nothing();
  frame_t bottom = frame_empty();
  bottom.stack_piece = result;
  bottom.frame_pointer = storage;
  bottom.stack_pointer = storage;
  bottom.limit_pointer = storage;
  bottom.flags = new_flag_set(ffSynthetic | ffStackPieceEmpty);
  bottom.pc = 0;
  close_frame(&bottom);
  return post_create_sanity_check(result, size);
}

// Pushes an artificial bottom frame onto the stack such that at the end of
// the execution we bottom out at a well-defined place and all instructions
// (particularly return) can assume that there's at least one frame below them.
static void push_stack_bottom_frame(runtime_t *runtime, value_t stack) {
  value_t code_block = ROOT(runtime, stack_bottom_code_block);
  frame_t bottom = open_stack(stack);
  bool pushed = try_push_new_frame(&bottom,
      get_code_block_high_water_mark(code_block), ffSynthetic | ffStackBottom,
      false);
  CHECK_TRUE("pushing bottom frame", pushed);
  frame_set_code_block(&bottom, code_block);
  close_frame(&bottom);
}

value_t new_heap_stack(runtime_t *runtime, size_t default_piece_capacity) {
  size_t size = kStackSize;
  TRY_DEF(piece, new_heap_stack_piece(runtime, default_piece_capacity, nothing(),
      nothing()));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, stack_species)));
  set_stack_piece_stack(piece, result);
  set_stack_top_piece(result, piece);
  set_stack_default_piece_capacity(result, default_piece_capacity);
  set_stack_top_barrier(result, nothing());
  push_stack_bottom_frame(runtime, result);
  return post_create_sanity_check(result, size);
}

value_t new_heap_escape(runtime_t *runtime, value_t section) {
  size_t size = kEscapeSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, escape_species)));
  set_escape_section(result, section);
  return post_create_sanity_check(result, size);
}

value_t new_heap_backtrace(runtime_t *runtime, value_t entries) {
  size_t size = kBacktraceSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, backtrace_species)));
  set_backtrace_entries(result, entries);
  return post_create_sanity_check(result, size);
}

value_t new_heap_backtrace_entry(runtime_t *runtime, value_t invocation,
    value_t opcode) {
  size_t size = kBacktraceEntrySize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, backtrace_entry_species)));
  set_backtrace_entry_invocation(result, invocation);
  set_backtrace_entry_opcode(result, opcode);
  return post_create_sanity_check(result, size);
}

value_t new_heap_process(runtime_t *runtime) {
  // First do everything that can fail. If it does fail then that's okay, there
  // will be no references to these objects so they should die untouched after
  // the next gc.
  size_t size = kProcessSize;
  TRY_DEF(work_queue, new_heap_fifo_buffer(runtime, kProcessWorkQueueWidth, 256));
  TRY_DEF(root_task, new_heap_task(runtime, nothing()));
  TRY_DEF(airlock_ptr, new_heap_void_p(runtime, NULL));
  tinymt64_state_t new_state;
  uint64_t hash_source_seed = tinymt64_next_uint64(&runtime->random, &new_state);
  TRY_DEF(hash_source, new_heap_hash_source(runtime, hash_source_seed));
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, process_species)));
  // Once everything is allocated we can initialize the result. From this point
  // on only airlock allocation can fail.
  set_process_work_queue(result, work_queue);
  set_process_root_task(result, root_task);
  set_process_hash_source(result, hash_source);
  set_process_airlock_ptr(result, airlock_ptr);
  set_task_process(root_task, result);
  // Allocate the airlock. If this fails, again, it's safe to leave everything
  // as garbage.
  process_airlock_t *airlock = process_airlock_new(runtime);
  if (airlock == NULL)
    return new_system_error_condition(seAllocationFailed);
  // Store the airlock in the process and schedule for it to be finalized at
  // the same time. From now on the process object has to be in a consistent
  // state because the finalizer may be invoked at any time.
  set_void_p_value(airlock_ptr, airlock);
  runtime_protect_value_with_flags(runtime, result, tfWeak | tfSelfDestruct | tfFinalize);
  // Don't update the random state until after we know the whole allocation has
  // succeeded.
  runtime->random.state = new_state;
  return post_create_sanity_check(result, size);
}

value_t new_heap_task(runtime_t *runtime, value_t process) {
  size_t size = kTaskSize;
  TRY_DEF(stack, new_heap_stack(runtime, 1024));
  TRY_DEF(result, alloc_heap_object(runtime, size, ROOT(runtime, task_species)));
  set_task_process(result, process);
  set_task_stack(result, stack);
  return post_create_sanity_check(result, size);
}

value_t new_heap_reified_arguments(runtime_t *runtime, value_t params,
    value_t values, value_t argmap, value_t tags) {
  size_t size = kReifiedArgumentsSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, reified_arguments_species)));
  set_reified_arguments_params(result, params);
  set_reified_arguments_values(result, values);
  set_reified_arguments_argmap(result, argmap);
  set_reified_arguments_tags(result, tags);
  return post_create_sanity_check(result, size);
}

value_t new_heap_native_remote(runtime_t *runtime, service_descriptor_t *impl) {
  size_t size = kNativeRemoteSize;
  TRY_DEF(display_name, import_pton_variant(runtime, impl->display_name));
  // Fill a map with the method pointers.
  TRY_DEF(impls, new_heap_id_hash_map(runtime, 16));
  for (size_t i = 0; i < impl->methodc; i++) {
    service_method_t *method = &(impl->methodv[i]);
    TRY_DEF(selector, import_pton_variant(runtime, method->selector));
    CHECK_DEEP_FROZEN(selector);
    unary_callback_t *callback = method->callback;
    TRY_DEF(impl, new_heap_void_p(runtime, callback));
    CHECK_DEEP_FROZEN(impl);
    TRY(try_set_id_hash_map_at(impls, selector, impl, false));
  }
  // Freeze the implementation map; the keys and values are known to already be
  // deep frozen.
  TRY(ensure_frozen(runtime, impls));
  TRY(validate_deep_frozen(runtime, impls, NULL));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, native_remote_species)));
  init_frozen_native_remote_impls(result, impls);
  init_frozen_native_remote_display_name(result, display_name);
  return post_create_sanity_check(result, size);
}


// --- M e t h o d ---

value_t new_heap_guard(runtime_t *runtime, alloc_flags_t flags, guard_type_t type,
    value_t value) {
  size_t size = kGuardSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_guard_species)));
  set_guard_type(result, type);
  set_guard_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_signature(runtime_t *runtime, alloc_flags_t flags, value_t tags,
    size_t param_count, size_t mandatory_count, bool allow_extra) {
  CHECK_FAMILY_OPT(ofArray, tags);
  size_t size = kSignatureSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_signature_species)));
  set_signature_tags(result, tags);
  set_signature_parameter_count(result, param_count);
  set_signature_mandatory_count(result, mandatory_count);
  set_signature_allow_extra(result, allow_extra);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_parameter(runtime_t *runtime, alloc_flags_t flags, value_t guard,
    value_t tags, bool is_optional, size_t index) {
  CHECK_FAMILY_OPT(ofGuard, guard);
  CHECK_FAMILY_OPT(ofArray, tags);
  size_t size = kParameterSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_parameter_species)));
  set_parameter_guard(result, guard);
  set_parameter_tags(result, tags);
  set_parameter_is_optional(result, is_optional);
  set_parameter_index(result, index);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_signature_map(runtime_t *runtime) {
  size_t size = kSignatureMapSize;
  TRY_DEF(entries, new_heap_array_buffer(runtime, kMethodArrayInitialSize));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_signature_map_species)));
  set_signature_map_entries(result, entries);
  return post_create_sanity_check(result, size);
}

value_t new_heap_methodspace(runtime_t *runtime, value_t parent) {
  CHECK_FAMILY_OPT(ofMethodspace, parent);
  CHECK_FROZEN(parent);
  size_t size = kMethodspaceSize;
  TRY_DEF(inheritance, new_heap_id_hash_map(runtime, kInheritanceMapInitialSize));
  TRY_DEF(methods, new_heap_signature_map(runtime));
  TRY_DEF(cache_ptr, new_heap_freeze_cheat(runtime, nothing()));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_methodspace_species)));
  set_methodspace_inheritance(result, inheritance);
  set_methodspace_methods(result, methods);
  set_methodspace_parent(result, parent);
  set_methodspace_cache_ptr(result, cache_ptr);
  return post_create_sanity_check(result, size);
}

value_t new_heap_method(runtime_t *runtime, alloc_flags_t alloc_flags,
    value_t signature, value_t syntax, value_t code, value_t fragment,
    value_t method_flags) {
  CHECK_FAMILY_OPT(ofSignature, signature);
  CHECK_FAMILY_OPT(ofCodeBlock, code);
  CHECK_PHYLUM(tpFlagSet, method_flags);
  size_t size = kMethodSize;
  TRY_DEF(code_ptr, new_heap_freeze_cheat(runtime, code));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_method_species)));
  set_method_signature(result, signature);
  set_method_code_ptr(result, code_ptr);
  set_method_syntax(result, syntax);
  set_method_module_fragment(result, fragment);
  set_method_flags(result, method_flags);
  TRY(post_process_result(runtime, result, alloc_flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_call_tags(runtime_t *runtime, alloc_flags_t flags,
    value_t entries) {
  size_t size = kCallTagsSize;
  CHECK_TRUE("unsorted argument array", is_pair_array_sorted(entries));
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_call_tags_species)));
  set_call_tags_entries(result, entries);
  // There's no reason to take these as arguments since they can be calculated
  // from the entries. Also, this way we're sure they are determined correctly.
  set_call_tags_subject_offset(result, nothing());
  set_call_tags_selector_offset(result, nothing());
  for (size_t i = 0; i < get_call_tags_entry_count(result); i++) {
    value_t tag = get_call_tags_tag_at(result, i);
    if (is_same_value(tag, ROOT(runtime, subject_key)))
      set_call_tags_subject_offset(result, new_integer(i));
    else if (is_same_value(tag, ROOT(runtime, selector_key)))
      set_call_tags_selector_offset(result, new_integer(i));
  }
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_call_data(runtime_t *runtime, value_t tags, value_t values) {
  CHECK_EQ("invalid call data", get_call_tags_entry_count(tags),
      get_array_length(values));
  size_t size = kCallDataSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_call_data_species)));
  set_call_data_tags(result, tags);
  set_call_data_values(result, values);
  return post_create_sanity_check(result, size);
}

value_t new_heap_builtin_marker(runtime_t *runtime, value_t name) {
  size_t size = kBuiltinMarkerSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, builtin_marker_species)));
  set_builtin_marker_name(result, name);
  return post_create_sanity_check(result, size);
}

value_t new_heap_builtin_implementation(runtime_t *runtime, alloc_flags_t flags,
    value_t name, value_t code, size_t posc, value_t method_flags) {
  size_t size = kBuiltinImplementationSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_builtin_implementation_species)));
  set_builtin_implementation_name(result, name);
  set_builtin_implementation_code(result, code);
  set_builtin_implementation_argument_count(result, posc);
  set_builtin_implementation_method_flags(result, method_flags);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}


// --- S y n t a x ---

value_t new_heap_literal_ast(runtime_t *runtime, alloc_flags_t flags, value_t value) {
  size_t size = kLiteralAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_literal_ast_species)));
  set_literal_ast_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_array_ast(runtime_t *runtime, alloc_flags_t flags, value_t elements) {
  size_t size = kArrayAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_array_ast_species)));
  set_array_ast_elements(result, elements);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_invocation_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t arguments) {
  size_t size = kInvocationAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_invocation_ast_species)));
  set_invocation_ast_arguments(result, arguments);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_call_literal_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t arguments) {
  size_t size = kCallLiteralAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_call_literal_ast_species)));
  set_call_literal_ast_arguments(result, arguments);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_call_literal_argument_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t tag, value_t value) {
  size_t size = kCallLiteralArgumentAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_call_literal_argument_ast_species)));
  set_call_literal_argument_ast_tag(result, tag);
  set_call_literal_argument_ast_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_signal_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t escape, value_t arguments, value_t defawlt) {
  size_t size = kSignalAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_signal_ast_species)));
  set_signal_ast_escape(result, escape);
  set_signal_ast_arguments(result, arguments);
  set_signal_ast_default(result, defawlt);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_signal_handler_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t body, value_t handlers) {
  size_t size = kSignalHandlerAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_signal_handler_ast_species)));
  set_signal_handler_ast_body(result, body);
  set_signal_handler_ast_handlers(result, handlers);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_ensure_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t body, value_t on_exit) {
  size_t size = kEnsureAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_ensure_ast_species)));
  set_ensure_ast_body(result, body);
  set_ensure_ast_on_exit(result, on_exit);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_argument_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t tag, value_t value, value_t next_guard) {
  size_t size = kArgumentAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_argument_ast_species)));
  set_argument_ast_tag(result, tag);
  set_argument_ast_value(result, value);
  set_argument_ast_next_guard(result, next_guard);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_sequence_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t values) {
  size_t size = kSequenceAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_sequence_ast_species)));
  set_sequence_ast_values(result, values);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_local_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t is_mutable, value_t value, value_t body) {
  size_t size = kLocalDeclarationAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_local_declaration_ast_species)));
  set_local_declaration_ast_symbol(result, symbol);
  set_local_declaration_ast_is_mutable(result, is_mutable);
  set_local_declaration_ast_value(result, value);
  set_local_declaration_ast_body(result, body);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_block_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t methods, value_t body) {
  size_t size = kBlockAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_block_ast_species)));
  set_block_ast_symbol(result, symbol);
  set_block_ast_methods(result, methods);
  set_block_ast_body(result, body);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_with_escape_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t body) {
  size_t size = kWithEscapeAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_with_escape_ast_species)));
  set_with_escape_ast_symbol(result, symbol);
  set_with_escape_ast_body(result, body);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_local_variable_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol) {
  size_t size = kLocalVariableAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_local_variable_ast_species)));
  set_local_variable_ast_symbol(result, symbol);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_variable_assignment_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t target, value_t value) {
  size_t size = kVariableAssignmentAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_variable_assignment_ast_species)));
  set_variable_assignment_ast_target(result, target);
  set_variable_assignment_ast_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}


value_t new_heap_namespace_variable_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t ident) {
  size_t size = kNamespaceVariableAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_namespace_variable_ast_species)));
  set_namespace_variable_ast_identifier(result, ident);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_symbol_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t name, value_t origin) {
  size_t size = kSymbolAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_symbol_ast_species)));
  set_symbol_ast_name(result, name);
  set_symbol_ast_origin(result, origin);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_lambda_ast(runtime_t *runtime, alloc_flags_t flags, value_t methods) {
  size_t size = kLambdaAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_lambda_ast_species)));
  set_lambda_ast_methods(result, methods);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_parameter_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t symbol, value_t tags, value_t guard) {
  size_t size = kParameterAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_parameter_ast_species)));
  set_parameter_ast_symbol(result, symbol);
  set_parameter_ast_tags(result, tags);
  set_parameter_ast_guard(result, guard);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_guard_ast(runtime_t *runtime, alloc_flags_t flags,
    guard_type_t type, value_t value) {
  size_t size = kGuardAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_guard_ast_species)));
  set_guard_ast_type(result, type);
  set_guard_ast_value(result, value);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_signature_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t parameters, value_t allow_extra, value_t reified) {
  size_t size = kSignatureAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_signature_ast_species)));
  set_signature_ast_parameters(result, parameters);
  set_signature_ast_allow_extra(result, allow_extra);
  set_signature_ast_reified(result, reified);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_method_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t signature, value_t body) {
  size_t size = kMethodAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_method_ast_species)));
  set_method_ast_signature(result, signature);
  set_method_ast_body(result, body);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_program_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t entry_point, value_t module) {
  size_t size = kProgramAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_program_ast_species)));
  set_program_ast_entry_point(result, entry_point);
  set_program_ast_module(result, module);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_identifier(runtime_t *runtime, alloc_flags_t flags,
    value_t stage, value_t path) {
  CHECK_PHYLUM_OPT(tpStageOffset, stage);
  CHECK_FAMILY_OPT(ofPath, path);
  size_t size = kIdentifierSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_identifier_species)));
  set_identifier_stage(result, stage);
  set_identifier_path(result, path);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_namespace_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t annotations, value_t path, value_t value) {
  CHECK_FAMILY_OPT(ofPath, path);
  CHECK_FAMILY_OPT(ofArray, annotations);
  size_t size = kNamespaceDeclarationAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_namespace_declaration_ast_species)));
  set_namespace_declaration_ast_path(result, path);
  set_namespace_declaration_ast_value(result, value);
  set_namespace_declaration_ast_annotations(result, annotations);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_method_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t annotations, value_t method) {
  size_t size = kMethodDeclarationAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_method_declaration_ast_species)));
  set_method_declaration_ast_annotations(result, annotations);
  set_method_declaration_ast_method(result, method);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_is_declaration_ast(runtime_t *runtime, alloc_flags_t flags,
    value_t subtype, value_t supertype) {
  size_t size = kIsDeclarationAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, mutable_is_declaration_ast_species)));
  set_is_declaration_ast_subtype(result, subtype);
  set_is_declaration_ast_supertype(result, supertype);
  TRY(post_process_result(runtime, result, flags));
  return post_create_sanity_check(result, size);
}

value_t new_heap_current_module_ast(runtime_t *runtime) {
  size_t size = kCurrentModuleAstSize;
  TRY_DEF(result, alloc_heap_object(runtime, size,
      ROOT(runtime, current_module_ast_species)));
  return post_create_sanity_check(result, size);
}


// --- M i s c ---

value_t alloc_heap_object(runtime_t *runtime, size_t bytes, value_t species) {
  address_t addr = NULL;
  if (runtime->gc_fuzzer != NULL) {
    if (gc_fuzzer_tick(runtime->gc_fuzzer))
      return new_heap_exhausted_condition(bytes);
  }
  if (!heap_try_alloc(&runtime->heap, bytes, &addr))
    return new_heap_exhausted_condition(bytes);
  value_t result = new_heap_object(addr);
  set_heap_object_header(result, species);
  return result;
}

value_t clone_heap_object(runtime_t *runtime, value_t original) {
  heap_object_layout_t layout;
  get_heap_object_layout(original, &layout);
  TRY_DEF(result, alloc_heap_object(runtime, layout.size, get_heap_object_species(original)));
  blob_t dest_blob = new_blob(get_heap_object_address(result), layout.size);
  blob_t src_blob = new_blob(get_heap_object_address(original), layout.size);
  blob_copy_to(src_blob, dest_blob);
  return result;
}

static value_t extend_id_hash_map(runtime_t *runtime, value_t map) {
  // Create the new entry array first so that if it fails we bail out asap.
  size_t old_capacity = get_id_hash_map_capacity(map);
  size_t new_capacity = old_capacity * 2;
  TRY_DEF(new_entry_array, new_heap_id_hash_map_entry_array(runtime, new_capacity));
  // Capture the relevant old state in an iterator before resetting the map.
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, map);
  // Reset the map.
  set_id_hash_map_capacity(map, new_capacity);
  set_id_hash_map_size(map, 0);
  set_id_hash_map_occupied_count(map, 0);
  set_id_hash_map_entry_array(map, new_entry_array);
  // Scan through and add the old data.
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    value_t extension = try_set_id_hash_map_at(map, key, value, false);
    // Since we were able to successfully add these pairs to the old smaller
    // map it can't fail this time around.
    CHECK_FALSE("rehashing failed", is_condition(extension));
  }
  return success();
}

value_t set_id_hash_map_at(runtime_t *runtime, value_t map, value_t key, value_t value) {
  value_t first_try = try_set_id_hash_map_at(map, key, value, false);
  if (in_condition_cause(ccMapFull, first_try)) {
    TRY(extend_id_hash_map(runtime, map));
    value_t second_try = try_set_id_hash_map_at(map, key, value, false);
    // It should be impossible for the second try to fail if the first try could
    // hash the key and extending was successful.
    CHECK_FALSE("second try failure", is_condition(second_try));
    return second_try;
  } else {
    return first_try;
  }
}

value_t set_instance_field(runtime_t *runtime, value_t instance, value_t key,
    value_t value) {
  CHECK_MUTABLE(instance);
  value_t fields = get_instance_fields(instance);
  return set_id_hash_map_at(runtime, fields, key, value);
}

value_t import_pton_variant(runtime_t *runtime, pton_variant_t variant) {
  switch (pton_type(variant)) {
    case PTON_INTEGER: {
      int64_t value = pton_int64_value(variant);
      return new_integer(value);
    }
    case PTON_STRING: {
      const char *chars = pton_string_chars(variant);
      size_t size = pton_string_length(variant);
      return new_heap_utf8(runtime, new_string(chars, size));
    }
    case PTON_ARRAY: {
      size_t size = pton_array_length(variant);
      TRY_DEF(result, new_heap_array(runtime, size));
      for (size_t i = 0; i < size; i++) {
        TRY_DEF(value, import_pton_variant(runtime, pton_array_get(variant, i)));
        set_array_at(result, i, value);
      }
      return result;
    }
    case PTON_NULL:
      return null();
    case PTON_BOOL:
      return pton_bool_value(variant) ? yes() : no();
    default:
      return new_invalid_input_condition();
  }
}
