// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Derived object types


#ifndef _DERIVED
#define _DERIVED

#include "check.h"
#include "utils.h"
#include "value.h"


/// ## Derived objects

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
#define DERIVED_OBJECT_FIELD_COUNT(N) (1 + N)

// Returns the offset of the N'th field in a derived object, starting from 0 so
// the header fields aren't included.
#define DERIVED_OBJECT_FIELD_OFFSET(N) ((N * kValueSize) + kDerivedObjectHeaderSize)

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

static const size_t kStackPointerSize = DERIVED_OBJECT_SIZE(0);
static const size_t kStackPointerFieldCount = DERIVED_OBJECT_FIELD_COUNT(0);


/// ## Allocation

// Returns a new stack pointer value within the given memory.
value_t new_derived_stack_pointer(runtime_t *runtime, value_array_t *memory,
    value_t host);

// Allocates a new derived object in the given block of memory and initializes
// it with the given genus and host but requires the caller to complete
// initialization.
//
// Beware that the "size" is not a size in bytes, unlike other allocation
// functions, it is the number of value-size fields of the object.
value_t alloc_derived_object(value_array_t *mem, size_t field_count,
    derived_object_genus_t genus, value_t host);


#endif // _DERIVED
