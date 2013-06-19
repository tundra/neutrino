#include "alloc.h"
#include "behavior.h"
#include "runtime.h"
#include "value-inl.h"

value_t roots_init(roots_t *roots, runtime_t *runtime) {
  // The first few roots are tricky because they depend on each other. We have
  // to set them up manually rather than depend on it being done automatically
  // as with the rest of them.

  // First set up the meta-species which it its own species. We can't give it
  // behavior yet because we're not ready to create void-ps yet.
  TRY_DEF(meta, new_heap_species(runtime, ofSpecies, NULL));
  set_object_species(meta, meta);
  roots->species_species = meta;
  // Then set up the void-p species which is what we need for the behavior
  // pointers.
  TRY_DEF(void_p_species, new_heap_species(runtime, ofVoidP, NULL));
  roots->void_p_species = void_p_species;
  // Finally set up the behavior pointers manually.
  TRY_DEF(meta_behavior, new_heap_void_p(runtime, &kSpeciesBehavior));
  set_species_behavior(meta, meta_behavior);
  TRY_DEF(void_p_behavior, new_heap_void_p(runtime, &kVoidPBehavior));
  set_species_behavior(void_p_species, void_p_behavior);

  // The other species are straightforward.
  TRY_SET(roots->string_species, new_heap_species(runtime, ofString, &kStringBehavior));
  TRY_SET(roots->blob_species, new_heap_species(runtime, ofBlob, &kBlobBehavior));
  TRY_SET(roots->array_species, new_heap_species(runtime, ofArray, &kArrayBehavior));
  TRY_SET(roots->id_hash_map_species, new_heap_species(runtime, ofIdHashMap, &kIdHashMapBehavior));
  TRY_SET(roots->null_species, new_heap_species(runtime, ofNull, &kNullBehavior));
  TRY_SET(roots->bool_species, new_heap_species(runtime, ofBool, &kBoolBehavior));
  TRY_SET(roots->instance_species, new_heap_species(runtime, ofInstance, &kInstanceBehavior));

  // Singletons
  TRY_SET(roots->null, new_heap_null(runtime));
  TRY_SET(roots->thrue, new_heap_bool(runtime, true));
  TRY_SET(roots->fahlse, new_heap_bool(runtime, false));
  return success();
}

void roots_clear(roots_t *roots) {
  roots->species_species = success();
  roots->void_p_species = success();
  roots->string_species = success();
  roots->blob_species = success();
  roots->array_species = success();
  roots->id_hash_map_species = success();
  roots->null_species = success();
  roots->bool_species = success();
  roots->null = success();
  roots->thrue = success();
  roots->fahlse = success();
  roots->instance_species = success();
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
  VALIDATE_SPECIES(ofVoidP, roots->void_p_species);
  VALIDATE_SPECIES(ofString, roots->string_species);
  VALIDATE_SPECIES(ofBlob, roots->blob_species);
  VALIDATE_SPECIES(ofArray, roots->array_species);
  VALIDATE_SPECIES(ofIdHashMap, roots->id_hash_map_species);
  VALIDATE_SPECIES(ofNull, roots->null_species);
  VALIDATE_SPECIES(ofBool, roots->bool_species);
  VALIDATE_SPECIES(ofInstance, roots->instance_species);

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
