// The runtime structure which holds all the shared state of a single VM
// instance.

#include "heap.h"

#ifndef _RUNTIME
#define _RUNTIME

// A collection of all the root objects.
typedef struct {
  // The species of species (which is its own species).
  value_ptr_t species_species;
  // The species of the different value types.
  value_ptr_t string_species;
} roots_t;

// All the data associated with a single VM instance.
typedef struct {
  // The heap where all the data lives.
  heap_t heap;
  // The root objects.
  roots_t roots;
} runtime_t;

// Initializes the given runtime according to the given config.
value_ptr_t runtime_init(runtime_t *runtime, space_config_t *config);

// Disposes of the given runtime.
void runtime_dispose(runtime_t *runtime);

#endif // _RUNTIME
