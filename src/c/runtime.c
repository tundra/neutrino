#include "runtime.h"

// Initializes the given runtime according to the given config.
value_ptr_t runtime_init(runtime_t *runtime, space_config_t *config) {
  TRY(heap_init(&runtime->heap, config));
  return non_signal();
}

// Disposes of the given runtime.
void runtime_dispose(runtime_t *runtime) {
  heap_dispose(&runtime->heap);
}
