// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Utilities related to runtime-internal signals. Not to be confused with
// system signals which live in <signal.h>, which is why this file is called
// signalS, plural.

#include "value.h"

#ifndef _SIGNALS
#define _SIGNALS

// Creates a new signal with the specified cause and details.
static value_t new_signal_with_details(signal_cause_t cause, uint32_t details) {
  return (value_t) {.as_signal={vdSignal, cause, details}};
}

// Creates a new signal with the specified cause and no details.
static value_t new_signal(signal_cause_t cause) {
  return new_signal_with_details(cause, 0);
}

// Returns the cause of a signal.
static signal_cause_t get_signal_cause(value_t value) {
  CHECK_DOMAIN(vdSignal, value);
  return value.as_signal.cause;
}

// Returns the string name of a signal cause.
const char *get_signal_cause_name(signal_cause_t cause);

// Returns the details associated with the given signal.
static uint32_t get_signal_details(value_t value) {
  CHECK_DOMAIN(vdSignal, value);
  return value.as_signal.details;
}

#define ENUM_INVALID_SYNTAX_CAUSES(F)                                          \
  F(ExpectedSymbol)                                                            \
  F(NotSyntax)                                                                 \
  F(SymbolAlreadyBound)                                                        \
  F(SymbolNotBound)

// Reasons for syntax to be invalid.
typedef enum {
  __isFirst__ = -1
#define __GEN_ENUM__(Name) , is##Name
  ENUM_INVALID_SYNTAX_CAUSES(__GEN_ENUM__)
#undef __GEN_ENUM__
} invalid_syntax_cause_t;

// Creates a new SyntaxInvalid signal with the given cause.
static value_t new_invalid_syntax_signal(invalid_syntax_cause_t cause) {
  return new_signal_with_details(scInvalidSyntax, cause);
}

// Returns the cause of an invalid syntax signal.
invalid_syntax_cause_t get_invalid_syntax_signal_cause(value_t signal);

// Returns the string representation of the cause of an invalid syntax signal.
const char *get_invalid_syntax_cause_name(invalid_syntax_cause_t cause);

// Creates a new heap exhausted signal where the given amount of memory is
// requested.
static value_t new_heap_exhausted_signal(int32_t requested) {
  return new_signal_with_details(scHeapExhausted, requested);
}

// Creates a new out-of-memory signal.
static value_t new_out_of_memory_signal() {
  return new_signal(scOutOfMemory);
}

// Creates a new invalid-mode-change signal whose current mode is the given
// value.
static value_t new_invalid_mode_change_signal(value_mode_t current_mode) {
  return new_signal_with_details(scInvalidModeChange, current_mode);
}

#endif // _SIGNALS
