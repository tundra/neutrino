// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Utilities related to runtime-internal conditions.


#ifndef _CONDITION
#define _CONDITION

#include "utils.h"
#include "value.h"

// Creates a new condition with the specified cause and details.
static value_t new_condition_with_details(consition_cause_t cause, uint32_t details) {
  value_t result;
  result.as_condition.domain = vdCondition;
  result.as_condition.cause = cause;
  result.as_condition.details = details;
  return result;
}

// Creates a new condition with the specified cause and no details.
static value_t new_condition(consition_cause_t cause) {
  return new_condition_with_details(cause, 0);
}

// Returns the cause of a condition.
static consition_cause_t get_condition_cause(value_t value) {
  CHECK_DOMAIN(vdCondition, value);
  return value.as_condition.cause;
}

// Returns the string name of a condition cause.
const char *get_condition_cause_name(consition_cause_t cause);

// Returns the details associated with the given condition.
static uint32_t get_condition_details(value_t value) {
  CHECK_DOMAIN(vdCondition, value);
  return value.as_condition.details;
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

// Simple condition constructors. They don't really add much except a tiny bit
// of type checking of details but they're convenient because you can set
// breakpoints in them and so suspend on a particular condition.

// Creates a new SyntaxInvalid condition with the given cause.
static value_t new_invalid_syntax_condition(invalid_syntax_cause_t cause) {
  return new_condition_with_details(ccInvalidSyntax, cause);
}

// Returns the cause of an invalid syntax condition.
invalid_syntax_cause_t get_invalid_syntax_condition_cause(value_t condition);

// Returns the string representation of the cause of an invalid syntax condition.
const char *get_invalid_syntax_cause_name(invalid_syntax_cause_t cause);

// Enumerate the causes of unsupported behavior. See comment for
// ENUM_INVALID_SYNTAX_CAUSES.
#define ENUM_UNSUPPORTED_BEHAVIOR_TYPES(F)                                     \
  F(Unspecified)                                                               \
  F(GetPrimaryType)                                                            \
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
    object_family_t family : 16;
    unsupported_behavior_cause_t cause : 8;
  } decoded;
  uint32_t encoded;
} unsupported_behavior_details_codec_t;

// Creates a new UnsupportedBehavior condition for the given type of behavior.
static value_t new_unsupported_behavior_condition(value_domain_t domain,
    object_family_t family, unsupported_behavior_cause_t cause) {
  unsupported_behavior_details_codec_t codec;
  codec.decoded.domain = domain;
  codec.decoded.family = family;
  codec.decoded.cause = cause;
  return new_condition_with_details(ccUnsupportedBehavior, codec.encoded);
}

// Returns the string representation of the cause of an unsupported behavior
// condition.
const char *get_unsupported_behavior_cause_name(unsupported_behavior_cause_t cause);

// Creates a new heap exhausted condition where the given amount of memory is
// requested.
static value_t new_heap_exhausted_condition(int32_t requested) {
  return new_condition_with_details(ccHeapExhausted, requested);
}

// Creates a new out-of-memory condition.
static value_t new_out_of_memory_condition() {
  return new_condition(ccOutOfMemory);
}

// Creates a new invalid-mode-change condition whose current mode is the given
// value.
static value_t new_invalid_mode_change_condition(value_mode_t current_mode) {
  return new_condition_with_details(ccInvalidModeChange, current_mode);
}

// Creates a new not-deep-frozen condition.
static value_t new_not_deep_frozen_condition() {
  return new_condition(ccNotDeepFrozen);
}

// Creates a new invalid input condition.
static value_t new_invalid_input_condition() {
  return new_condition(ccInvalidInput);
}

// Converter to get and set details for invalid input as a uint32_t.
typedef union {
  char decoded[4];
  uint32_t encoded;
} invalid_input_details_codec_t;

// Creates a new invalid input condition with a hint describing the problem.
static value_t new_invalid_input_condition_with_hint(string_hint_t hint) {
  invalid_input_details_codec_t codec;
  codec.decoded[0] = hint.value[0];
  codec.decoded[1] = hint.value[1];
  codec.decoded[2] = hint.value[2];
  codec.decoded[3] = hint.value[3];
  return new_condition_with_details(ccInvalidInput, codec.encoded);
}

// Enumerate the causes of lookup errors. See comment for
// ENUM_INVALID_SYNTAX_CAUSES.
#define ENUM_LOOKUP_ERROR_CAUSES(F)                                            \
  F(Unspecified)                                                               \
  F(Ambiguity)                                                                 \
  F(Namespace)                                                                 \
  F(NoMatch)                                                                   \
  F(NoSuchStage)                                                               \
  F(UnresolvedImport)

// Reasons why method lookup may fail.
typedef enum {
  __lcFirst__ = -1
#define __GEN_ENUM__(Name) , lc##Name
  ENUM_LOOKUP_ERROR_CAUSES(__GEN_ENUM__)
#undef __GEN_ENUM__
} lookup_error_cause_t;

// Returns the string representation of the cause of a lookup error condition.
const char *get_lookup_error_cause_name(lookup_error_cause_t cause);

// Creates a new lookup error condition.
static value_t new_lookup_error_condition(lookup_error_cause_t cause) {
  return new_condition_with_details(ccLookupError, cause);
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

// Returns the string representation of the cause of a system error condition.
const char *get_system_error_cause_name(system_error_cause_t cause);

// Creates a new system error condition.
static value_t new_system_error_condition(system_error_cause_t cause) {
  return new_condition_with_details(ccSystemError, cause);
}

// Creates a new not-found condition. Not-found is a very generic and
// non-informative condition so it should be caught and converted quickly while
// the context gives the information needed to understand it. If it indicates an
// error that should be propagated it should still be caught and then converted
// to a more informative condition.
static value_t new_not_found_condition() {
  return new_condition(ccNotFound);
}

// Creates a new condition indicating that no builtin with a given name is known
// by the runtime.
static value_t new_unknown_builtin_condition() {
  return new_condition(ccUnknownBuiltin);
}

// Creates a new condition indicating that a signal was raised.
static value_t new_signal_condition() {
  return new_condition(ccSignal);
}

#endif // _CONDITION
