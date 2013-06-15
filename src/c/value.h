// The basic types of values.

#include "check.h"
#include "globals.h"
#include "utils.h"

#ifndef _VALUE
#define _VALUE

// A tagged value pointer. Values are divided into a hierarchy with three
// different levels. At the top level they are divided into different _domains_
// which can be distinguished by how the pointer is tagged; objects, integers,
// signals, etc. are different domains.
//
// Object pointers can point to objects with different layouts, each such kind
// of object is a _family_; strings and arrays are different families. Which
// family an object belongs to is stored in a field in the object's species.
// 
// Finally, within each family there may be objects with the same basic layout
// but different shared state, each kind of state is a _species_. For instance,
// all user-defined types are within the same family but each different type
// belong to a different species. The species is given by the object's species
// pointer.
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
static value_t non_signal() {
  return new_integer(0);
}


// --- S i g n a l ---

// Enum identifying the type of a signal.
typedef enum {
  // The heap has run out of space.
  scHeapExhausted = 0,
  scSystemError,
  scValidationFailed
} signal_cause_t;

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

// Enum identifying the different families of heap objects.
typedef enum {
  // A species object.
  ofSpecies = 0,
  // A dumb string.
  ofString,
  // A primitive array of values.
  ofArray,
  // Singleton null value.
  ofNull
} object_family_t;

// Number of bytes in an object header.
#define kObjectHeaderSize kValueSize

// Returns the size in bytes of an object with N fields, where the header
// is not counted as a field. 
#define OBJECT_SIZE(N) ((N * kValueSize) + kObjectHeaderSize)

// Converts a pointer to an object into an tagged object value pointer.
static value_t new_object(address_t addr) {
  CHECK_EQ(0, ((address_arith_t) addr) & kDomainTagMask);
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

// The size of a species object.
static const size_t kSpeciesSize = OBJECT_SIZE(1);

// Given a species object, sets the instance type field to the specified value.
void set_species_instance_family(value_t species, object_family_t instance_family);

// Given a species object, returns the instance type field.
object_family_t get_species_instance_family(value_t species);


// --- S t r i n g ---

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


// --- A r r a y ---

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


// --- N u l l ---

static const size_t kNullSize = OBJECT_SIZE(1);

#endif // _VALUE
