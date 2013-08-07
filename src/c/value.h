// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// The basic types of values.

#include "check.h"
#include "globals.h"
#include "utils.h"

#ifndef _VALUE
#define _VALUE

FORWARD(runtime_t);

// Value domain identifiers.
typedef enum {
  // Pointer to a heap object.
  vdObject = 0,
  // Tagged integer.
  vdInteger,
  // A VM-internal signal.
  vdSignal,
  // An object that has been moved during an in-process garbage collection.
  vdMovedObject
} value_domain_t;

// Invokes the given macro for each signal cause.
#define ENUM_SIGNAL_CAUSES(F)                                                  \
  F(HeapExhausted)                                                             \
  F(InvalidInput)                                                              \
  F(InvalidSyntax)                                                             \
  F(MapFull)                                                                   \
  F(NotFound)                                                                  \
  F(Nothing)                                                                   \
  F(OutOfBounds)                                                               \
  F(SystemError)                                                               \
  F(UnsupportedBehavior)                                                       \
  F(ValidationFailed)                                                          \
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
  signal_cause_t cause;
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


// --- I n t e g e r ---

// Creates a new tagged integer with the given value.
static value_t new_integer(int64_t value) {
  value_t result;
  result.as_integer.value = value;
  result.as_integer.domain = vdInteger;
  return result;
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_t value) {
  CHECK_DOMAIN(vdInteger, value);
  return value.as_integer.value;
}

// Returns a value that is _not_ a signal. This can be used to indicate
// unspecific success.
static value_t success() {
  return new_integer(0);
}


// --- S i g n a l ---

// Returns the string name of a signal cause.
const char *signal_cause_name(signal_cause_t cause);

// How many bits do we need to hold the cause of a signal?
static const int kSignalCauseSize = 5;
static const int kSignalCauseMask = 0x1f;

// Creates a new signal with the specified cause.
static value_t new_signal(signal_cause_t cause) {
  value_t result;
  result.as_signal.cause = cause;
  result.as_signal.domain = vdSignal;
  return result;
}

// Returns the cause of a signal.
static signal_cause_t get_signal_cause(value_t value) {
  CHECK_DOMAIN(vdSignal, value);
  return  value.as_signal.cause;
}


// --- M o v e d   o b j e c t ---

// Given a moved object forward pointer returns the object this object has been
// moved to.
static value_t get_moved_object_target(value_t value) {
  CHECK_DOMAIN(vdMovedObject, value);
  value_t target;
  target.encoded = value.encoded - vdMovedObject;
  CHECK_DOMAIN(vdObject, target);
  return target;
}

// Creates a new moved object pointer pointing to the given target object.
static value_t new_moved_object(value_t target) {
  CHECK_DOMAIN(vdObject, target);
  value_t moved;
  moved.encoded = target.encoded + vdMovedObject;
  CHECK_DOMAIN(vdMovedObject, moved);
  return moved;
}


// --- O b j e c t ---

// Enumerates the special species, the ones that require special handling during
// startup.
#define ENUM_SPECIAL_OBJECT_FAMILIES(F)                                        \
  F(Species,    species)

// Enumerates the syntax tree families.
#define ENUM_SYNTAX_OBJECT_FAMILIES(F)                                         \
  F(LiteralAst, literal_ast)

// Enumerates the compact object species.
#define ENUM_COMPACT_OBJECT_FAMILIES(F)                                        \
  F(String,     string)                                                        \
  F(Array,      array)                                                         \
  F(Null,       null)                                                          \
  F(Bool,       bool)                                                          \
  F(IdHashMap,  id_hash_map)                                                   \
  F(Blob,       blob)                                                          \
  F(Instance,   instance)                                                      \
  F(VoidP,      void_p)                                                        \
  F(Factory,    factory)                                                       \
  F(CodeBlock,  code_block)                                                    \
  F(StackPiece, stack_piece)                                                   \
  F(Stack,      stack)                                                         \
  ENUM_SYNTAX_OBJECT_FAMILIES(F)

// Enumerates all the object families.
#define ENUM_OBJECT_FAMILIES(F)                                                \
    ENUM_SPECIAL_OBJECT_FAMILIES(F)                                            \
    ENUM_COMPACT_OBJECT_FAMILIES(F)

// Enum identifying the different families of heap objects.
typedef enum {
  __ofFirst__ = -1
  #define DECLARE_OBJECT_FAMILY_ENUM(Family, family) , of##Family
  ENUM_OBJECT_FAMILIES(DECLARE_OBJECT_FAMILY_ENUM)
  #undef DECLARE_OBJECT_FAMILY_ENUM
} object_family_t;

// Number of bytes in an object header.
#define kObjectHeaderSize kValueSize

// Returns the size in bytes of an object with N fields, where the header
// is not counted as a field.
#define OBJECT_SIZE(N) ((N * kValueSize) + kObjectHeaderSize)

static const size_t kObjectHeaderOffset = 0;

// Converts a pointer to an object into an tagged object value pointer.
static value_t new_object(address_t addr) {
  CHECK_EQ("unaligned", 0, ((address_arith_t) addr) & kDomainTagMask);
  value_t result;
  result.encoded = ((address_arith_t) addr);
  return result;
}

// Returns the address of the object pointed to by a tagged object value
// pointer.
static address_t get_object_address(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  return (address_t) value.encoded;
}

// Returns a pointer to the index'th field in the given heap object. Check
// fails if the value is not a heap object.
static value_t *access_object_field(value_t value, size_t index) {
  CHECK_DOMAIN(vdObject, value);
  address_t addr = get_object_address(value);
  return ((value_t*) addr) + index;
}

// Returns the object type of the object the given value points to.
object_family_t get_object_family(value_t value);

// Sets the species pointer of an object to the specified species value.
void set_object_species(value_t value, value_t species);

// Expands to the declaration of a setter function.
#define SETTER_DECL(receiver, field)                                           \
void set_##receiver##_##field(value_t self, value_t value)

// Expands to the declaration of a getter.
#define GETTER_DECL(receiver, field)                                           \
value_t get_##receiver##_##field(value_t self)

// Expands to declarations of a getter and setter for the specified field in the
// specified object.
#define ACCESSORS_DECL(receiver, field)                                        \
SETTER_DECL(receiver, field);                                                  \
GETTER_DECL(receiver, field)

#define INTEGER_SETTER_DECL(receiver, field)                                   \
void set_##receiver##_##field(value_t self, size_t value)

#define INTEGER_GETTER_DECL(receiver, field)                                   \
size_t get_##receiver##_##field(value_t self)

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
  F(Compact, compact)

// Identifies the division a species belongs to.
typedef enum {
  __sdFirst__ = -1
#define DECLARE_SPECIES_DIVISION_ENUM(Division, division) , sd##Division
ENUM_SPECIES_DIVISIONS(DECLARE_SPECIES_DIVISION_ENUM)
#undef DECLARE_SPECIES_DIVISION_ENUM
} species_division_t;

static const size_t kSpeciesInstanceFamilyOffset = 1;
static const size_t kSpeciesFamilyBehaviorOffset = 2;
static const size_t kSpeciesDivisionBehaviorOffset = 3;

// Given a species object, sets the instance type field to the specified value.
void set_species_instance_family(value_t species, object_family_t instance_family);

// Given a species object, returns the instance type field.
object_family_t get_species_instance_family(value_t species);

// Forward declaration of the object behavior struct (see behavior.h).
FORWARD(family_behavior_t);

// Returns the object family behavior of this species belongs to.
family_behavior_t *get_species_family_behavior(value_t species);

// Sets the object family behavior of this species.
void set_species_family_behavior(value_t species, family_behavior_t *behavior);

FORWARD(division_behavior_t);

// Returns the species division behavior of this species belongs to.
division_behavior_t *get_species_division_behavior(value_t species);

// Sets the species division behavior of this species.
void set_species_division_behavior(value_t species, division_behavior_t *behavior);

// Returns the division the given species belongs to.
species_division_t get_species_division(value_t value);


// --- C o m p a c t   s p e c i e s ---

static const size_t kCompactSpeciesSize = OBJECT_SIZE(3);


// --- S t r i n g ---

static const size_t kStringLengthOffset = 1;
static const size_t kStringCharsOffset = 2;

// Returns the size of a heap string with the given number of characters.
size_t calc_string_size(size_t char_count);

// The length in characters of a heap string.
INTEGER_ACCESSORS_DECL(string, length);

// Returns a pointer to the array that holds the contents of this array.
char *get_string_chars(value_t value);

// Stores the contents of this string in the given output.
void get_string_contents(value_t value, string_t *out);


// --- B l o b ---

static const size_t kBlobLengthOffset = 1;
static const size_t kBlobDataOffset = 2;

// Returns the size of a heap blob with the given number of bytes.
size_t calc_blob_size(size_t size);

// The length in bytes of a heap blob.
INTEGER_ACCESSORS_DECL(blob, length);

// Gives access to the data in the given blob value in a blob struct through
// the out parameter.
void get_blob_data(value_t value, blob_t *blob_out);


// --- V o i d   P ---

static const size_t kVoidPSize = OBJECT_SIZE(1);
static const size_t kVoidPValueOffset = 1;

// Sets the pointer stored in a void-p.
void set_void_p_value(value_t value, void *ptr);

// Gets the pointer stored in a void-p.
void *get_void_p_value(value_t value);


// --- A r r a y ---

static const size_t kArrayLengthOffset = 1;
static const size_t kArrayElementsOffset = 2;

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


// --- I d e n t i t y   h a s h   m a p ---

static const size_t kIdHashMapSize = OBJECT_SIZE(3);
static const size_t kIdHashMapSizeOffset = 1;
static const size_t kIdHashMapCapacityOffset = 2;
static const size_t kIdHashMapEntryArrayOffset = 3;

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

// Adds a binding from the given key to the given value to this map, replacing
// the existing one if it already exists. Returns a signal on failure, either
// if the key cannot be hashed or the map is full.
value_t try_set_id_hash_map_at(value_t map, value_t key, value_t value);

// Returns the binding for the given key or, if no binding is present, an
// appropriate signal.
value_t get_id_hash_map_at(value_t map, value_t key);

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

// Initializes an iterator for iterating the given map.
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


// --- B o o l ---

static const size_t kBoolSize = OBJECT_SIZE(1);
static const size_t kBoolValueOffset = 1;

// Sets whether the given bool represents true or false.
void set_bool_value(value_t value, bool truth);

// Returns whether the given bool is true.
bool get_bool_value(value_t value);


// --- I n s t a n c e ---

static const size_t kInstanceSize = OBJECT_SIZE(1);
static const size_t kInstanceFieldsOffset = 1;

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
static const size_t kFactoryConstructorOffset = 1;

// The constructor function for this factory wrapped in a void-p.
ACCESSORS_DECL(factory, constructor);


//  --- C o d e   b l o c k ---

static const size_t kCodeBlockSize = OBJECT_SIZE(3);
static const size_t kCodeBlockBytecodeOffset = 1;
static const size_t kCodeBlockValuePoolOffset = 2;
static const size_t kCodeBlockHighWaterMarkOffset = 3;

// The binary blob of bytecode for this code block.
ACCESSORS_DECL(code_block, bytecode);

// The value pool array for this code block.
ACCESSORS_DECL(code_block, value_pool);

// The highest stack height possible when executing this code.
INTEGER_ACCESSORS_DECL(code_block, high_water_mark);


// --- D e b u g ---

// Prints the string representation of the given value on stdout.
void value_print_ln(value_t value);


#endif // _VALUE
