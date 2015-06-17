//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Ctrino is the proxy object that gives source code access to calling into the
// C runtime.

#ifndef _CTRINO
#define _CTRINO

#include "builtin.h"
#include "plugin.h"

// Check that fails unless the object is in the specified family.
#define CHECK_C_OBJECT_TAG(TAG, EXPR)                                               \
IF_CHECKS_ENABLED(__CHECK_CLASS__(uint32_t, TAG, EXPR, get_c_object_int_tag))

#define FOR_EACH_BUILTIN_TAG(F)                                                \
  F(Ctrino, 9dcda24f)

// Enum identifying the type of a condition.
typedef enum {
  __btFirst__ = -1
#define DECLARE_BUILTIN_TAG_ENUM(Builtin, UID) , bt##Builtin = (0x##UID)
  FOR_EACH_BUILTIN_TAG(DECLARE_BUILTIN_TAG_ENUM)
#undef DECLARE_BUILTIN_TAG_ENUM
} builtin_tag_t;

// Adds the ctrino method to the given methodspace.
value_t create_ctrino_factory(runtime_t *runtime, value_t space);


/// ## C object species

static const size_t kCObjectSpeciesSize = SPECIES_SIZE(4);
static const size_t kCObjectSpeciesDataSizeOffset = SPECIES_FIELD_OFFSET(0);
static const size_t kCObjectSpeciesValueCountOffset = SPECIES_FIELD_OFFSET(1);
static const size_t kCObjectSpeciesTypeOffset = SPECIES_FIELD_OFFSET(2);
static const size_t kCObjectSpeciesTagOffset = SPECIES_FIELD_OFFSET(3);

// Size in bytes of the data stored in instances.
SPECIES_ACCESSORS_DECL(c_object, data_size);

// Number of field values stored in instances.
SPECIES_ACCESSORS_DECL(c_object, value_count);

// The type tag used to identify instances of this species.
SPECIES_ACCESSORS_DECL(c_object, type);

// The type tag used to identify instances of this species.
SPECIES_ACCESSORS_DECL(c_object, tag);

// Updates the given info to hold a description of instances of this species.
// This function chases moved objects to it works during gc.
void get_c_object_species_layout_gc_tolerant(value_t self, c_object_layout_t *info_out);

// Returns the offset within instances of this c object species where the values
// section starts.
size_t get_c_object_species_values_offset(value_t self);


/// ## C object

// The C object header is kind of cheating since C objects can have nonempty
// data sections and the object model doesn't allow for data between the species
// and the data. However, as long as these values don't need to be seen by the
// gc, that is, it's just integers and such, we're okay.
static const size_t kCObjectHeaderSize = HEAP_OBJECT_SIZE(1);
static const size_t kCObjectModeOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// Returns the size in bytes of a c object with the given descriptor.
size_t calc_c_object_size(c_object_layout_t *layout);

// Returns the address at which the data section of the given c object starts.
byte_t *get_c_object_data_start(value_t self);

// Returns the underlying data array for the given c object. The result is
// backed by the value so changing the contents will change the object. Also,
// a GC invalidates the array. The value must be mutable.
blob_t get_mutable_c_object_data(value_t self);

// Returns the address at which the value section of the given c object starts.
value_t *get_c_object_value_start(value_t self);

// Returns the underlying value array for the given c object. The result is
// backed by the value so changing the contents will change the object. Also,
// a GC invalidates the array. The value must be mutable.
value_array_t get_mutable_c_object_values(value_t self);

// Returns the index'th value from the given c object.
value_t get_c_object_value_at(value_t self, size_t index);

#endif // _CTRINO
