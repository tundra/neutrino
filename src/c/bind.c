// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "bind.h"
#include "file.h"
#include "runtime.h"


// --- M o d u l e   l o a d e r ---

FIXED_GET_MODE_IMPL(module_loader, vmMutable);

ACCESSORS_IMPL(ModuleLoader, module_loader, acInFamilyOpt, ofIdHashMap, Modules, modules);

value_t module_loader_validate(value_t self) {
  VALIDATE_FAMILY(ofModuleLoader, self);
  return success();
}

// Reads a library from the given library path and adds the modules to this
// loaders set of available modules.
static value_t module_loader_read_library(runtime_t *runtime, value_t self,
    value_t library_path) {
  // Read the library from the file.
  string_t library_path_str;
  get_string_contents(library_path, &library_path_str);
  TRY_DEF(data, read_file_to_blob(runtime, &library_path_str));
  TRY_DEF(library, runtime_plankton_deserialize(runtime, data));
  if (!in_family(ofLibrary, library))
    return new_invalid_input_signal();
  set_library_display_name(library, library_path);
  // Load all the modules from the library into this module loader.
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, get_library_modules(library));
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    TRY(set_id_hash_map_at(runtime, get_module_loader_modules(self), key, value));
  }
  return success();
}

value_t module_loader_process_options(runtime_t *runtime, value_t self,
    value_t options) {
  CHECK_FAMILY(ofIdHashMap, options);
  value_t libraries = get_id_hash_map_at_with_default(options, RSTR(runtime, libraries),
      ROOT(runtime, empty_array));
  for (size_t i = 0; i < get_array_length(libraries); i++) {
    value_t library_path = get_array_at(libraries, i);
    TRY(module_loader_read_library(runtime, self, library_path));
  }
  return success();
}

void module_loader_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<module loader ");
  value_t modules = get_module_loader_modules(value);
  value_print_inner_on(modules, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- L i b r a r y ---

FIXED_GET_MODE_IMPL(library, vmMutable);

ACCESSORS_IMPL(Library, library, acNoCheck, 0, DisplayName, display_name);
ACCESSORS_IMPL(Library, library, acInFamilyOpt, ofIdHashMap, Modules, modules);

value_t library_validate(value_t self) {
  VALIDATE_FAMILY(ofLibrary, self);
  return success();
}

value_t plankton_new_library(runtime_t *runtime) {
  return new_heap_library(runtime, ROOT(runtime, nothing), ROOT(runtime, nothing));
}

value_t plankton_set_library_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, modules);
  set_library_modules(object, modules);
  return success();
}

void library_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<library(");
  value_t display_name = get_library_display_name(value);
  value_print_inner_on(display_name, buf, flags, depth - 1);
  string_buffer_printf(buf, ") ");
  value_t modules = get_library_modules(value);
  value_print_inner_on(modules, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- U n b o u n d   m o d u l e ---

FIXED_GET_MODE_IMPL(unbound_module, vmMutable);

ACCESSORS_IMPL(UnboundModule, unbound_module, acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(UnboundModule, unbound_module, acInFamilyOpt, ofArray, Fragments, fragments);

value_t unbound_module_validate(value_t self) {
  VALIDATE_FAMILY(ofUnboundModule, self);
  return success();
}

value_t plankton_new_unbound_module(runtime_t *runtime) {
  return new_heap_unbound_module(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing));
}

value_t plankton_set_unbound_module_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, path, fragments);
  set_unbound_module_path(object, path);
  set_unbound_module_fragments(object, fragments);
  return success();
}

void unbound_module_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<unbound_module(");
  value_t path = get_unbound_module_path(value);
  value_print_inner_on(path, buf, flags, depth - 1);
  string_buffer_printf(buf, ") ");
  value_t fragments = get_unbound_module_fragments(value);
  value_print_inner_on(fragments, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}


// --- U n b o u n d   m o d u l e   f r a g m e n t ---

FIXED_GET_MODE_IMPL(unbound_module_fragment, vmMutable);

ACCESSORS_IMPL(UnboundModuleFragment, unbound_module_fragment, acNoCheck, 0,
    Stage, stage);
ACCESSORS_IMPL(UnboundModuleFragment, unbound_module_fragment, acInFamilyOpt,
    ofArray, Imports, imports);
ACCESSORS_IMPL(UnboundModuleFragment, unbound_module_fragment, acInFamilyOpt,
    ofArray, Elements, elements);

value_t unbound_module_fragment_validate(value_t self) {
  VALIDATE_FAMILY(ofUnboundModuleFragment, self);
  return success();
}

value_t plankton_new_unbound_module_fragment(runtime_t *runtime) {
  return new_heap_unbound_module_fragment(runtime, ROOT(runtime, nothing),
      ROOT(runtime, nothing), ROOT(runtime, nothing));
}

value_t plankton_set_unbound_module_fragment_contents(value_t object, runtime_t *runtime,
    value_t contents) {
  UNPACK_PLANKTON_MAP(contents, stage, imports, elements);
  set_unbound_module_fragment_stage(object, stage);
  set_unbound_module_fragment_imports(object, imports);
  set_unbound_module_fragment_elements(object, elements);
  return success();
}

void unbound_module_fragment_print_on(value_t value, string_buffer_t *buf, print_flags_t flags,
    size_t depth) {
  string_buffer_printf(buf, "#<unbound_module_fragment(");
  value_t stage = get_unbound_module_fragment_stage(value);
  value_print_inner_on(stage, buf, flags, depth - 1);
  string_buffer_printf(buf, ") imports: ");
  value_t imports = get_unbound_module_fragment_imports(value);
  value_print_inner_on(imports, buf, flags, depth - 1);
  string_buffer_printf(buf, ") elements: ");
  value_t elements = get_unbound_module_fragment_elements(value);
  value_print_inner_on(elements, buf, flags, depth - 1);
  string_buffer_printf(buf, ">");
}
