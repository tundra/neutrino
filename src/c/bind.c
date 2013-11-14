// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "behavior.h"
#include "bind.h"
#include "file.h"
#include "interp.h"
#include "log.h"
#include "runtime.h"

// --- B i n d i n g ---

// Encapsulates the data maintained during the binding process.
typedef struct {
  // Map from module names to bound modules.
  value_t modules;
  runtime_t *runtime;
} binding_context_t;

// Returns true if the given context has an entry for the module with the given
// path.
static bool binding_context_has_module(binding_context_t *context, value_t path) {
  return has_id_hash_map_at(context->modules, path);
}

// Adds both versions of a module, bound and unbound, to the set to be loaded
// by this binding context.
static value_t binding_context_add_module(binding_context_t *context,
    value_t unbound_module, value_t module) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  CHECK_FAMILY(ofModule, module);
  value_t path = get_unbound_module_path(unbound_module);
  CHECK_FALSE("already bound", binding_context_has_module(context, path));
  TRY_DEF(pair, new_heap_array(context->runtime, 2));
  set_array_at(pair, 0, unbound_module);
  set_array_at(pair, 1, module);
  TRY(set_id_hash_map_at(context->runtime, context->modules, path, pair));
  return success();
}

// Looks up the bound and unbound versions of the module with the given path
// in this context. Either out-argument can be NULL in which case no value
// of that type is stored.
static value_t binding_context_get_module(binding_context_t *context,
    value_t path, value_t *unbound_module_out, value_t *module_out) {
  TRY_DEF(pair, get_id_hash_map_at(context->modules, path));
  if (unbound_module_out != NULL)
    *unbound_module_out = get_array_at(pair, 0);
  if (module_out != NULL)
    *module_out = get_array_at(pair, 1);
  return success();
}

// Ensures that the given unbound module has been scheduled to be bound.
static value_t ensure_unbound_module_scheduled(binding_context_t *context,
    value_t unbound_module) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  value_t path = get_unbound_module_path(unbound_module);
  value_t module;
  if (!binding_context_has_module(context, path)) {
    TRY_SET(module, new_heap_empty_module(context->runtime, path));
    TRY(binding_context_add_module(context, unbound_module, module));
  }
  return success();
}

// Returns true if the given fragment hasn't already been bound but has all its
// dependencies bound and hence can be bound itself.
static bool is_fragment_ready_to_bind(binding_context_t *context,
    value_t unbound_module, value_t unbound_fragment) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  CHECK_FAMILY(ofUnboundModuleFragment, unbound_fragment);
  // Check whether we've already bound this fragment.
  value_t path = get_unbound_module_path(unbound_module);
  size_t stage = get_integer_value(get_unbound_module_fragment_stage(unbound_fragment));
  value_t module;
  binding_context_get_module(context, path, NULL, &module);
  if (!is_signal(scNotFound, get_module_fragment_at(module, stage)))
    return false;
  return true;
}

// Adds a namespace binding for the value
static value_t apply_namespace_declaration(runtime_t *runtime, value_t decl,
    value_t fragment) {
  value_t value_syntax = get_namespace_declaration_ast_value(decl);
  TRY_DEF(code_block, compile_expression(runtime, value_syntax,
      fragment, scope_lookup_callback_get_bottom()));
  TRY_DEF(value, run_code_block_until_signal(runtime, code_block));
  value_t namespace = get_module_fragment_namespace(fragment);
  value_t path = get_namespace_declaration_ast_path(decl);
  value_t name = get_path_head(path);
  TRY(set_namespace_binding_at(runtime, namespace, name, value));
  return success();
}

static value_t apply_method_declaration(runtime_t *runtime, value_t decl,
    value_t fragment) {
  value_t method_ast = get_method_declaration_ast_method(decl);
  print_ln("%9v", method_ast);
  TRY_DEF(method, compile_method_ast_to_method(runtime, method_ast, fragment));
  value_t methodspace = get_module_fragment_methodspace(fragment);
  TRY(add_methodspace_method(runtime, methodspace, method));
  return success();
}

// Performs the appropriate action for a fragment element to the given fragment.
static value_t apply_unbound_fragment_element(runtime_t *runtime, value_t element,
    value_t fragment) {
  object_family_t family = get_object_family(element);
  switch (family) {
    case ofNamespaceDeclarationAst:
      return apply_namespace_declaration(runtime, element, fragment);
    case ofMethodDeclarationAst:
      return apply_method_declaration(runtime, element, fragment);
    default:
      ERROR("Invalid toplevel element %s", get_object_family_name(family));
      return success();
  }
}

static value_t bind_module_fragment(binding_context_t *context,
    value_t unbound_module, value_t unbound_fragment) {
  CHECK_FAMILY(ofUnboundModule, unbound_module);
  CHECK_FAMILY(ofUnboundModuleFragment, unbound_fragment);
  // Create the new bound fragment object and add it to the module.
  value_t path = get_unbound_module_path(unbound_module);
  size_t stage = get_integer_value(get_unbound_module_fragment_stage(unbound_fragment));
  value_t module;
  binding_context_get_module(context, path, NULL, &module);
  TRY_DEF(namespace, new_heap_namespace(context->runtime));
  TRY_DEF(methodspace, new_heap_methodspace(context->runtime));
  // Prime all methodspaces with the built-in one. This is a temporary hack
  // (famous last words), longer term the built-ins should be loaded through
  // the same mechanism as all other methods.
  TRY(add_methodspace_import(context->runtime, methodspace,
      ROOT(context->runtime, builtin_methodspace)));
  TRY_DEF(fragment, new_heap_module_fragment(context->runtime, module, stage,
      namespace, methodspace));
  TRY(add_to_array_buffer(context->runtime, get_module_fragments(module), fragment));
  // Apply all the elements.
  value_t elements = get_unbound_module_fragment_elements(unbound_fragment);
  for (size_t i = 0; i < get_array_length(elements); i++) {
    value_t element = get_array_at(elements, i);
    TRY(apply_unbound_fragment_element(context->runtime, element, fragment));
  }
  return success();
}

// Iteratively bind modules, for each round in the loop binding the next
// fragment whose dependencies have all been bound. This is not super efficient
// but for few modules it should work just fine.
static value_t run_module_binding_loop(binding_context_t *context) {
  // Loop around as long as there is work to do. When the work runs out we'll
  // drop out through the bottom of the loop.
  loop: do {
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, context->modules);
    while (id_hash_map_iter_advance(&iter)) {
      value_t key;
      value_t pair;
      id_hash_map_iter_get_current(&iter, &key, &pair);
      value_t unbound_module = get_array_at(pair, 0);
      value_t unbound_fragments = get_unbound_module_fragments(unbound_module);
      for (size_t i = 0; i < get_array_length(unbound_fragments); i++) {
        value_t unbound_fragment = get_array_at(unbound_fragments, i);
        if (is_fragment_ready_to_bind(context, unbound_module, unbound_fragment)) {
          TRY(bind_module_fragment(context, unbound_module, unbound_fragment));
          goto loop;
        }
      }
    }
  } while (false);
  return success();
}

value_t build_bound_module(runtime_t *runtime, value_t unbound_module) {
  binding_context_t context;
  context.runtime = runtime;
  TRY_SET(context.modules, new_heap_id_hash_map(runtime, 16));
  TRY(ensure_unbound_module_scheduled(&context, unbound_module));
  TRY(run_module_binding_loop(&context));
  value_t path = get_unbound_module_path(unbound_module);
  value_t module;
  TRY(binding_context_get_module(&context, path, NULL, &module));
  return module;
}


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
