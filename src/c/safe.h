// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Gc-safe references.


#ifndef _SAFE
#define _SAFE

#include "value.h"

// An object reference tracked by the runtime. These handles form a
// doubly-linked list such that nodes can add and remove themselves from the
// chain of all object trackers. Object trackers can only store heap objects
// since they're the only values that require tracking.
typedef struct object_tracker_t {
  // The pinned value.
  value_t value;
  // The next pin descriptor.
  struct object_tracker_t *next;
  // The previous pin descriptor.
  struct object_tracker_t *prev;
} object_tracker_t;

// An immutable gc-safe reference. Gc-safe references work much like values,
// they are tagged like the value they reference. Indeed, for non-objects a safe
// value is identical to the value itself. Only objects can move and so we only
// need to do anything for them. For objects a object_tracker_t is created to
// register a reference to the object with the runtime and instead of pointing
// directly to the object the safe reference points to that record.
//
// All this stuff is transparent to the user of handles though.
typedef struct safe_value_t {
  // The payload of the safe value encoded as a value_t. This is _not_ just the
  // value.
  value_t as_value;
} safe_value_t;

// "Cast" an object tracker a safe-value.
safe_value_t object_tracker_to_safe_value(object_tracker_t *tracker);

// "Cast" a safe-value to an object tracker.
object_tracker_t *safe_value_to_object_tracker(safe_value_t s_value);

// Make a safe value out of an immediate. The input must be known not to be an
// object.
safe_value_t protect_immediate(value_t immediate);

// Returns a safe value that is safe to use as an empty value.
safe_value_t empty_safe_value();

// Returns the immediate value stored in a safe reference. The value must be
// known not to be an object.
value_t deref_immediate(safe_value_t s_value);

// Is this safe-value an immediate?
bool safe_value_is_immediate(safe_value_t s_value);

// Returns the value stored in a safe value reference.
value_t deref(safe_value_t s_value);


// A pool of safe values that can be disposed together. This is not a scoped
// value and safe values can be allocated into this in any order.
typedef struct {
  // Array that holds this pool's values.
  safe_value_t *values;
  // The max number of values this pool can hold.
  size_t capacity;
  // The number of values currently held.
  size_t used;
  // The runtime this pool belongs to.
  runtime_t *runtime;
} safe_value_pool_t;

// Sets up a safe value pool so that it's ready to use. You usually don't
// want to call this explicitly but use the CREATE_SAFE_VALUE_POOL macro.
void safe_value_pool_init(safe_value_pool_t *pool, safe_value_t *values,
    size_t capacity, runtime_t *runtime);

// Disposes any safe values allocated into this safe value pool.
void safe_value_pool_dispose(safe_value_pool_t *pool);

// Protects the given value and adds it to the pool. Check fails if this pool
// is already full.
safe_value_t protect(safe_value_pool_t *pool, value_t value);

#endif // _SAFE
