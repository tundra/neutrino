#ifndef _PLANKTON
#define _PLANKTON

#include "runtime.h"
#include "value.h"

// Callback that can be used to resolve environment references.
typedef value_t (value_mapping_function_t)(value_t value, runtime_t *runtime,
    void *data);

// A function-like structure that maps values to other values.
typedef struct {
  // Callback used to map a value.
  value_mapping_function_t *function;
  // Extra data to pass to the callback.
  void *data;
} value_mapping_t;

// Maps a value using the given value mapping.
value_t value_mapping_apply(value_mapping_t *mapping, value_t value,
    runtime_t *runtime);

// Initializes a value mapping struct with the specified state.
void value_mapping_init(value_mapping_t *resolver, value_mapping_function_t function,
    void *data);

// Serializes an object graph into a plankton encoded binary blob. The resolver
// is used to decide which objects to fetch from the environment and which to
// serialize.
value_t plankton_serialize(runtime_t *runtime, value_mapping_t *resolver_or_null,
    value_t data);

// Plankton deserialize a binary blob containing a serialized object graph. The
// access mapping is used to acquire values from the environment.
value_t plankton_deserialize(runtime_t *runtime, value_mapping_t *access_or_null,
    value_t blob);

#endif // _PLANKTON
