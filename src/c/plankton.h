#ifndef _PLANKTON
#define _PLANKTON

#include "value.h"

// Serializes an object graph into a plankton encoded binary blob.
value_t plankton_serialize(value_t data);

// Plankton deserialize a binary blob containing a serialized object graph.
value_t plankton_deserialize(value_t blob);

#endif // _PLANKTON
