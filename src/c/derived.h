// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Derived object types


#ifndef _DERIVED
#define _DERIVED

#include "behavior.h"
#include "check.h"
#include "utils.h"
#include "value.h"


/// ## Derived objects

// A description of the behavior and layout of a genus.
typedef struct {
  // The genus being described.
  derived_object_genus_t genus;
  // The number of fields of this genus include the anchor.
  size_t field_count;
  // The number of fields before the anchor.
  size_t before_field_count;
  // The number of fields after the anchor.
  size_t after_field_count;
  // Function for validating this genus.
  value_t (*validate)(value_t value);
  // Writes a string representation of the value on a string buffer.
  void (*print_on)(value_t value, print_on_context_t *context);
  // Perform the on-scope-exit action associated with this derived object. If
  // this family is not scoped the value is NULL. It's a bit of a mess if these
  // can fail since the barrier gets unhooked from the chain before we call this
  // so they should always succeed, otherwise use a full code block.
  void (*on_scope_exit)(value_t self);
} genus_descriptor_t;

// Returns the behavior struct that describes the given genus.
genus_descriptor_t *get_genus_descriptor(derived_object_genus_t genus);

// Converts a pointer to a derived object into an tagged derived object value
// pointer.
static value_t new_derived_object(address_t addr) {
  value_t result = pointer_to_value_bit_cast(addr + vdDerivedObject);
  CHECK_DOMAIN(vdDerivedObject, result);
  return result;
}

static address_t get_derived_object_address(value_t value) {
  CHECK_DOMAIN_HOT(vdDerivedObject, value);
  address_t tagged_address = (address_t) value_to_pointer_bit_cast(value);
  return tagged_address - ((address_arith_t) vdDerivedObject);
}

// Number of bytes in a derived object header.
#define kDerivedObjectHeaderSize (1 * kValueSize)

// Returns the size in bytes of a derived object with N fields, where the header
// is not counted as a field.
#define DERIVED_OBJECT_SIZE(N) ((N * kValueSize) + kDerivedObjectHeaderSize)

// Returns the number of fields in a derived object with N fields, where the header
// is not counted as a field.
#define DERIVED_OBJECT_FIELD_COUNT(N) (N)

// Returns the offset of the N'th field in a derived object. The 0'th field is
// the anchor.
#define DERIVED_OBJECT_FIELD_OFFSET(N) (N * kValueSize)

// The offset of the derived object anchor.
static const size_t kDerivedObjectAnchorOffset = 0 * kValueSize;

// Returns a pointer to the index'th field in the given derived object. This is
// a hot operation so it's a macro.
#define access_derived_object_field(VALUE, INDEX)                              \
  ((value_t*) (((address_t) get_derived_object_address(VALUE)) + ((size_t) (INDEX))))

static void set_derived_object_anchor(value_t self, value_t value) {
  *access_derived_object_field(self, kDerivedObjectAnchorOffset) = value;
}

static value_t get_derived_object_anchor(value_t self) {
  return *access_derived_object_field(self, kDerivedObjectAnchorOffset);
}

// Returns the genus of the given value which must be a derived object.
derived_object_genus_t get_derived_object_genus(value_t self);

// Returns the host that contains the given derived object.
value_t get_derived_object_host(value_t self);

// Returns the string name of the given genus.
const char *get_derived_object_genus_name(derived_object_genus_t genus);


/// ## Stack pointer

#define kStackPointerBeforeFieldCount 0
#define kStackPointerAfterFieldCount 0


/// ## Barrier state
///
/// Barrier state is present in most of the sections below. It occupies the
/// fields immediately before the anchor:
///
//%       :    ...     :
//%       +------------+ ---+
//%       |  previous  |    |
//%       +------------+    | barrier state
//%       |  payload   |    |
//%       +============+ ---+
//%       |   anchor   | <----- derived
//%       +============+
//%       :    ...     :
///
/// A barrier consists of three parts: the anchor identifies the type of the
/// barrier which is what controls what happens when we leave the barrier. The
/// previous pointer points to the previous barrier. The payload is some extra
/// data that can be used during barrier exit in whatever way the handler wants.
/// For escape barriers, for instance, it is the escape object to kill.

#define kBarrierStateFieldCount 2
static const size_t kBarrierStatePayloadOffset = DERIVED_OBJECT_FIELD_OFFSET(-1);
static const size_t kBarrierStatePreviousOffset = DERIVED_OBJECT_FIELD_OFFSET(-2);

// The data field in a barrier.
ACCESSORS_DECL(barrier_state, payload);

// The previous barrier.
ACCESSORS_DECL(barrier_state, previous);

FORWARD(frame_t);

// Completes the initialization of a barrier state and registers it as the top
// frame on the stack.
void barrier_state_register(value_t self, value_t stack, value_t payload);

// Unregisters the top barrier state.
void barrier_state_unregister(value_t self, value_t stack);


/// ## Escape state
///
/// State used by escapes. Escape state makes up escape sections but are also
/// part of other sections. Like barrier state (which is part of escape state)
/// everything is before the anchor:
///
//%       :    ...     :
//%       +============+ ---+
//%       :            :    |
//%       :   exec     :    |
//%       :   state    :    |
//%       :            :    | escape state
//%       +------------+    |
//%       |            |    |
//%       +- barrier  -+    |
//%       |            |    |
//%       +============+ ---+
//%       |   anchor   | <----- derived
//%       +============+
//%       :    ...     :

#define kEscapeStateFieldCount (kBarrierStateFieldCount + 5)
static const size_t kEscapeStateStackPointerOffset = DERIVED_OBJECT_FIELD_OFFSET(-3);
static const size_t kEscapeStateFramePointerOffset = DERIVED_OBJECT_FIELD_OFFSET(-4);
static const size_t kEscapeStateLimitPointerOffset = DERIVED_OBJECT_FIELD_OFFSET(-5);
static const size_t kEscapeStateFlagsOffset = DERIVED_OBJECT_FIELD_OFFSET(-6);
static const size_t kEscapeStatePcOffset = DERIVED_OBJECT_FIELD_OFFSET(-7);

// The recorded stack pointer.
ACCESSORS_DECL(escape_state, stack_pointer);

// The recorded frame pointer.
ACCESSORS_DECL(escape_state, frame_pointer);

// The recorded limit pointer.
ACCESSORS_DECL(escape_state, limit_pointer);

// The frame's flags.
ACCESSORS_DECL(escape_state, flags);

// The pc to return to.
ACCESSORS_DECL(escape_state, pc);

// Initializes the complete escape state of a section.
void escape_state_init(value_t self, size_t stack_pointer, size_t frame_pointer,
    size_t limit_pointer, value_t flags, size_t pc);


/// ## Escape section
///
/// An escape section is the data associated with an escape object. It consists
/// just of a block of escape state.

#define kEscapeSectionBeforeFieldCount kEscapeStateFieldCount
#define kEscapeSectionAfterFieldCount 0


/// ## Refraction point
///
/// State related to refraction, like barrier state present in most of the
/// control-related derived objects. Comes immediately after the anchor.
///
//%       :    ...     :
//%       +============+
//%       |   anchor   | <----- derived
//%       +============+ +-+
//%       |    fp      |   | refraction point
//%       +------------+ --+
//%       :    ...     :

#define kRefractionPointFieldCount 1
static const size_t kRefractionPointFramePointerOffset = DERIVED_OBJECT_FIELD_OFFSET(1);

// The containing frame's frame pointer.
ACCESSORS_DECL(refraction_point, frame_pointer);

// Initializes the given refraction point such that it refracts for the given
// frame.
void refraction_point_init(value_t self, frame_t *frame);


/// ## Ensure section

#define kEnsureSectionBeforeFieldCount kBarrierStateFieldCount
#define kEnsureSectionAfterFieldCount kRefractionPointFieldCount


/// ## Block section

#define kBlockSectionBeforeFieldCount kBarrierStateFieldCount
#define kBlockSectionAfterFieldCount kRefractionPointFieldCount + 1
static const size_t kBlockSectionMethodspaceOffset = DERIVED_OBJECT_FIELD_OFFSET(2);

ACCESSORS_DECL(block_section, methodspace);


/// ## Signal handler section

#define kSignalHandlerSectionBeforeFieldCount kEscapeStateFieldCount
#define kSignalHandlerSectionAfterFieldCount kRefractionPointFieldCount


/// ## Allocation

// Returns a new stack pointer value within the given memory.
value_t new_derived_stack_pointer(runtime_t *runtime, value_array_t memory,
    value_t host);

// Allocates a new derived object in the given block of memory and initializes
// it with the given genus and host but requires the caller to complete
// initialization.
//
// Beware that the "size" is not a size in bytes, unlike other allocation
// functions, it is the number of value-size fields of the object.
value_t alloc_derived_object(value_array_t memory, genus_descriptor_t *desc,
    value_t host);


/// ## Genus descriptors

#define __GENUS_STRUCT__(Genus, genus, SC)                                     \
value_t genus##_validate(value_t value);                                       \
void genus##_print_on(value_t value, print_on_context_t *context);             \
SC(void on_##genus##_exit(value_t value);,)
ENUM_DERIVED_OBJECT_GENERA(__GENUS_STRUCT__)
#undef __GENUS_STRUCT__
extern genus_descriptor_t kGenusDescriptors[kDerivedObjectGenusCount];

// Accessor for the descriptor corresponding to the given genus. We need these
// pretty often so it's convenient that access is really cheap.
#define get_genus_descriptor(GENUS) (&kGenusDescriptors[GENUS])

#endif // _DERIVED
