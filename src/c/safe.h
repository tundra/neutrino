//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Gc-safe references.

#ifndef _SAFE
#define _SAFE

#include "value.h"

// Flags set on object trackers that control how they behave.
typedef enum {
  // No special behavior.
  tfNone = 0x0,
  // This tracker should not keep the object alive. If the value becomes garbage
  // during the lifetime of the returned tracker, the tracker will be marked as
  // garbage and the reference to the value will be cleared to nothing.
  tfAlwaysWeak = 0x1,
  // When the object tracked by this tracker goes away the tracker itself should
  // be disposed. This really only makes sense if combined with some of the
  // other flags but it's treated orthogonally because it can be and to make
  // testing easier without involving a bunch of other flags.
  tfSelfDestruct = 0x2,
  // When the object tracked by this tracker becomes garbage invoke the object's
  // finalizer. Note that creating multiple finalizing trackers for the same
  // object will cause the finalizer to be called for each tracker so you either
  // need to ensure that finalization is idempotent or that only one tracker is
  // created for an object. Selbstdisziplin haben!
  tfFinalize = 0x4,
  // This reference may or may not be weak, depending on the state of the object
  // in question. The predicate used to determine whether the value is weak is
  // passed along in the constructor. Note that only the thread that runs the
  // runtime, that is the same thread the executes gcs, is allowed to modify
  // the state that determines whether a value is weak. It's not an assumption
  // the runtime uses but if other threads start manipulating the state while
  // a gc is running they may think the value is strong when the gc believes it
  // is weak and kill the value, hence invalidating the other thread's
  // assumptions.
  tfMaybeWeak = 0x8
} object_tracker_flags_t;

// Flags set by the gc on object trackers that indicate their current state.
typedef enum {
  tsGarbage = 0x1
} object_tracker_state_t;

// An object reference tracked by the runtime. These handles form a
// doubly-linked list such that nodes can add and remove themselves from the
// chain of all object trackers. Object trackers can only store heap objects
// since they're the only values that require tracking.
typedef struct object_tracker_t {
  // The pinned value.
  value_t value;
  // Flags that control how the tracker behaves.
  uint32_t flags;
  // Flags that indicate the current state of the tracker.
  uint32_t state;
  // The next pin descriptor.
  struct object_tracker_t *next;
  // The previous pin descriptor.
  struct object_tracker_t *prev;
} object_tracker_t;

// Returns true iff the given object tracker is an always-weak reference. Note
// that even if the tracker isn't always-weak it may still be weak temporarily
// at this moment.
bool object_tracker_is_always_weak(object_tracker_t *tracker);

// Returns if the given tracker may or may not be weak at any given time.
bool object_tracker_is_maybe_weak(object_tracker_t *tracker);

// Returns true if the given tracker represented an object that has now become
// garbage.
bool object_tracker_is_garbage(object_tracker_t *tracker);

typedef enum {
  // The weakness state hasn't been determined.
  wsUnknown,
  // The reference is weak.
  wsWeak,
  // The reference isn't weak.
  wsStrong
} weakness_state_t;

// Callback used to determine whether a maybe-weak value is weak at this
// particular point in time.
typedef bool (is_weak_function_t)(value_t value, void *data);

// An object tracker with additional info about weakness.
typedef struct {
  // Basic object tracker state.
  object_tracker_t base;
  // Weakness gets determined once before the gc proper begins since at that
  // point the heap is still consistent which makes everything simpler.
  weakness_state_t weakness;
  // Callback to use to determine weakness.
  is_weak_function_t *is_weak;
  void *is_weak_data;
} maybe_weak_object_tracker_t;

// If the given tracker is maybe-weak, returns the maybe-weak view of it.
// Otherwise NULL.
maybe_weak_object_tracker_t *maybe_weak_object_tracker_from(
    object_tracker_t *tracker);

// Returns true iff the given object tracker is currently weak. If the tracker
// is maybe-weak this requires that its weakness has been determined previously.
bool object_tracker_is_currently_weak(object_tracker_t *tracker);

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

// Is this value one that can be stored as an immediate safe value?
bool value_is_immediate(value_t value);

// Returns the value stored in a safe value reference.
value_t deref(safe_value_t s_value);

// Returns true iff the given safe value was a weak reference to a value that
// has not been garbage collected.
bool safe_value_is_garbage(safe_value_t s_value);

// Does the given safe value wrap nothing?
bool safe_value_is_nothing(safe_value_t s_value);

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
