//- Copyright 2014 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

/// # Value freezing
///
/// Any value can be in one of four modes. These modes indicate which operations
/// are legal. A value starts out least restricted and can then move towards
/// more restrictions, never fewer. The modes are:
///
///   - _Fluid_: Any changes can be made to this object, including changing
///      which type it belongs to and which global fields it has.
///   - _Mutable_: The object's fields can be set but no changes can be made to
///      which global fields exist or which primary type it has.
///   - _Frozen_: The object cannot be changed but can reference other objects
///     that can.
///   - _Deep frozen_: The object cannot change and neither can any objects it
///     references.
///
/// Not all values can be in all states. For instance, integers and strings
/// start out deep frozen so the less restricted states don't apply to them.
///
/// You can explicitly move an object to the mutable or frozen mode if you know
/// the object is in a less restricted mode but you can't make it deep frozen,
/// and you don't need to. Being deep frozen is a property of a full object
/// graph so you can ask if an object is deep frozen and the object graph will
/// be traversed for you to determine whether it is. That traversal may cause
/// the object to be marked as deep frozen.
///
/// ### Ownership
///
/// Some values are considered to logically *own* other values. For instance,
/// an id hash map owns its entry array, an array buffer owns it storage array,
/// and the roots object owns all the roots. Basically, if creating one object
/// requires another object to be created that isn't passed to it from elsewhere
/// then the object is owned. The place you see this is mainly around freezing.
/// If you freeze an object (not shallow-freeze, freeze) then the object is
/// responsible for freezing any objects it owns. You can think of it as a
/// matter of encapsulation. If an object needs some other objects to function
/// which it otherwise doesn't expose to you then that's that object's business
/// and it should be transparent when freezing it that those other objects
/// exist. If freezing it didn't recursively freeze those you could tell they
/// existed because they would prevent it from being deep frozen.
///
/// Ownership is strictly linear: if object _a_ owns object _b_ then _b_ may
/// itself own other objects, but it must not be the case that _b_ or something
/// transitively owned by _b_ considers itself to own _a_.
///
/// ### Cheating
///
/// The deep freezing infrastructure is based on the same object-layout
/// inspection code that the gc uses which means that except for ownership
/// families don't need explicit support for deciding whether an object is deep
/// frozen. This also means that you can't easily decide to cheat for a
/// particular field and claim that it is deep frozen when it isn't, because
/// the deep freezing code doesn't know what the fields mean, it just knows that
/// whatever they are they must be frozen. To deal with this there's a separate
/// `ofFreezeCheat` family which claims to be deep frozen but in reality can be
/// mutated. Use with caution.

#ifndef _FREEZE
#define _FREEZE

#include "value.h"

// Returns true iff the given value is in a state where it can be mutated.
bool is_mutable(value_t value);

// Returns true iff the given value is in a frozen, though not necessarily deep
// frozen, state.
bool is_frozen(value_t value);

// Returns true if the value has already been validated to be deep frozen. Note
// that this is not for general use, you almost always want to use one of the
// validate functions if you depend on the result for anything but sanity
// checking.
bool peek_deep_frozen(value_t value);

// Ensures that the value is in a frozen state. Since being frozen is the most
// restrictive mode this cannot fail except if freezing an object requires
// interacting with the runtime (for instance allocating a value) and that
// interaction fails. Note that this only freezes the immediate object, if it
// has any references including owned references (for instance the entry array
// in an id hash map) they will not be frozen by this.
value_t ensure_shallow_frozen(runtime_t *runtime, value_t value);

// Ensures that the value as well as any owned references (for instance the
// entry array in an id hash map) is in a frozen state. This does not mean that
// the value becomes deep frozen, it may have references to non-owned mutable
// values. For instance, an array is not considered to own any of its elements.
value_t ensure_frozen(runtime_t *runtime, value_t value);

// If the given value is deep frozen, returns true. If it is not attempts to
// make it deep frozen, that it, traverses the objects reachable and checking
// whether they're all frozen or deep frozen and marking them as deep frozen as
// we go. If this succeeds returns true, otherwise false.
//
// If validation fails and the offender_out parameter is non-null, an arbitrary
// mutable object from the object graph will be stored there. This is a
// debugging aid and since it's arbitrary which object will be stored you should
// not depend on the particular value in any way.
//
// This is the only reliable way to check whether a value is deep frozen since
// being deep frozen is a property of an object graph, not an individual object,
// and using marking like this is the only efficient way to reliably determine
// that property.
bool try_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out);

// Works the same way as try_validate_deep_frozen but returns a non-condition
// instead of true and a condition for false. Depending on what the most
// convenient interface is you can use either this or the other, they do the
// same thing.
value_t validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out);

typedef enum {
  mfShallow = 0x0,
  mfFreezeValues = 0x1,
  mfFreezeKeys = 0x2,
  mfFreezeKeysAndValues = 0x3
} id_hash_map_freeze_mode_t;

// Ensures that an id hash map is frozen. If you pass mfShallow this works the
// same way as just ensure_freezing a map value but the other modes will also
// freeze the values and/or keys -- shallowly though, to freeze deeper through
// them you have to write custom code.
value_t ensure_id_hash_map_frozen(runtime_t *runtime, value_t value,
    id_hash_map_freeze_mode_t mode);

/// ## Freeze cheat
///
/// At least for now we need a way to cheat the freezing infrastructure such
/// that there is mutable state referenced directly from deep frozen objects.
/// A freeze cheat accomplishes that: it is a deep frozen reference that allows
/// you to set its value at any time. If there is _any_ way for you to avoid
/// using this, do.

static const size_t kFreezeCheatSize = HEAP_OBJECT_SIZE(1);
static const size_t kFreezeCheatValueOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The raw value currently held by this freeze cheat. The setter obviously
// doesn't check mutability since that's the whole point of freeze cheats so
// you can use it directly rather than the init_frozen_ method you'd usually
// use.
ACCESSORS_DECL(freeze_cheat, value);


#endif // _FREEZE
