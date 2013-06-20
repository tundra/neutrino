// The basic types of values.

#include "check.h"
#include "globals.h"
#include "utils.h"

#ifndef _VALUE
#define _VALUE

// A tagged value pointer. Values are divided into a hierarchy with four
// different levels. At the top level they are divided into different _domains_
// which can be distinguished by how the pointer is tagged; objects, integers,
// signals, etc. are different domains.
//
// Object pointers can point to objects with different layouts, each such kind
// of object is a _family_; strings and arrays are different families. Which
// family an object belongs to is stored in a field in the object's species.
//
// Species can also have different layout since some families have extra state
// associated with them that is stored in extra fields in their species. For
// instance, the syntax tree families all have a tag in their species that allows
// the vm to recognize them. Each of these is called a _division_.
//
// Finally, within each family there may be objects with the same basic layout
// but different shared state, each kind of state is a _species_. For instance,
// all user-defined types are within the same family but each different type
// belong to a different species. The species is given by the object's species
// pointer.
//
// In short: all values belong to some domain, within each domain values are
// divided into divisions based on their species' layout, each division is again
// divided into families based on instances' layout, and finally some families
// are divided into species based on their species objects.
//
// (domains)
// - integer
// - object
//   (divisions)
//   - compact
//     (families)
//     - species
//     - string
//     - array
//     - null
//     - bool
//     - id hash map
//     - blob
//     - instance
//       (species)
//       ...
//     - void-p
//     - literal
// - signal
typedef uint64_t value_t;

// Value domain identifiers.
typedef enum {
  // Pointer to a heap object.
  vdObject = 0,
  // Tagged integer.
  vdInteger,
  // A VM-internal signal.
  vdSignal
} value_domain_t;

// How many bits are used for the domain tag?
static const int kDomainTagSize = 3;
static const int kDomainTagMask = 0x7;

// Alignment that ensures that object pointers have tag 0.
#define kValueSize 8

// Returns the tag from a tagged value.
static value_domain_t get_value_domain(value_t value) {
  return (value_domain_t) (value & kDomainTagMask);
}


// --- I n t e g e r ---

// Creates a new tagged integer with the given value.
static value_t new_integer(int64_t value) {
  return (value << kDomainTagSize) | vdInteger;
}

// Returns the integer value stored in a tagged integer.
static int64_t get_integer_value(value_t value) {
  CHECK_DOMAIN(vdInteger, value);
  return (((int64_t) value) >> kDomainTagSize);
}

// Returns a value that is _not_ a signal. This can be used to indicate
// unspecific success.
static value_t success() {
  return new_integer(0);
}


// --- S i g n a l ---

#define ENUM_SIGNAL_CAUSES(F)                                                  \
  F(HeapExhausted)                                                             \
  F(SystemError)                                                               \
  F(ValidationFailed)                                                          \
  F(UnsupportedBehavior)                                                       \
  F(MapFull)                                                                   \
  F(NotFound)                                                                  \
  F(InvalidInput)

// Enum identifying the type of a signal.
typedef enum {
  __scFirst__ = -1
#define DECLARE_SIGNAL_CAUSE_ENUM(Cause) , sc##Cause
  ENUM_SIGNAL_CAUSES(DECLARE_SIGNAL_CAUSE_ENUM)
#undef DECLARE_SIGNAL_CAUSE_ENUM
} signal_cause_t;

// Returns the string name of a signal cause.
const char *signal_cause_name(signal_cause_t cause);

// How many bits do we need to hold the cause of a signal?
static const int kSignalCauseSize = 5;
static const int kSignalCauseMask = 0x1f;

// Creates a new signal with the specified cause.
static value_t new_signal(signal_cause_t cause) {
  return (value_t) (cause << kDomainTagSize) | vdSignal;
}

// Returns the cause of a signal.
static signal_cause_t get_signal_cause(value_t value) {
  CHECK_DOMAIN(vdSignal, value);
  return  (((int64_t) value) >> kDomainTagSize) & kSignalCauseMask;
}


// --- O b j e c t ---

// Macro that invokes the given macro callback for each object family.
#define ENUM_OBJECT_FAMILIES(F)                                                \
  F(Species,   species)                                                        \
  F(String,    string)                                                         \
  F(Array,     array)                                                          \
  F(Null,      null)                                                           \
  F(Bool,      bool)                                                           \
  F(IdHashMap, id_hash_map)                                                    \
  F(Blob,      blob)                                                           \
  F(Instance,  instance)                                                       \
  F(VoidP,     void_p)

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

static const size_t kObjectSpeciesOffset = 0;

// Converts a pointer to an object into an tagged object value pointer.
static value_t new_object(address_t addr) {
  CHECK_EQ("unaligned", 0, ((address_arith_t) addr) & kDomainTagMask);
  return (value_t) ((address_arith_t) addr);
}

// Returns the address of the object pointed to by a tagged object value
// pointer.
static address_t get_object_address(value_t value) {
  CHECK_DOMAIN(vdObject, value);
  return (address_t) ((address_arith_t) value);
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

// Returns the species of the given object value.
value_t get_object_species(value_t value);


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
struct family_behavior_t;

// Returns the object family behavior of this species belongs to.
struct family_behavior_t *get_species_family_behavior(value_t species);

// Sets the object family behavior of this species.
void set_species_family_behavior(value_t species, value_t behavior);

struct division_behavior_t;

// Returns the species division behavior of this species belongs to.
struct division_behavior_t *get_species_division_behavior(value_t species);

// Sets the species division behavior of this species.
void set_species_division_behavior(value_t species, value_t behavior);


// --- C o m p a c t   s p e c i e s ---

static const size_t kCompactSpeciesSize = OBJECT_SIZE(3);



// --- S t r i n g ---

static const size_t kStringLengthOffset = 1;
static const size_t kStringCharsOffset = 2;

// Returns the size of a heap string with the given number of characters.
size_t calc_string_size(size_t char_count);

// Sets the length of a heap string.
void set_string_length(value_t value, size_t length);

// Returns the length of a heap string.
size_t get_string_length(value_t value);

// Returns a pointer to the array that holds the contents of this array.
char *get_string_chars(value_t value);

// Stores the contents of this string in the given output.
void get_string_contents(value_t value, string_t *out);


// --- B l o b ---

static const size_t kBlobLengthOffset = 1;
static const size_t kBlobDataOffset = 2;

// Returns the size of a heap blob with the given number of bytes.
size_t calc_blob_size(size_t size);

// Sets the length of a heap blob.
void set_blob_length(value_t value, size_t length);

// Returns the length of a heap blob.
size_t get_blob_length(value_t value);

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

// Returns the length of the given array.
size_t get_array_length(value_t value);

// Sets the length field of an array object.
void set_array_length(value_t value, size_t length);

// Returns the index'th element in the given array.
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

// Returns the array of hash map entries for this map.
value_t get_id_hash_map_entry_array(value_t value);

// Replaces the array of hash map entries for this map.
void set_id_hash_map_entry_array(value_t value, value_t entry_array);

// Returns the size of a map.
size_t get_id_hash_map_size(value_t value);

// Updates the size of a map.
void set_id_hash_map_size(value_t value, size_t size);

// Returns the total capacity of the given map.
size_t get_id_hash_map_capacity(value_t value);

// Sets the total capacity of this map.
void set_id_hash_map_capacity(value_t value, size_t size);

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

// Returns the fields map from the given instance.
value_t get_instance_fields(value_t value);

// Sets the fields map for the given instance.
void set_instance_fields(value_t value, value_t fields);

// Returns the field with the given key from the given instance.
value_t get_instance_field(value_t value, value_t key);

// Sets the field with the given key on the given instance. Returns a signal
// if setting the field failed, for instance if the field map was full.
value_t try_set_instance_field(value_t instance, value_t key, value_t value);


// --- D e b u g ---

// Prints the string representation of the given value on stdout.
void value_print_ln(value_t value);


#endif // _VALUE
