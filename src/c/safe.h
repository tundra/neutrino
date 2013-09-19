// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Gc-safe references.

#include "value.h"

#ifndef _SAFE
#define _SAFE

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

// Returns the immediate value stored in a safe reference. The value must be
// known not to be an object.
value_t deref_immediate(safe_value_t s_value);

// Is this safe-value an immediate?
bool safe_value_is_immediate(safe_value_t s_value);

// Returns the value stored in a safe value reference.
value_t deref(safe_value_t s_value);

#endif // _SAFE
