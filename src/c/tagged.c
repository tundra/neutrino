// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "behavior.h"
#include "tagged.h"
#include "utils.h"

// --- S t a g e   o f f s e t ---

void stage_offset_print_on(value_t value, string_buffer_t *buf, print_flags_t flags) {
  int32_t offset = get_stage_offset_value(value);
  char c;
  if (offset < 0) {
    c = '@';
    offset = -offset;
  } else {
    c = '$';
    offset += 1;
  }
  for (int32_t i = 0; i < offset; i++)
    string_buffer_putc(buf, c);
}

value_t stage_offset_ordering_compare(value_t a, value_t b) {
  CHECK_PHYLUM(tpStageOffset, a);
  CHECK_PHYLUM(tpStageOffset, b);
  int32_t a_stage = get_stage_offset_value(a);
  int32_t b_stage = get_stage_offset_value(b);
  return int_to_ordering(a_stage - b_stage);
}


// --- N o t h i n g ---

void nothing_print_on(value_t value, string_buffer_t *buf, print_flags_t flags) {
  string_buffer_printf(buf, "#<nothing>");
}
