#include "alloc.h"
#include "behavior.h"
#include "runtime.h"
#include "value-inl.h"

value_t roots_init(roots_t *roots, runtime_t *runtime) {
  // The meta-root is tricky because it is its own species. So we set it up in
  // two steps.
  TRY_DEF(meta, new_heap_compact_species_unchecked(runtime, ofSpecies, &kSpeciesBehavior));
  set_object_species(meta, meta);
  roots->species_species = meta;

  // Generate initialization for the other compact species.
#define CREATE_COMPACT_SPECIES(Family, family)                                 \
  TRY_SET(roots->family##_species, new_heap_compact_species(runtime, of##Family, &k##Family##Behavior));
  ENUM_COMPACT_OBJECT_FAMILIES(CREATE_COMPACT_SPECIES)
#undef CREATE_COMPACT_SPECIES

  // Initialize singletons first since we need those to create more complex
  // values below.
  TRY_SET(roots->null, new_heap_null(runtime));
  TRY_SET(roots->thrue, new_heap_bool(runtime, true));
  TRY_SET(roots->fahlse, new_heap_bool(runtime, false));

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

void roots_clear(roots_t *roots) {
  // Generate code for clearing the species.
#define CLEAR_SPECIES_FIELD(Family, family) roots->family##_species = success();
  ENUM_OBJECT_FAMILIES(CLEAR_SPECIES_FIELD)
#undef CLEAR_SPECIES_FIELD

  // Clear the singletons manually.
  roots->syntax_factories = success();
  roots->null = success();
  roots->thrue = success();
  roots->fahlse = success();

#define __CLEAR_STRING_TABLE_ENTRY__(name, value) roots->string_table.name = success();
  ENUM_STRING_TABLE(__CLEAR_STRING_TABLE_ENTRY__)
#undef __CLEAR_STRING_TABLE_ENTRY__
}

value_t roots_validate(roots_t *roots) {
  // Checks whether the argument is within the specified family, otherwise
  // signals a validation failure.
  #define VALIDATE_OBJECT(ofFamily, value) do { \
    if (!in_family(ofFamily, (value)))          \
      return new_signal(scValidationFailed);    \
    TRY(object_validate(value));                \
  } while (false)

  // Checks that the given value is a species with the specified instance
  // family.
  #define VALIDATE_SPECIES(ofFamily, value) do {        \
    VALIDATE_OBJECT(ofSpecies, value);                  \
    if (get_species_instance_family(value) != ofFamily) \
      return new_signal(scValidationFailed);            \
    TRY(object_validate(value));                        \
  } while (false)

  // Generate validation for species.
#define VALIDATE_SPECIES_FIELD(Family, family) VALIDATE_SPECIES(of##Family, roots->family##_species);
  ENUM_OBJECT_FAMILIES(VALIDATE_SPECIES_FIELD)
#undef VALIDATE_SPECIES_FIELD

  // Validate singletons manually.
  VALIDATE_OBJECT(ofNull, roots->null);
  VALIDATE_OBJECT(ofBool, roots->thrue);
  VALIDATE_OBJECT(ofBool, roots->fahlse);

#define __VALIDATE_STRING_TABLE_ENTRY__(name, value) VALIDATE_OBJECT(ofString, roots->string_table.name);
  ENUM_STRING_TABLE(__VALIDATE_STRING_TABLE_ENTRY__)
#undef __VALIDATE_STRING_TABLE_ENTRY__

  #undef VALIDATE_TYPE
  #undef VALIDATE_SPECIES
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
  TRY(roots_validate(&runtime->roots));
  value_callback_t validate_callback;
  value_callback_init(&validate_callback, runtime_validate_object, NULL);
  TRY(heap_for_each_object(&runtime->heap, &validate_callback));
  return success();
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

value_t runtime_bool(runtime_t *runtime, bool which) {
  return which ? runtime->roots.thrue : runtime->roots.fahlse;
}
