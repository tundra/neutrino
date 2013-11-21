// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// The basic types of values.


#ifndef _VALUE
#define _VALUE

#include "check.h"
#include "globals.h"

FORWARD(blob_t);
FORWARD(cycle_detector_t);
FORWARD(hash_stream_t);
FORWARD(runtime_t);
FORWARD(string_t);
FORWARD(string_buffer_t);

// Value domain identifiers.
typedef enum {
  // Tagged integer.
  vdInteger = 0,
  // Pointer to a heap object.
  vdObject = 1,
  // A VM-internal signal.
  vdSignal = 2,
  // An object that has been moved during an in-process garbage collection.
  vdMovedObject = 3
} value_domain_t;

// Returns the string name of the given domain.
const char *get_value_domain_name(value_domain_t domain);

// Invokes the given macro for each signal cause.
#define ENUM_SIGNAL_CAUSES(F)                                                  \
  F(Circular)                                                                  \
  F(EmptyPath)                                                                 \
  F(HeapExhausted)                                                             \
  F(InternalFamily)                                                            \
  F(InvalidCast)                                                               \
  F(InvalidInput)                                                              \
  F(InvalidModeChange)                                                         \
  F(InvalidSyntax)                                                             \
  F(LookupError)                                                               \
  F(MapFull)                                                                   \
  F(NotComparable)                                                             \
  F(NotFound)                                                                  \
  F(NotDeepFrozen)                                                             \
  F(Nothing)                                                                   \
  F(OutOfBounds)                                                               \
  F(OutOfMemory)                                                               \
  F(SystemError)                                                               \
  F(UnsupportedBehavior)                                                       \
  F(ValidationFailed)                                                          \
  F(SafePoolFull)                                                              \
  F(Wat)

// Enum identifying the type of a signal.
typedef enum {
  __scFirst__ = -1
#define DECLARE_SIGNAL_CAUSE_ENUM(Cause) , sc##Cause
  ENUM_SIGNAL_CAUSES(DECLARE_SIGNAL_CAUSE_ENUM)
#undef DECLARE_SIGNAL_CAUSE_ENUM
} signal_cause_t;


// Data for a tagged integer value.
typedef struct {
  value_domain_t domain : 3;
  int64_t value : 61;
} integer_value_t;

// Data for any value.
typedef struct {
  value_domain_t domain : 3;
  uint64_t payload : 61;
} unknown_value_t;

// Data for signals.
typedef struct {
  value_domain_t domain : 3;
  signal_cause_t cause : 8;
  uint32_t details;
} signal_value_t;

// The data type used to represent a raw encoded value.
typedef uint64_t encoded_value_t;

// A tagged runtime value.
typedef union value_t {
  unknown_value_t as_unknown;
  integer_value_t as_integer;
  signal_value_t as_signal;
  encoded_value_t encoded;
} value_t;

// How many bits are used for the domain tag?
static const int kDomainTagSize = 3;
static const int kDomainTagMask = 0x7;

// Alignment that ensures that object pointers have tag 0.
#define kValueSize sizeof(value_t)

// Returns the tag from a tagged value.
static value_domain_t get_value_domain(value_t value) {
  return value.as_unknown.domain;
}

// Are the two given values identically the same. This is stronger than object
// identity, for instance two different strings with the same contents would be
// object identical but not the same value.
static bool is_same_value(value_t a, value_t b) {
  return a.encoded == b.encoded;
}


// --- I n t e g e r ---

// Returns true if the given value can be stored as a tagged integer.
static bool fits_as_tagged_integer(int64_t value) {
  // We store signed values as integers by shifting away the three most
  // significant bits, keeping the sign bit, and restore them by shifting the
  // other way with sign extension. This only works if the three most significant
  // bits are the same as the sign bit, in other words only if the four most
  // significant bits are the same: 0b1111... or 0b0000.... The way we recognize
  // whether this is the case is to add 0b0001 to the top nibble, which in the
  // case we want to recognize yields a top nibble of 0b0000 and 0b0001 (that
  // is, a number of the form 0x0............... and 0x1...............). This
  // relies on 0b1111+0b0001 overflowing to 0b0000. The largest possible value
  // of this form is if the following part is all 1s, 0x1FFFFFFFFFFFFFFF, so if
  // the value is that or smaller then we know the value will fit. We test that
  // by checking if it's strictly smaller than the next value, 0x2000000000000000.
  uint64_t uvalue = value;
  return (uvalue + 0x1000000000000000) < 0x2000000000000000;
}

// Creates a new tagged integer with the given value.
static value_t new_integer(int64_t value) {
  CHECK_TRUE("new integer overflow", fits_as_tagged_integer(value));
  return (value_t) {.as_integer={vdInteger, value}};
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_t value) {
  CHECK_DOMAIN(vdInteger, value);
  return value.as_integer.value;
}

// Returns a new tagged integer which is the sum of the two given tagged
// integers.
static value_t add_integers(value_t a, value_t b) {
  return new_integer(get_integer_value(a) + get_integer_value(b));
}

// Returns a new tagged integer which is the first of the given tagged integers
// minus the second.
static value_t sub_integers(value_t a, value_t b) {
  return new_integer(get_integer_value(a) - get_integer_value(b));
}

// Returns a value that is _not_ a signal. This can be used to indicate
// unspecific success.
static value_t success() {
  return new_integer(0);
}

// An arbitrary non-signal value which can be used instead of success to
// indicate that the concrete value doesn't matter.
static value_t whatever() {
  return new_integer(1);
}

// Returns a value representing the present stage.
static value_t present_stage() {
  return new_integer(0);
}

// Returns a value representing the past stage.
static value_t past_stage() {
  return new_integer(-1);
}

// Returns a value representing the past stage.
static value_t past_past_stage() {
  return new_integer(-2);
}

// Shifts a stage index being looked up through an imported stage index. Going
// through a non-present import will shift which stage you're looking for and
// this function tells you what the new stage should be. If, for instance,
// you're looking up @foo:X through import $foo you want to continue looking for
// @X in $foo, whereas if you're looking for @foo:X in @foo you want to shift to
// looking for $X. Hence shifting @ by $ yields @, shifting @ by @ yields $,
// etc.
static value_t shift_lookup_through_import(value_t import, value_t stage) {
  return sub_integers(stage, import);
}

// Shifts a fragment index through an imported stage index. Importing a module
// non-presently will shift the dependencies between modules such that, for
// instance, if you import @x then you'll want to import $x in your past, and
// @x in your past^2. That's what importing @x means. This function tells you,
// given the point at which you're importing and a stage being imported, which
// fragment of yours should the fragment being imported be imported into..
static value_t shift_fragment_through_import(value_t import, value_t stage) {
  return add_integers(import, stage);
}

// Creates an internal boolean with the given value. Note that this is purely
// for runtime internal use and does not yield the true/false boolean values
// used by the surface language.
static value_t to_internal_boolean(bool value) {
  return new_integer(value);
}

// Returns a non-signal value that can be used to signify "true". Note that this
// is purely for runtime internal use and not the true boolean value used by the
// surface language.
static value_t internal_true_value() {
  return to_internal_boolean(true);
}

// Returns a non-signal value that can be used to signify "false". Note that
// this is purely for runtime internal use and not the false boolean value used
// by the surface language.
static value_t internal_false_value() {
  return to_internal_boolean(false);
}

// Is the given value the runtime internal true value. This does not identify
// the surface language boolean true value, this is for internal use.
static bool is_internal_true_value(value_t value) {
  return (value.as_unknown.domain == vdInteger) && !!value.as_integer.value;
}


// --- M o v e d   o b j e c t ---

// Given an encoded value, returns the corresponding value_t struct.
static value_t decode_value(encoded_value_t value) {
  return (value_t) {.encoded=value};
}

// Given a moved object forward pointer returns the object this object has been
// moved to.
static value_t get_moved_object_target(value_t value) {
  CHECK_DOMAIN(vdMovedObject, value);
  value_t target = decode_value(value.encoded - (vdMovedObject-vdObject));
  CHECK_DOMAIN(vdObject, target);
  return target;
}

// Creates a new moved object pointer pointing to the given target object.
static value_t new_moved_object(value_t target) {
  CHECK_DOMAIN(vdObject, target);
  value_t moved = decode_value(target.encoded + (vdMovedObject-vdObject));
  CHECK_DOMAIN(vdMovedObject, moved);
  return moved;
}


// --- O b j e c t ---

// Indicates that a family has a particular attribute.
#define X(T, F) T

// Indicates that a family does not have a particular attribute.
#define _(T, F) F

// The columns are,
//
//   - CamelName: the name of the family in upper camel case.
//   - underscore_name: the name of the family in lower underscore case.
//   - Cm: do the values support ordered comparison?
//   - Id: do the values have a custom identity comparison function?
//   - Pt: can this be read from plankton?
//   - Sr: is this type exposed to the surface language?
//   - Nl: does this type have a nontrivial layout, either non-value fields or
//       variable size.
//   - Fu: does this type require a post-migration fixup during garbage
//       collection?
//   - Em: is this family a syntax tree that can be emitted as code? Not all
//       syntax tree nodes can be emitted.
//   - Md: is this family in the modal division, that is, is the mutability
//       state stored in the species?
//   - Ow: do objects of this family use some other objects in their
//       implementation that they own?
//
// CamelName            underscore_name                 Cm Id Pt Sr Nl Fu Em Md Ow

// Enumerates the special species, the ones that require special handling during
// startup.
#define ENUM_SPECIAL_OBJECT_FAMILIES(F)                                           \
  F(Species,                 species,                   _, _, _, _, X, _, _, X, _)

// Enumerates the compact object species.
#define ENUM_OTHER_OBJECT_FAMILIES(F)                                             \
  F(ArgumentAst,             argument_ast,              _, _, X, X, _, _, _, _, _)\
  F(ArgumentMapTrie,         argument_map_trie,         _, _, _, _, _, _, _, X, X)\
  F(Array,                   array,                     _, X, _, X, X, _, _, X, _)\
  F(ArrayAst,                array_ast,                 _, _, X, X, _, _, X, _, _)\
  F(ArrayBuffer,             array_buffer,              _, _, _, X, _, _, _, X, X)\
  F(Blob,                    blob,                      _, _, _, X, X, _, _, _, _)\
  F(Boolean,                 boolean,                   X, _, _, X, _, _, _, _, _)\
  F(CodeBlock,               code_block,                _, _, _, _, _, _, _, X, X)\
  F(Ctrino,                  ctrino,                    _, _, _, X, _, _, _, _, _)\
  F(Factory,                 factory,                   _, _, _, _, _, _, _, _, _)\
  F(Function,                function,                  _, _, X, X, _, _, _, X, _)\
  F(Guard,                   guard,                     _, _, _, _, _, _, _, X, _)\
  F(GuardAst,                guard_ast,                 _, _, X, X, _, _, _, _, _)\
  F(Identifier,              identifier,                X, X, X, _, _, _, _, _, _)\
  F(IdHashMap,               id_hash_map,               _, _, _, X, _, X, _, X, X)\
  F(Instance,                instance,                  _, _, X, X, _, _, _, _, _)\
  F(InvocationAst,           invocation_ast,            _, _, X, X, _, _, X, _, _)\
  F(InvocationRecord,        invocation_record,         _, _, _, _, _, _, _, X, X)\
  F(Key,                     key,                       X, _, _, X, _, _, _, X, _)\
  F(Lambda,                  lambda,                    _, _, _, X, _, _, _, X, X)\
  F(LambdaAst,               lambda_ast,                _, _, X, X, _, _, X, _, _)\
  F(Library,                 library,                   _, _, X, _, _, _, _, _, _)\
  F(LiteralAst,              literal_ast,               _, _, X, X, _, _, X, _, _)\
  F(LocalDeclarationAst,     local_declaration_ast,     _, _, X, X, _, _, X, _, _)\
  F(LocalVariableAst,        local_variable_ast,        _, _, X, X, _, _, X, _, _)\
  F(Method,                  method,                    _, _, _, _, _, _, _, X, _)\
  F(MethodAst,               method_ast,                _, _, X, X, _, _, _, X, _)\
  F(MethodDeclarationAst,    method_declaration_ast,    _, _, X, _, _, _, _, _, _)\
  F(Methodspace,             methodspace,               _, _, _, _, _, _, _, X, X)\
  F(Module,                  module,                    _, _, _, _, _, _, _, _, _)\
  F(ModuleFragment,          module_fragment,           _, _, _, X, _, _, _, X, _)\
  F(ModuleLoader,            module_loader,             _, _, _, _, _, _, _, _, _)\
  F(MutableRoots,            mutable_roots,             _, _, _, _, _, _, _, X, X)\
  F(Namespace,               namespace,                 _, _, _, _, _, _, _, X, X)\
  F(NamespaceDeclarationAst, namespace_declaration_ast, _, _, X, _, _, _, _, _, _)\
  F(NamespaceVariableAst,    namespace_variable_ast,    _, _, X, X, _, _, X, _, _)\
  F(Nothing,                 nothing,                   _, _, _, _, _, _, _, _, _)\
  F(Null,                    null,                      _, _, _, X, _, _, _, _, _)\
  F(Operation,               operation,                 _, X, X, _, _, _, _, X, _)\
  F(Options,                 options,                   _, _, X, _, _, _, _, _, _)\
  F(Parameter,               parameter,                 _, _, _, _, _, _, _, X, _)\
  F(ParameterAst,            parameter_ast,             _, _, X, X, _, _, _, _, _)\
  F(Path,                    path,                      X, X, X, X, _, _, _, X, _)\
  F(ProgramAst,              program_ast,               _, _, X, _, _, _, _, _, _)\
  F(Protocol,                protocol,                  _, _, X, X, _, _, _, X, _)\
  F(Roots,                   roots,                     _, _, _, _, _, _, _, X, X)\
  F(SequenceAst,             sequence_ast,              _, _, X, X, _, _, X, _, _)\
  F(Signature,               signature,                 _, _, _, _, _, _, _, X, X)\
  F(SignatureAst,            signature_ast,             _, _, X, X, _, _, _, _, _)\
  F(Stack,                   stack,                     _, _, _, _, _, _, _, _, _)\
  F(StackPiece,              stack_piece,               _, _, _, _, _, _, _, _, _)\
  F(String,                  string,                    X, X, _, X, X, _, _, _, _)\
  F(SymbolAst,               symbol_ast,                _, _, X, X, _, _, _, _, _)\
  F(UnboundModule,           unbound_module,            _, _, X, _, _, _, _, _, _)\
  F(UnboundModuleFragment,   unbound_module_fragment,   _, _, X, _, _, _, _, _, _)\
  F(Unknown,                 unknown,                   _, _, X, _, _, _, _, _, _)\
  F(VoidP,                   void_p,                    _, _, _, _, X, _, _, _, _)

// Enumerates all the object families.
#define ENUM_OBJECT_FAMILIES(F)                                                \
    ENUM_SPECIAL_OBJECT_FAMILIES(F)                                            \
    ENUM_OTHER_OBJECT_FAMILIES(F)

// Enum identifying the different families of heap objects.
typedef enum {
  __ofFirst__ = -1
  #define __DECLARE_OBJECT_FAMILY_ENUM__(Family, family, CM, ID, PT, SR, NL, FU, EM, MD, OW) \
  , of##Family
  ENUM_OBJECT_FAMILIES(__DECLARE_OBJECT_FAMILY_ENUM__)
  #undef __DECLARE_OBJECT_FAMILY_ENUM__
  // This is a special value separate from any of the others that can be used
  // to indicate no family.
  , __ofUnknown__
} object_family_t;

// Returns the string name of the given family.
const char *get_object_family_name(object_family_t family);

// Number of bytes in an object header.
#define kObjectHeaderSize kValueSize

// Returns the size in bytes of an object with N fields, where the header
// is not counted as a field.
#define OBJECT_SIZE(N) ((N * kValueSize) + kObjectHeaderSize)

// Returns the offset of the N'th field in an object, starting from 0 so the
// header fields aren't included.
#define OBJECT_FIELD_OFFSET(N) ((N * kValueSize) + kObjectHeaderSize)

// The offset of the object header.
static const size_t kObjectHeaderOffset = 0;

// Forward declaration of the object behavior structs (see behavior.h).
FORWARD(family_behavior_t);
FORWARD(division_behavior_t);

// Bit cast a void* to a value. This is similar to making a new object except
// that the pointer is allowed to be unaligned.
static value_t pointer_to_value_bit_cast(void *ptr) {
  // See the sizes test in test_value.
  encoded_value_t encoded = 0;
  memcpy(&encoded, &ptr, sizeof(void*));
  return (value_t) {.encoded=encoded};
}

// Converts a pointer to an object into an tagged object value pointer.
static value_t new_object(address_t addr) {
  value_t result = pointer_to_value_bit_cast(addr + vdObject);
  CHECK_DOMAIN(vdObject, result);
  return result;
}

// Bit cast a value to a void*.
static void *value_to_pointer_bit_cast(value_t value) {
  void *result = 0;
  memcpy(&result, &value.encoded, sizeof(void*));
  return result;
}

// Returns the address of the object pointed to by a tagged object value
// pointer.
static address_t get_object_address(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  return value_to_pointer_bit_cast(value) - vdObject;
}

// Returns a pointer to the index'th field in the given heap object. Check
// fails if the value is not a heap object.
static value_t *access_object_field(value_t value, size_t index) {
  CHECK_DOMAIN(vdObject, value);
  address_t addr = get_object_address(value);
  return (value_t*) (addr + index);
}

// Returns the object type of the object the given value points to.
object_family_t get_object_family(value_t value);

// Returns true iff the given value belongs to a syntax family.
bool in_syntax_family(value_t value);

// This is for consistency -- any predicate used in a check predicate must have
// a method for converting the predicate result into a string.
const char *in_syntax_family_name(bool value);

// Returns the family behavior for the given object.
family_behavior_t *get_object_family_behavior(value_t self);

// Does the same as get_object_family_behavior but with checks of the species
// disabled. This allows it to be called during garbage collection on objects
// whose species have already been migrated. The behavior is still there but
// the species won't survive any sanity checks.
family_behavior_t *get_object_family_behavior_unchecked(value_t self);

// Sets the species pointer of an object to the specified species value.
void set_object_species(value_t value, value_t species);

// Expands to the declaration of a setter that takes the specified value type.
#define TYPED_SETTER_DECL(receiver, type_t, field)                             \
void set_##receiver##_##field(value_t self, type_t value);

// Expands to the declaration of a getter that returns the specified value type.
#define TYPED_GETTER_DECL(receiver, type_t, field)                             \
type_t get_##receiver##_##field(value_t self)

// Expands to the declaration of a getter and setter for the specified field.
#define TYPED_ACCESSORS_DECL(receiver, type_t, field)                          \
TYPED_SETTER_DECL(receiver, type_t, field);                                    \
TYPED_GETTER_DECL(receiver, type_t, field)

// Expands to declarations of a getter and setter for the specified field in the
// specified object.
#define ACCESSORS_DECL(receiver, field)                                        \
TYPED_SETTER_DECL(receiver, value_t, field);                                   \
TYPED_GETTER_DECL(receiver, value_t, field)

// Expands to declarations of a getter and setter for the specified field in the
// specified object.
#define SPECIES_ACCESSORS_DECL(receiver, field)                                \
ACCESSORS_DECL(receiver##_species, field);                                     \
TYPED_GETTER_DECL(receiver, value_t, field)

#define INTEGER_SETTER_DECL(receiver, field) TYPED_SETTER_DECL(receiver, size_t, field)

#define INTEGER_GETTER_DECL(receiver, field) TYPED_GETTER_DECL(receiver, size_t, field)

#define INTEGER_ACCESSORS_DECL(receiver, field)                                \
INTEGER_SETTER_DECL(receiver, field);                                          \
INTEGER_GETTER_DECL(receiver, field)

// The species of this object.
ACCESSORS_DECL(object, species);

// The header of the given object value. During normal execution this will be
// the species but during GC it may be a forward pointer. Only use these calls
// during GC, anywhere else use the species accessors which does more checking.
ACCESSORS_DECL(object, header);

// --- S p e c i e s ---

#define ENUM_SPECIES_DIVISIONS(F)                                              \
  F(Compact, compact)                                                          \
  F(Instance, instance)                                                        \
  F(Modal, modal)

// Identifies the division a species belongs to.
typedef enum {
  __sdFirst__ = -1
#define DECLARE_SPECIES_DIVISION_ENUM(Division, division) , sd##Division
ENUM_SPECIES_DIVISIONS(DECLARE_SPECIES_DIVISION_ENUM)
#undef DECLARE_SPECIES_DIVISION_ENUM
} species_division_t;

// Returns the string name of the given division.
const char *get_species_division_name(species_division_t division);

// The size of the species header, the part that's the same for all species.
#define kSpeciesHeaderSize OBJECT_SIZE(3)

// The instance family could be stored within the family struct but we need it
// so often that it's worth the extra space to have it as close at hand as
// possible.
static const size_t kSpeciesInstanceFamilyOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kSpeciesFamilyBehaviorOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kSpeciesDivisionBehaviorOffset = OBJECT_FIELD_OFFSET(2);

// Expands to the size of a species with N field in addition to the species
// header.
#define SPECIES_SIZE(N) (kSpeciesHeaderSize + (N * kValueSize))

// Expands to the byte offset of the N'th field in a species object, starting
// from 0 and not counting the species header fields.
#define SPECIES_FIELD_OFFSET(N) (kSpeciesHeaderSize + (N * kValueSize))

// The object family of instances of this species.
TYPED_ACCESSORS_DECL(species, object_family_t, instance_family);

// Returns the object family behavior of this species belongs to.
family_behavior_t *get_species_family_behavior(value_t species);

// Sets the object family behavior of this species.
void set_species_family_behavior(value_t species, family_behavior_t *behavior);

// Returns the species division behavior of this species belongs to.
division_behavior_t *get_species_division_behavior(value_t species);

// Sets the species division behavior of this species.
void set_species_division_behavior(value_t species, division_behavior_t *behavior);

// Returns the division the given species belongs to.
species_division_t get_species_division(value_t value);

// Returns the division the given object belongs to.
species_division_t get_object_division(value_t value);


// --- C o m p a c t   s p e c i e s ---

static const size_t kCompactSpeciesSize = SPECIES_SIZE(0);


// --- I n s t a n c e   s p e c i e s ---

static const size_t kInstanceSpeciesSize = SPECIES_SIZE(1);
static const size_t kInstanceSpeciesPrimaryProtocolOffset = SPECIES_FIELD_OFFSET(0);

// The primary protocol of the instance.
SPECIES_ACCESSORS_DECL(instance, primary_protocol);


// --- M o d a l    s p e c i e s ---

// Enum of the modes an object can be in.
typedef enum {
  // Any changes can be made to this object, including changing which protocol
  // it supports and which fields it has.
  vmFluid,
  // The object's fields can be set.
  vmMutable,
  // The object cannot be changed but can reference other objects that can.
  vmFrozen,
  // The object cannot change and neither can any objects it references.
  vmDeepFrozen
} value_mode_t;

static const size_t kModalSpeciesSize = SPECIES_SIZE(2);
static const size_t kModalSpeciesModeOffset = SPECIES_FIELD_OFFSET(0);
static const size_t kModalSpeciesBaseRootOffset = SPECIES_FIELD_OFFSET(1);

// Returns the mode this species indicates.
value_mode_t get_modal_species_mode(value_t value);

// Returns the mode for a value in the modal division.
value_mode_t get_modal_object_mode(value_t value);

// Sets the mode for a modal object by switching to the species with the
// appropriate mode.
value_t set_modal_object_mode_unchecked(runtime_t *runtime, value_t self,
    value_mode_t mode);

// Sets the mode this species indicates.
void set_modal_species_mode(value_t value, value_mode_t mode);

// Returns the root key for the first modal species of the same block of modal
// species as this one. The result is really a root_key_t but that type is not
// visible here and it doesn't really matter.
size_t get_modal_species_base_root(value_t value);

// Sets the root key for the given modal species.
void set_modal_species_base_root(value_t value, size_t base_root);

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

// If the given value is deep frozen, returns the internal true value. If it is
// not attempts to make it deep frozen, that it, traverses the objects reachable
// and checking whether they're all frozen or deep frozen and marking them as
// deep frozen as we go. If this succeeds returns the internal true value,
// otherwise the internal false value. Marking the objects as deep frozen may
// under some circumstances involve allocation; if there is a problem there a
// signal may be returned.
//
// If validation fails and the offender_out parameter is non-null, an arbitrary
// mutable object from the object graph will be stored there. This is a
// debugging aid, since it's arbitrary which object will be stored you should
// not depend on the particular value in any way.
//
// This is the only reliable way to check whether a value is deep frozen since
// being deep frozen is a property of an object graph, not an individual object,
// and using marking like this is the only efficient way to reliably determine
// that property.
value_t try_validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out);

// Works the same way as try_validate_deep_frozen but returns a non-signal instead
// of true and a signal for false. Depending on what the most convenient
// interface is you can use either this or the other, they do the same thing.
value_t validate_deep_frozen(runtime_t *runtime, value_t value,
    value_t *offender_out);


// --- S t r i n g ---

static const size_t kStringLengthOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kStringCharsOffset = OBJECT_FIELD_OFFSET(1);

// Returns the size of a heap string with the given number of characters.
size_t calc_string_size(size_t char_count);

// The length in characters of a heap string.
INTEGER_ACCESSORS_DECL(string, length);

// Returns a pointer to the array that holds the contents of this array.
char *get_string_chars(value_t value);

// Stores the contents of this string in the given output.
void get_string_contents(value_t value, string_t *out);

// Writes the raw contents of the given string on the given buffer
void string_buffer_append_string(string_buffer_t *buf, value_t value);


// --- B l o b ---

static const size_t kBlobLengthOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kBlobDataOffset = OBJECT_FIELD_OFFSET(1);

// Returns the size of a heap blob with the given number of bytes.
size_t calc_blob_size(size_t size);

// The length in bytes of a heap blob.
INTEGER_ACCESSORS_DECL(blob, length);

// Gives access to the data in the given blob value in a blob struct through
// the out parameter.
void get_blob_data(value_t value, blob_t *blob_out);


// --- V o i d   P ---

static const size_t kVoidPSize = OBJECT_SIZE(1);
static const size_t kVoidPValueOffset = OBJECT_FIELD_OFFSET(0);

// Sets the pointer stored in a void-p.
void set_void_p_value(value_t value, void *ptr);

// Gets the pointer stored in a void-p.
void *get_void_p_value(value_t value);


// --- A r r a y ---

static const size_t kArrayLengthOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kArrayElementsOffset = OBJECT_FIELD_OFFSET(1);

// Calculates the size of a heap array with the given number of elements.
size_t calc_array_size(size_t length);

// The number of elements in this array.
INTEGER_ACCESSORS_DECL(array, length);

// Returns the index'th element in the given array. Bounds checks the index and
// returns an OutOfBounds signal under soft check failures.
value_t get_array_at(value_t value, size_t index);

// Sets the index'th element in the given array.
void set_array_at(value_t value, size_t index, value_t element);

// Returns the underlying array element array.
value_t *get_array_elements(value_t value);

// Does the same as get_array_elements but without any sanity checks which means
// that it can be used during garbage collection. Don't use it anywhere else.
value_t *get_array_elements_unchecked(value_t value);

// Sorts the given array. If there are any errors, for instance if some of the
// values are not comparable, a signal is returned.
value_t sort_array(value_t value);

// Sorts the first 'elmc' elements of this array.
value_t sort_array_partial(value_t value, size_t elmc);

// Returns true if the given array is sorted.
bool is_array_sorted(value_t value);

// Pair array. An even-sized array can also be viewed as an array of pairs such
// that [1, 2, 3, 4] is used to represent [(1, 2), (3, 4)]. The pair_array
// functions help use an array like that.

// Sorts pairs of array elements by the first element, moving the second element
// with the first one but not using it for comparison.
value_t co_sort_pair_array(value_t value);

// Returns true if this array, viewed as a pair array, is sorted by the first
// pair element.
bool is_pair_array_sorted(value_t value);

// Binary searches for a pair whose first component is the given key in the
// given sorted pair array, returning the second component.
value_t binary_search_pair_array(value_t self, value_t key);

// Sets the first entry in the index'th pair in the given array, viewed as a
// pair array.
void set_pair_array_first_at(value_t self, size_t index, value_t value);

// Gets the first entry in the index'th pair in the given array, viewed as a
// pair array.
value_t get_pair_array_first_at(value_t self, size_t index);

// Sets the second entry in the index'th pair in the given array, viewed as a
// pair array.
void set_pair_array_second_at(value_t self, size_t index, value_t value);

// Gets the second entry in the index'th pair in the given array, viewed as a
// pair array.
value_t get_pair_array_second_at(value_t self, size_t index);

// Returns the length of this array when viewed as a pair array.
size_t get_pair_array_length(value_t self);


// --- T u p l e ---

// Returns the index'th entry in the given tuple.
value_t get_tuple_at(value_t self, size_t index);


// --- A r r a y   b u f f e r ---

// An array buffer is similar to an array but can grow as elements are added
// to it.

static const size_t kArrayBufferSize = OBJECT_SIZE(2);
static const size_t kArrayBufferElementsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kArrayBufferLengthOffset = OBJECT_FIELD_OFFSET(1);

// The array storing the elements in this buffer.
ACCESSORS_DECL(array_buffer, elements);

// The length of this array buffer.
INTEGER_ACCESSORS_DECL(array_buffer, length);

// Returns the index'th element in the given array buffer. Bounds checks the
// index and returns an OutOfBounds signal under soft check failures.
value_t get_array_buffer_at(value_t self, size_t index);

// Sets the index'th element in this array buffer. Bounds checks the index.
void set_array_buffer_at(value_t self, size_t index, value_t value);

// Attempts to add an element at the end of this array buffer, increasing its
// length by 1. Returns true if this succeeds, false if it wasn't possible.
bool try_add_to_array_buffer(value_t self, value_t value);

// Checks whether there is already a value in this array buffer object identical
// to the given value and if not adds it.
value_t ensure_array_buffer_contains(runtime_t *runtime, value_t self,
    value_t value);

// Returns true iff the given array buffer contains a value identical to the
// given value.
bool in_array_buffer(value_t self, value_t value);

// Sorts the contents of this array buffer.
void sort_array_buffer(value_t self);


// --- I d e n t i t y   h a s h   m a p ---

static const size_t kIdHashMapSize = OBJECT_SIZE(4);
static const size_t kIdHashMapSizeOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kIdHashMapCapacityOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kIdHashMapOccupiedCountOffset = OBJECT_FIELD_OFFSET(2);
static const size_t kIdHashMapEntryArrayOffset = OBJECT_FIELD_OFFSET(3);

static const size_t kIdHashMapEntryFieldCount = 3;
static const size_t kIdHashMapEntryKeyOffset = 0;
static const size_t kIdHashMapEntryHashOffset = 1;
static const size_t kIdHashMapEntryValueOffset = 2;

// The backing array storing the entries of this hash map.
ACCESSORS_DECL(id_hash_map, entry_array);

// The number of mappings in this hash map.
INTEGER_ACCESSORS_DECL(id_hash_map, size);

// The max capacity of this hash map.
INTEGER_ACCESSORS_DECL(id_hash_map, capacity);

// The number of occupied entries, that is, entries that are either non-empty
// or deleted.
INTEGER_ACCESSORS_DECL(id_hash_map, occupied_count);

// Adds a binding from the given key to the given value to this map, replacing
// the existing one if it already exists. Returns a signal on failure, either
// if the key cannot be hashed or the map is full.
value_t try_set_id_hash_map_at(value_t map, value_t key, value_t value,
    bool allow_frozen);

// Returns the binding for the given key or, if no binding is present, an
// appropriate signal.
value_t get_id_hash_map_at(value_t map, value_t key);

// Returns the binding for the given key or, if no binding is present, the
// specified default value.
value_t get_id_hash_map_at_with_default(value_t map, value_t key, value_t defawlt);

// Returns true iff the given map has a binding for the given key.
bool has_id_hash_map_at(value_t map, value_t key);

// Adds a binding from the given key to the given value to this map, replacing
// the existing one if it already exists. Returns a signal on failure, either
// if the key cannot be hashed or there isn't enough memory in the runtime to
// extend the map.
value_t set_id_hash_map_at(runtime_t *runtime, value_t map, value_t key, value_t value);

// Removes the mapping for the given key from the map if it exists. Returns
// a NotFound signal if that is the case, otherwise a non-signal.
value_t delete_id_hash_map_at(runtime_t *runtime, value_t map, value_t key);

// Data associated with iterating through a map. The iterator grabs the fields
// it needs from the map on initialization so it's safe to overwrite map fields
// while iterating, however it is _not_ safe to modify the entry array that was
// captured in any way.
typedef struct {
  // The entries we're iterating through.
  value_t *entries;
  // The starting point from which to search for the next entry to return.
  size_t cursor;
  // The total capacity of this map.
  size_t capacity;
  // The current entry.
  value_t *current;
} id_hash_map_iter_t;

// Initializes an iterator for iterating the given map. Once initialized the
// iterator starts out before the first entry so after the first call to advance
// you'll have access to the first element.
void id_hash_map_iter_init(id_hash_map_iter_t *iter, value_t map);

// Advances the iterator to the next element. Returns true iff an element
// is found.
bool id_hash_map_iter_advance(id_hash_map_iter_t *iter);

// Reads the key and value from the current map entry, storing them in the
// out parameters. Fails if the last advance call did not return true.
void id_hash_map_iter_get_current(id_hash_map_iter_t *iter, value_t *key_out,
    value_t *value_out);


// --- N u l l ---

static const size_t kNullSize = OBJECT_SIZE(0);


// --- N o t h i n g ---

static const size_t kNothingSize = OBJECT_SIZE(0);


// --- B o o l e a n ---

static const size_t kBooleanSize = OBJECT_SIZE(1);
static const size_t kBooleanValueOffset = OBJECT_FIELD_OFFSET(0);

// Sets whether the given bool represents true or false.
void set_boolean_value(value_t value, bool truth);

// Returns whether the given bool is true.
bool get_boolean_value(value_t value);


// --- K e y ---

static const size_t kKeySize = OBJECT_SIZE(2);
static const size_t kKeyIdOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kKeyDisplayNameOffset = OBJECT_FIELD_OFFSET(1);

// The unique id for this key (unique across this runtime).
INTEGER_ACCESSORS_DECL(key, id);

// This key's optional display name. This has no semantic meaning, it's only a
// debugging aid.
ACCESSORS_DECL(key, display_name);


// --- I n s t a n c e ---

static const size_t kInstanceSize = OBJECT_SIZE(1);
static const size_t kInstanceFieldsOffset = OBJECT_FIELD_OFFSET(0);

// The field map for this instance.
ACCESSORS_DECL(instance, fields);

// Returns the field with the given key from the given instance.
value_t get_instance_field(value_t value, value_t key);

// Sets the field with the given key on the given instance. Returns a signal
// if setting the field failed, for instance if the field map was full.
value_t try_set_instance_field(value_t instance, value_t key, value_t value);


// --- F a c t o r y ---

// A factory is a wrapper around a constructor C function that produces
// instances of a particular family.

// The type of constructor functions stored in a factory.
typedef value_t (factory_constructor_t)(runtime_t *runtime);

static const size_t kFactorySize = OBJECT_SIZE(1);
static const size_t kFactoryConstructorOffset = OBJECT_FIELD_OFFSET(0);

// The constructor function for this factory wrapped in a void-p.
ACCESSORS_DECL(factory, constructor);


//  --- C o d e   b l o c k ---

static const size_t kCodeBlockSize = OBJECT_SIZE(3);
static const size_t kCodeBlockBytecodeOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kCodeBlockValuePoolOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kCodeBlockHighWaterMarkOffset = OBJECT_FIELD_OFFSET(2);

// The binary blob of bytecode for this code block.
ACCESSORS_DECL(code_block, bytecode);

// The value pool array for this code block.
ACCESSORS_DECL(code_block, value_pool);

// The highest stack height possible when executing this code.
INTEGER_ACCESSORS_DECL(code_block, high_water_mark);


// --- P r o t o c o l ---

static const size_t kProtocolSize = OBJECT_SIZE(1);
static const size_t kProtocolDisplayNameOffset = OBJECT_FIELD_OFFSET(0);

// Returns the display (debug) name for this protocol object.
ACCESSORS_DECL(protocol, display_name);


// --- A r g u m e n t   m a p   t r i e ---

// A trie that allows you to construct and reuse argument maps such that there's
// only one instance of each argument map.

static const size_t kArgumentMapTrieSize = OBJECT_SIZE(2);
static const size_t kArgumentMapTrieValueOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kArgumentMapTrieChildrenOffset = OBJECT_FIELD_OFFSET(1);

// The value of this argument trie node whose contents match the path taken to
// reach this node.
ACCESSORS_DECL(argument_map_trie, value);

// Pointers to the child trie nodes whose prefixes match this one.
ACCESSORS_DECL(argument_map_trie, children);

// Returns an argument map trie whose value has the value of the given trie
// as a prefix, followed by the given index. Creates the child if necessary
// which means that this call may fail.
value_t get_argument_map_trie_child(runtime_t *runtime, value_t self, value_t key);


// --- L a m b d a ---

static const size_t kLambdaSize = OBJECT_SIZE(2);
static const size_t kLambdaMethodsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kLambdaOutersOffset = OBJECT_FIELD_OFFSET(1);

// Returns the method space where the methods supported by this lambda live.
ACCESSORS_DECL(lambda, methods);

// Returns the array of outer variables for this lambda.
ACCESSORS_DECL(lambda, outers);

// Returns the index'th outer value captured by the given lambda.
value_t get_lambda_outer(value_t self, size_t index);


// --- N a m e s p a c e ---

static const size_t kNamespaceSize = OBJECT_SIZE(1);
static const size_t kNamespaceBindingsOffset = OBJECT_FIELD_OFFSET(0);

// Returns the bindings map for this namespace.
ACCESSORS_DECL(namespace, bindings);

// Returns the binding for the given name in the given namespace. If the binding
// doesn't exist a NotFound signal is returned.
value_t get_namespace_binding_at(value_t namespace, value_t name);

// Sets the binding for a given name in the given namespace.
value_t set_namespace_binding_at(runtime_t *runtime, value_t namespace,
    value_t name, value_t value);


// --- M o d u l e   f r a g m e n t ---

// Identifies the phases a module fragment goes through is it's being loaded
// and bound.
typedef enum {
  // The fragment has been created but nothing else has been done.
  feUnbound,
  // We're in the process of constructing the contents of this fragment.
  feBinding,
  // The construction process is complete.
  feComplete
} module_fragment_epoch_t;

static const size_t kModuleFragmentSize = OBJECT_SIZE(6);
static const size_t kModuleFragmentStageOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kModuleFragmentModuleOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kModuleFragmentNamespaceOffset = OBJECT_FIELD_OFFSET(2);
static const size_t kModuleFragmentMethodspaceOffset = OBJECT_FIELD_OFFSET(3);
static const size_t kModuleFragmentImportsOffset = OBJECT_FIELD_OFFSET(4);
static const size_t kModuleFragmentEpochOffset = OBJECT_FIELD_OFFSET(5);

// The index of the stage this fragment belongs to.
ACCESSORS_DECL(module_fragment, stage);

// The module this is a fragment of.
ACCESSORS_DECL(module_fragment, module);

// The namespace that holds the module fragment's own bindings.
ACCESSORS_DECL(module_fragment, namespace);

// The method space that holds the module fragment's own methods.
ACCESSORS_DECL(module_fragment, methodspace);

// The namespace that holds the fragment's imports. Imports work slightly
// differently than other definitions in that they can be accessed through
// any stage so they get their own namespace.
// TODO: consider whether this is really a good design.
ACCESSORS_DECL(module_fragment, imports);

// The current epoch of the given module fragment.
TYPED_ACCESSORS_DECL(module_fragment, module_fragment_epoch_t, epoch);

// Returns true iff this fragment has been bound.
bool is_module_fragment_bound(value_t fragment);


// --- M o d u l e   ---

static const size_t kModuleSize = OBJECT_SIZE(2);
static const size_t kModulePathOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kModuleFragmentsOffset = OBJECT_FIELD_OFFSET(1);

// This module's namespace path.
ACCESSORS_DECL(module, path);

// The fragments that make up this module.
ACCESSORS_DECL(module, fragments);

// Looks up an identifier in the given module by finding the fragment for the
// appropriate stage and looking the path up in the namespace. If for any reason
// the binding cannot be found a lookup error signal is returned. To avoid
// having to construct new identifiers when shifting between stages this call
// takes the stage and path of the identifier separately.
value_t module_lookup_identifier(runtime_t *runtime, value_t self, value_t stage,
    value_t path);

// Returns the fragment in the given module that corresponds to the specified
// stage. If there is no such fragment a NotFound signal is returned.
value_t get_module_fragment_at(value_t self, value_t stage);

// Add a module fragment to the list of fragments held by this module.
value_t add_module_fragment(runtime_t *runtime, value_t self, value_t fragment);

// Returns the fragment in the given module that comes immediately before the
// given stage.
value_t get_module_fragment_before(value_t self, value_t stage);

// Returns true iff the given module has a fragment for the given stage. If there
// is no stage before this one NotFound is returned.
bool has_module_fragment_at(value_t self, value_t stage);


// --- P a t h ---

static const size_t kPathSize = OBJECT_SIZE(2);
static const size_t kPathRawHeadOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kPathRawTailOffset = OBJECT_FIELD_OFFSET(1);

// The first part of the path. For instance, the head of a:b:c is a. This can
// be safely called on any path; for the empty path the nothing object will
// be returned.
ACCESSORS_DECL(path, raw_head);

// The end of the path. For instance, the tail of a:b:c is b:c. This can
// be safely called on any path; for the empty path the nothing object will
// be returned.
ACCESSORS_DECL(path, raw_tail);

// Returns the head of the given non-empty path. If the path is empty a signal
// will be returned in soft check mode, otherwise the nothing object will be
// returned.
value_t get_path_head(value_t path);

// Returns the tail of the given non-empty path. If the path is empty a signal
// will be returned in soft check mode, otherwise the nothing object will be
// returned.
value_t get_path_tail(value_t path);

// Returns true iff the given path is empty.
bool is_path_empty(value_t path);


// --- I d e n t i f i e r ---

static const size_t kIdentifierSize = OBJECT_SIZE(2);
static const size_t kIdentifierPathOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kIdentifierStageOffset = OBJECT_FIELD_OFFSET(1);

// The path (ie. x:y:z etc.) of this identifier.
ACCESSORS_DECL(identifier, path);

// The stage (ie. $..., @..., etc) of this identifier.
ACCESSORS_DECL(identifier, stage);

// Returns true if the given identifier has the specified stage and path.
bool is_identifier_identical(value_t self, value_t stage, value_t path);


// --- F u n c t i o n ---

static const size_t kFunctionSize = OBJECT_SIZE(1);
static const size_t kFunctionDisplayNameOffset = OBJECT_FIELD_OFFSET(0);

// The display name of this function.
ACCESSORS_DECL(function, display_name);


// --- U n k n o w n ---

static const size_t kUnknownSize = OBJECT_SIZE(2);
static const size_t kUnknownHeaderOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kUnknownPayloadOffset = OBJECT_FIELD_OFFSET(1);

// The header of this object of unknown type.
ACCESSORS_DECL(unknown, header);

// The payload of this object of unknown type.
ACCESSORS_DECL(unknown, payload);


// --- O p t i o n s ---

// This should so be an instance-type object defined in a library. But alas it's
// currently part of the implementation of libraries so maybe later.
static const size_t kOptionsSize = OBJECT_SIZE(1);
static const size_t kOptionsElementsOffset = OBJECT_FIELD_OFFSET(0);

// The elements array of this set of options.
ACCESSORS_DECL(options, elements);

// Returns the value of the flag with the given key in the given options set.
// If the key is not available the default is returned.
value_t get_options_flag_value(runtime_t *runtime, value_t self, value_t key,
    value_t defawlt);


// --- O r d e r i n g ---

// Returns a value ordering to an integer such that less than becomes -1,
// greater than becomes 1 and equals becomes 0.
int ordering_to_int(value_t value);

// Returns a non-signal indicating an ordering such that ordering_to_int returns
// the given value.
value_t int_to_ordering(int value);


// --- M i s c ---

// Initialize the map from core type names to the factories themselves.
value_t init_plankton_core_factories(value_t map, runtime_t *runtime);

// Adds a factory object to the given plankton factory environment map under th
// given category and name.
value_t add_plankton_factory(value_t map, value_t category, const char *name,
    factory_constructor_t constructor, runtime_t *runtime);


// --- D e b u g ---

// Similar to printf but adds a newline and allows %v to print values.
void print_ln(const char *fmt, ...);


#endif // _VALUE
