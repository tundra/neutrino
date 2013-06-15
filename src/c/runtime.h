// The runtime structure which holds all the shared state of a single VM
// instance.

#include "heap.h"

#ifndef _RUNTIME
#define _RUNTIME

// A collection of all the root objects.
typedef struct {
  // The species of species (which is its own species).
  value_t species_species;
  // The species of the different value types.
  value_t string_species;
  value_t array_species;
  // Singletons
  value_t null;
} roots_t;


// All the data associated with a single VM instance.
typedef struct {
  // The heap where all the data lives.
  heap_t heap;
  // The root objects.
  roots_t roots;
} runtime_t;

// Initializes the given runtime according to the given config.
value_t runtime_init(runtime_t *runtime, space_config_t *config);

// Resets this runtime to a well-defined state such that if anything fails
// during the subsequent initialization all fields that haven't been
// initialized are sane.
void runtime_clear(runtime_t *runtime);

// Disposes of the given runtime.
void runtime_dispose(runtime_t *runtime);

// Returns this runtime's null value.
value_t runtime_null(runtime_t *runtime);


// Initialize this root set.
value_t roots_init(roots_t *roots, runtime_t *runtime);

// Clears all the fields to a well-defined value.
void roots_clear(roots_t *roots);

#endif // _RUNTIME
