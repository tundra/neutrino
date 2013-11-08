// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "bind.h"
#include "file.h"
#include "runtime.h"


// --- M o d u l e   l o a d e r ---

FIXED_GET_MODE_IMPL(module_loader, vmMutable);
TRIVIAL_PRINT_ON_IMPL(ModuleLoader, module_loader);

ACCESSORS_IMPL(ModuleLoader, module_loader, acInFamilyOpt, ofIdHashMap, Modules, modules);

value_t module_loader_validate(value_t self) {
  VALIDATE_FAMILY(ofModuleLoader, self);
  return success();
}

// Reads a library from the given library path and adds the modules to this
// loaders set of available modules.
static value_t module_loader_read_library(runtime_t *runtime, value_t self,
    value_t library_path) {
  string_t library_path_str;
  get_string_contents(library_path, &library_path_str);
  TRY_DEF(data, read_file_to_blob(runtime, &library_path_str));
  TRY_DEF(library, runtime_plankton_deserialize(runtime, data));
  if (false)
    print_ln("library: %v", library);
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


// --- U n b o u n d   m o d u l e ---

FIXED_GET_MODE_IMPL(unbound_module, vmMutable);
TRIVIAL_PRINT_ON_IMPL(UnboundModule, unbound_module);

ACCESSORS_IMPL(UnboundModule, unbound_module, acInFamilyOpt, ofPath, Path, path);
ACCESSORS_IMPL(UnboundModule, unbound_module, acInFamilyOpt, ofArray, Fragments, fragments);

value_t unbound_module_validate(value_t self) {
  VALIDATE_FAMILY(ofUnboundModule, self);
  return success();
}

value_t set_unbound_module_contents(value_t object, runtime_t *runtime, value_t contents) {
  EXPECT_FAMILY(scInvalidInput, ofIdHashMap, contents);
  TRY_DEF(path, get_id_hash_map_at(contents, RSTR(runtime, path)));
  TRY_DEF(fragments, get_id_hash_map_at(contents, RSTR(runtime, fragments)));
  set_unbound_module_path(object, path);
  set_unbound_module_fragments(object, fragments);
  return success();
}
