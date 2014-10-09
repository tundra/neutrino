//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Ctrino is the proxy object that gives source code access to calling into the
// C runtime.

#ifndef _CTRINO
#define _CTRINO

static const size_t kCtrinoSize = HEAP_OBJECT_SIZE(0);

// Adds the ctrino method to the given methodspace.
value_t add_ctrino_builtin_methods(runtime_t *runtime, value_t space);


/// ## C object species

// Description of the layout of a C object. Note that unlike fully general
// built-in objects, all C object instances of the same species must have the
// same layout. To get variable size data or field count use a value field that
// holds a blob or array.
typedef struct {
  // Size in bytes of the data stored in the object.
  size_t data_size;
  // Number of bytes of data aligned to a full value size.
  size_t aligned_data_size;
  // Number of field values stored in the object.
  size_t value_count;
} c_object_info_t;

// Sets the fields of an info value.
void c_object_info_init(c_object_info_t *info, size_t data_size, size_t value_count);

static const size_t kCObjectSpeciesSize = SPECIES_SIZE(2);
static const size_t kCObjectSpeciesDataSizeOffset = SPECIES_FIELD_OFFSET(0);
static const size_t kCObjectSpeciesValueCountOffset = SPECIES_FIELD_OFFSET(1);

// Size in bytes of the data stored in instances.
SPECIES_ACCESSORS_DECL(c_object, data_size);

// Number of field values stored in instances.
SPECIES_ACCESSORS_DECL(c_object, value_count);

// Updates the given info to hold a description of instances of this species.
void get_c_object_species_object_info(value_t self, c_object_info_t *info_out);


/// ## C object

size_t calc_c_object_size(c_object_info_t *info);

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
