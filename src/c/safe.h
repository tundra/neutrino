// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Gc-safe references.

#include "value.h"

#ifndef _SAFE
#define _SAFE

// Extra data associated with a protected reference. These handles form a
// doubly-linked list such that nodes can add and remove themselves from the
// chain of all safe objects. Protected references can only store heap objects,
// everything else must be encoded directly in the safe value reference.
typedef struct protected_reference_t {
  // The pinned value.
  value_t value;
  // The next pin descriptor.
  struct protected_reference_t *next;
  // The previous pin descriptor.
  struct protected_reference_t *prev;
} protected_reference_t;

// An immutable gc-safe reference. Gc-safe references work much like values,
// they are tagged like the value they reference. Indeed, for non-objects a safe
// value is identical to the value itself. Only objects can move and so we only
// need to do anything for them. For objects a protected_reference_t is created to
// register a reference to the object with the runtime and instead of pointing
// directly to the object the safe reference points to that record.
//
// All this stuff is transparent to the user of handles though, if you just do
// deref() you'll get the right value in both cases.
typedef struct safe_value_t {
  // This value represented as a value_t.
  value_t as_value;
} safe_value_t;

// "Cast" a gc safe object handle to a safe-value.
safe_value_t protected_reference_to_safe_value(protected_reference_t *reference);

// "Cast" a safe-value to a gc safe object handle.
protected_reference_t *safe_value_to_protected_reference(safe_value_t s_value);

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
