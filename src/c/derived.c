// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "derived.h"
#include "runtime.h"
#include "value-inl.h"

/// ## Derived descriptor

derived_object_genus_t get_derived_object_genus(value_t self) {
  value_t descriptor = get_derived_object_descriptor(self);
  return get_derived_descriptor_genus(descriptor);
}

value_t get_derived_object_host(value_t self) {
  address_t addr = get_derived_object_address(self);
  value_t descriptor = get_derived_object_descriptor(self);
  size_t host_offset = get_derived_descriptor_host_offset(descriptor);
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

value_t new_derived_stack_pointer(runtime_t *runtime, value_array_t *memory, value_t host) {
  return alloc_derived_object(memory, kStackPointerFieldCount, dgStackPointer,
      host);
}

// Returns true iff the given offset is within the bounds of the given host
// object.
static bool is_within_host(value_t host, size_t offset, size_t words) {
  heap_object_layout_t layout;
  get_heap_object_layout(host, &layout);
  return (offset <= layout.size) && (offset + words <= layout.size);
}

value_t alloc_derived_object(value_array_t *memory, size_t field_count,
    derived_object_genus_t genus, value_t host) {
  CHECK_EQ("invalid derived alloc", memory->length, field_count);
  // The descriptor stores the offset of the derived object within the host
  // so we have to determine that. Note that we're juggling both field counts
  // and byte offsets and it's important that they don't get mixed up.
  address_t host_start = get_heap_object_address(host);
  size_t host_offset = ((address_t) memory->start) - host_start;
  size_t size = field_count * kValueSize;
  CHECK_TRUE("derived not within object", is_within_host(host, host_offset, size));
  value_t descriptor = new_derived_descriptor(genus, host_offset);
  value_t result = new_derived_object((address_t) memory->start);
  set_derived_object_descriptor(result, descriptor);
  CHECK_TRUE("derived mispoint", is_same_value(get_derived_object_host(result),
      host));
  return result;
}
