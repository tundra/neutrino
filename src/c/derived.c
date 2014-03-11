// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "derived.h"
#include "runtime.h"
#include "value-inl.h"


/// ## Derived object anchor

derived_object_genus_t get_derived_object_genus(value_t self) {
  value_t anchor = get_derived_object_anchor(self);
  return get_derived_object_anchor_genus(anchor);
}

value_t get_derived_object_host(value_t self) {
  address_t addr = get_derived_object_address(self);
  value_t anchor = get_derived_object_anchor(self);
  size_t host_offset = get_derived_object_anchor_host_offset(anchor);
  address_t host_addr = addr - host_offset;
  return new_heap_object(host_addr);
}

const char *get_derived_object_genus_name(derived_object_genus_t genus) {
  switch (genus) {
#define __EMIT_GENUS_CASE__(Name, name) case dg##Name: return #Name;
  ENUM_DERIVED_OBJECT_GENERA(__EMIT_GENUS_CASE__)
#undef __EMIT_GENUS_CASE__
    default:
      return "invalid genus";
  }
}


/// ## Allocation

value_t new_derived_stack_pointer(runtime_t *runtime, value_array_t memory, value_t host) {
  return alloc_derived_object(memory, &kStackPointerDescriptor, host);
}

// Returns true iff the given offset is within the bounds of the given host
// object.
static bool is_within_host(value_t host, size_t offset, size_t words) {
  heap_object_layout_t layout;
  get_heap_object_layout(host, &layout);
  return (offset <= layout.size) && (offset + words <= layout.size);
}

value_t alloc_derived_object(value_array_t memory, genus_descriptor_t *desc,
    value_t host) {
  CHECK_EQ("invalid derived alloc", memory.length, desc->field_count);
  // The anchor stores the offset of the derived object within the host
  // so we have to determine that. Note that we're juggling both field counts
  // and byte offsets and it's important that they don't get mixed up.
  // The offset is measured not from the start of the derived object but the
  // location of the anchor, which is before_field_count fields into the object.
  address_t anchor_addr = (address_t) (memory.start + desc->before_field_count);
  address_t host_addr = get_heap_object_address(host);
  size_t host_offset = anchor_addr - host_addr;
  size_t size = desc->field_count * kValueSize;
  CHECK_TRUE("derived not within object", is_within_host(host, host_offset, size));
  value_t anchor = new_derived_object_anchor(desc->genus, host_offset);
  value_t result = new_derived_object(anchor_addr);
  set_derived_object_anchor(result, anchor);
  CHECK_TRUE("derived mispoint", is_same_value(get_derived_object_host(result),
      host));
  return result;
}


/// ## Descriptors

#define __GENUS_STRUCT__(Genus, genus)                                         \
genus_descriptor_t k##Genus##Descriptor = {                                    \
  dg##Genus,                                                                   \
  (k##Genus##BeforeFieldCount + k##Genus##AfterFieldCount + 1),                \
  k##Genus##BeforeFieldCount,                                                  \
  k##Genus##AfterFieldCount                                                    \
};
ENUM_DERIVED_OBJECT_GENERA(__GENUS_STRUCT__)
#undef __GENUS_STRUCT__

genus_descriptor_t *get_genus_descriptor(derived_object_genus_t genus) {
  switch (genus) {
#define __GENUS_CASE__(Genus, genus) case dg##Genus: return &k##Genus##Descriptor;
ENUM_DERIVED_OBJECT_GENERA(__GENUS_CASE__)
#undef __GENUS_CASE__
    default:
      return NULL;
  }
}
