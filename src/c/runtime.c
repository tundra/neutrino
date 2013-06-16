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
  TRY_SET(roots->null_species, new_heap_species(runtime, ofNull, &kNullBehavior));

  // Singletons
  TRY_SET(roots->null, new_heap_null(runtime));
  return non_signal();
}

void roots_clear(roots_t *roots) {
  roots->species_species = non_signal();
  roots->string_species = non_signal();
  roots->array_species = non_signal();
  roots->null_species = non_signal();
  roots->null = non_signal();
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
  VALIDATE_SPECIES(ofNull, roots->null_species);

  VALIDATE_OBJECT(ofNull, roots->null);

  #undef VALIDATE_TYPE
  #undef VALIDATE_SPECIES
  return non_signal();
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
