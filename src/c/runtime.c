#include "alloc.h"
#include "behavior.h"
#include "runtime.h"
#include "value-inl.h"

value_t roots_init(roots_t *roots, runtime_t *runtime) {
  // First set up the meta-species which it its own species.
  TRY_DEF(meta, new_heap_species(runtime, ofSpecies, &kSpeciesBehavior));
  set_object_species(meta, meta);
  roots->species_species = meta;

  // The other species are straightforward.
  TRY_SET(roots->string_species, new_heap_species(runtime, ofString, &kStringBehavior));
  TRY_SET(roots->array_species, new_heap_species(runtime, ofArray, &kArrayBehavior));
  TRY_SET(roots->map_species, new_heap_species(runtime, ofMap, &kMapBehavior));
  TRY_SET(roots->null_species, new_heap_species(runtime, ofNull, &kNullBehavior));
  TRY_SET(roots->bool_species, new_heap_species(runtime, ofBool, &kBoolBehavior));

  // Singletons
  TRY_SET(roots->null, new_heap_null(runtime));
  TRY_SET(roots->thrue, new_heap_bool(runtime, true));
  TRY_SET(roots->fahlse, new_heap_bool(runtime, false));
  return success();
}

void roots_clear(roots_t *roots) {
  roots->species_species = success();
  roots->string_species = success();
  roots->array_species = success();
  roots->map_species = success();
  roots->null_species = success();
  roots->bool_species = success();
  roots->null = success();
  roots->thrue = success();
  roots->fahlse = success();
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

  VALIDATE_SPECIES(ofSpecies, roots->species_species);
  VALIDATE_SPECIES(ofString, roots->string_species);
  VALIDATE_SPECIES(ofArray, roots->array_species);
  VALIDATE_SPECIES(ofMap, roots->map_species);
  VALIDATE_SPECIES(ofNull, roots->null_species);
  VALIDATE_SPECIES(ofBool, roots->bool_species);

  VALIDATE_OBJECT(ofNull, roots->null);
  VALIDATE_OBJECT(ofBool, roots->thrue);
  VALIDATE_OBJECT(ofBool, roots->fahlse);

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

value_t runtime_validate(runtime_t *runtime) {
  return roots_validate(&runtime->roots);
}

void runtime_clear(runtime_t *runtime) {
  roots_clear(&runtime->roots);
}

void runtime_dispose(runtime_t *runtime) {
  heap_dispose(&runtime->heap);
}

value_t runtime_null(runtime_t *runtime) {
  return runtime->roots.null;
}

value_t runtime_bool(runtime_t *runtime, bool which) {
  return which ? runtime->roots.thrue : runtime->roots.fahlse;
}
