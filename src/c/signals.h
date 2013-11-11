// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Utilities related to runtime-internal signals. Not to be confused with
// system signals which live in <signal.h>, which is why this file is called
// signalS, plural.


#ifndef _SIGNALS
#define _SIGNALS

#include "value.h"

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

// Enumerate the causes of invalid syntax. They should be sorted except for the
// first one, Unspecified, which gets value 0 and hence matches the case where
// no cause is specified (since it defaults to 0).
#define ENUM_INVALID_SYNTAX_CAUSES(F)                                          \
  F(Unspecified)                                                               \
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

// Simple signal constructors. They don't really add much except a tiny bit
// of type checking of details but they're convenient because you can set
// breakpoints in them and so suspend on a particular signal.

// Creates a new SyntaxInvalid signal with the given cause.
static value_t new_invalid_syntax_signal(invalid_syntax_cause_t cause) {
  return new_signal_with_details(scInvalidSyntax, cause);
}

// Returns the cause of an invalid syntax signal.
invalid_syntax_cause_t get_invalid_syntax_signal_cause(value_t signal);

// Returns the string representation of the cause of an invalid syntax signal.
const char *get_invalid_syntax_cause_name(invalid_syntax_cause_t cause);

// Enumerate the causes of unsupported behavior. See comment for
// ENUM_INVALID_SYNTAX_CAUSES.
#define ENUM_UNSUPPORTED_BEHAVIOR_TYPES(F)                                     \
  F(Unspecified)                                                               \
  F(GetProtocol)                                                               \
  F(NewObjectWithType)                                                         \
  F(PlanktonSerialize)                                                         \
  F(SetContents)                                                               \
  F(TransientIdentityHash)

// Behaviors that some objects may not support.
typedef enum {
  __ubFirst__ = -1
#define __GEN_ENUM__(Name) , ub##Name
  ENUM_UNSUPPORTED_BEHAVIOR_TYPES(__GEN_ENUM__)
#undef __GEN_ENUM__
} unsupported_behavior_cause_t;

typedef union {
  struct {
    value_domain_t domain : 8;
    object_family_t family : 8;
    unsupported_behavior_cause_t cause : 8;
  } decoded;
  uint32_t encoded;
} unsupported_behavior_details_codec_t;

// Creates a new UnsupportedBehavior signal for the given type of behavior.
static value_t new_unsupported_behavior_signal(value_domain_t domain,
    object_family_t family, unsupported_behavior_cause_t cause) {
  unsupported_behavior_details_codec_t codec;
  codec.decoded.domain = domain;
  codec.decoded.family = family;
  codec.decoded.cause = cause;
  return new_signal_with_details(scUnsupportedBehavior, codec.encoded);
}

// Returns the string representation of the cause of an unsupported behavior signal.
const char *get_unsupported_behavior_cause_name(unsupported_behavior_cause_t cause);

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

// Creates a new not-deep-frozen signal.
static value_t new_not_deep_frozen_signal() {
  return new_signal(scNotDeepFrozen);
}

// Creates a new invalid input signal.
static value_t new_invalid_input_signal() {
  return new_signal(scInvalidInput);
}

// Enumerate the causes of lookup errors. See comment for
// ENUM_INVALID_SYNTAX_CAUSES.
#define ENUM_LOOKUP_ERROR_CAUSES(F)                                            \
  F(Unspecified)                                                               \
  F(NoMatch)                                                                   \
  F(Ambiguity)

// Reasons why method lookup may fail.
typedef enum {
  __lcFirst__ = -1
#define __GEN_ENUM__(Name) , lc##Name
  ENUM_LOOKUP_ERROR_CAUSES(__GEN_ENUM__)
#undef __GEN_ENUM__
} lookup_error_cause_t;

// Returns the string representation of the cause of a lookup error signal.
const char *get_lookup_error_cause_name(lookup_error_cause_t cause);

// Creates a new lookup error signal.
static value_t new_lookup_error_signal(lookup_error_cause_t cause) {
  return new_signal_with_details(scLookupError, cause);
}

// Enumerate the causes of system error. They should be sorted except for the
// first one, Unspecified, which gets value 0 and hence matches the case where
// no cause is specified (since it defaults to 0).
#define ENUM_SYSTEM_ERROR_CAUSES(F)                                            \
  F(Unspecified)                                                               \
  F(AllocationFailed)                                                          \
  F(FileNotFound)

// Reasons for a system error.
typedef enum {
  __seFirst__ = -1
#define __GEN_ENUM__(Name) , se##Name
  ENUM_SYSTEM_ERROR_CAUSES(__GEN_ENUM__)
#undef __GEN_ENUM__
} system_error_cause_t;

// Returns the string representation of the cause of a system error signal.
const char *get_system_error_cause_name(system_error_cause_t cause);

// Creates a new system error signal.
static value_t new_system_error_signal(system_error_cause_t cause) {
  return new_signal_with_details(scSystemError, cause);
}

#endif // _SIGNALS
