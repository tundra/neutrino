// Copyright 2014 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "derived-inl.h"
#include "process.h"
#include "runtime.h"
#include "tagged-inl.h"
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
#define __EMIT_GENUS_CASE__(Name, name, SC) case dg##Name: return #Name;
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


// Expands to a trivial implementation of print_on.
#define TRIVIAL_DERIVED_PRINT_ON_IMPL(Genus, genus)                            \
void genus##_print_on(value_t value, print_on_context_t *context) {            \
  CHECK_GENUS(dg##Genus, value);                                               \
  string_buffer_printf(context->buf, "#<" #genus " ~%w>", value.encoded);      \
}                                                                              \
SWALLOW_SEMI(tdpo)


/// ## Stack pointer

TRIVIAL_DERIVED_PRINT_ON_IMPL(StackPointer, stack_pointer);

value_t stack_pointer_validate(value_t self) {
  VALIDATE_GENUS(dgStackPointer, self);
  return success();
}


/// ## Barrier state

DERIVED_ACCESSORS_IMPL(BarrierState, barrier_state, acNoCheck, 0, Payload, payload);
DERIVED_ACCESSORS_IMPL(BarrierState, barrier_state, acInDomainOpt, vdDerivedObject,
    Previous, previous);

void barrier_state_register(value_t self, value_t stack, value_t payload) {
  CHECK_DOMAIN(vdDerivedObject, self);
  CHECK_FAMILY(ofStack, stack);
  set_barrier_state_payload(self, payload);
  set_barrier_state_previous(self, get_stack_top_barrier(stack));
  set_stack_top_barrier(stack, self);
}

void barrier_state_unregister(value_t self, value_t stack) {
  CHECK_DOMAIN(vdDerivedObject, self);
  CHECK_FAMILY(ofStack, stack);
  CHECK_TRUE("unregistering non-top barrier",
      is_same_value(self, get_stack_top_barrier(stack)));
  set_stack_top_barrier(stack, get_barrier_state_previous(self));
}

value_t barrier_state_validate(value_t self) {
  VALIDATE_DOMAIN_OPT(vdDerivedObject, get_barrier_state_previous(self));
  return success();
}


/// ## Escape state

DERIVED_ACCESSORS_IMPL(EscapeState, escape_state, acNoCheck, 0, StackPointer, stack_pointer);
DERIVED_ACCESSORS_IMPL(EscapeState, escape_state, acNoCheck, 0, FramePointer, frame_pointer);
DERIVED_ACCESSORS_IMPL(EscapeState, escape_state, acNoCheck, 0, LimitPointer, limit_pointer);
DERIVED_ACCESSORS_IMPL(EscapeState, escape_state, acNoCheck, 0, Flags, flags);
DERIVED_ACCESSORS_IMPL(EscapeState, escape_state, acNoCheck, 0, Pc, pc);

value_t escape_state_validate(value_t self) {
  TRY(barrier_state_validate(self));
  VALIDATE_DOMAIN(vdInteger, get_escape_state_stack_pointer(self));
  VALIDATE_DOMAIN(vdInteger, get_escape_state_frame_pointer(self));
  VALIDATE_DOMAIN(vdInteger, get_escape_state_limit_pointer(self));
  VALIDATE_PHYLUM(tpFlagSet, get_escape_state_flags(self));
  VALIDATE_DOMAIN(vdInteger, get_escape_state_pc(self));
  return success();
}

void escape_state_init(value_t self, size_t stack_pointer, size_t frame_pointer,
    size_t limit_pointer, value_t flags, size_t pc) {
  CHECK_DOMAIN(vdDerivedObject, self);
  set_escape_state_stack_pointer(self, new_integer(stack_pointer));
  set_escape_state_frame_pointer(self, new_integer(frame_pointer));
  set_escape_state_limit_pointer(self, new_integer(limit_pointer));
  set_escape_state_flags(self, flags);
  set_escape_state_pc(self, new_integer(pc));
}


/// ## Escape section

TRIVIAL_DERIVED_PRINT_ON_IMPL(EscapeSection, escape_section);

value_t escape_section_validate(value_t self) {
  VALIDATE_GENUS(dgEscapeSection, self);
  TRY(escape_state_validate(self));
  return success();
}


/// ## Refraction point

DERIVED_ACCESSORS_IMPL(RefractionPoint, refraction_point, acNoCheck, 0, FramePointer,
    frame_pointer);

void refraction_point_init(value_t self, frame_t *frame) {
  value_t *stack_bottom = frame_get_stack_piece_bottom(frame);
  size_t offset = frame->frame_pointer - stack_bottom;
  set_refraction_point_frame_pointer(self, new_integer(offset));
}

value_t refraction_point_validate(value_t self) {
  VALIDATE_DOMAIN(vdInteger, get_refraction_point_frame_pointer(self));
  return success();
}


/// ## Block section

TRIVIAL_DERIVED_PRINT_ON_IMPL(BlockSection, block_section);

DERIVED_ACCESSORS_IMPL(BlockSection, block_section, acInFamily, ofMethodspace,
    Methodspace, methodspace);

value_t block_section_validate(value_t self) {
  VALIDATE_GENUS(dgBlockSection, self);
  TRY(barrier_state_validate(self));
  TRY(refraction_point_validate(self));
  VALIDATE_FAMILY_OPT(ofMethodspace, get_block_section_methodspace(self));
  return success();
}


/// ## Code shard section

TRIVIAL_DERIVED_PRINT_ON_IMPL(CodeShardSection, code_shard_section);

DERIVED_ACCESSORS_IMPL(CodeShardSection, code_shard_section, acInFamily,
    ofCodeBlock, Code, code);

value_t code_shard_section_validate(value_t self) {
  VALIDATE_GENUS(dgCodeShardSection, self);
  TRY(barrier_state_validate(self));
  TRY(refraction_point_validate(self));
  VALIDATE_FAMILY_OPT(ofCodeBlock, get_code_shard_section_code(self));
  return success();
}


/// ## Signal handler section

TRIVIAL_DERIVED_PRINT_ON_IMPL(SignalHandlerSection, signal_handler_section);

DERIVED_ACCESSORS_IMPL(SignalHandlerSection, signal_handler_section, acInFamily,
    ofMethodspace, Methods, methods);

value_t signal_handler_section_validate(value_t self) {
  VALIDATE_GENUS(dgSignalHandlerSection, self);
  TRY(escape_state_validate(self));
  TRY(refraction_point_validate(self));
  VALIDATE_FAMILY_OPT(ofMethodspace, get_signal_handler_section_methods(self));
  return success();
}

void on_signal_handler_section_exit(value_t self) {
  // Nothing to do.
}


/// ## Descriptors

#define __GENUS_STRUCT__(Genus, genus, SC)                                     \
genus_descriptor_t k##Genus##Descriptor = {                                    \
  dg##Genus,                                                                   \
  (k##Genus##BeforeFieldCount + k##Genus##AfterFieldCount + 1),                \
  k##Genus##BeforeFieldCount,                                                  \
  k##Genus##AfterFieldCount,                                                   \
  genus##_validate,                                                            \
  genus##_print_on,                                                            \
  SC(on_##genus##_exit, NULL)                                                  \
};
ENUM_DERIVED_OBJECT_GENERA(__GENUS_STRUCT__)
#undef __GENUS_STRUCT__

genus_descriptor_t *get_genus_descriptor(derived_object_genus_t genus) {
  switch (genus) {
#define __GENUS_CASE__(Genus, genus, SC) case dg##Genus: return &k##Genus##Descriptor;
ENUM_DERIVED_OBJECT_GENERA(__GENUS_CASE__)
#undef __GENUS_CASE__
    default:
      return NULL;
  }
}
