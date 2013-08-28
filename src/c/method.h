// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Methods and method lookup. See details in method.md.

#include "process.h"
#include "value-inl.h"

#ifndef _METHOD
#define _METHOD


// A guard score against a value.
typedef uint32_t score_t;


// --- S i g n a t u r e ---

static const size_t kSignatureSize = OBJECT_SIZE(4);
static const size_t kSignatureTagsOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kSignatureParameterCountOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kSignatureMandatoryCountOffset = OBJECT_FIELD_OFFSET(2);
static const size_t kSignatureAllowExtraOffset = OBJECT_FIELD_OFFSET(3);

// The sorted array of signature tags and parameters.
ACCESSORS_DECL(signature, tags);

// The number of parameters defined by this signature.
INTEGER_ACCESSORS_DECL(signature, parameter_count);

// The number of mandatory parameters required by this signature.
INTEGER_ACCESSORS_DECL(signature, mandatory_count);

// Are extra arguments allowed?
INTEGER_ACCESSORS_DECL(signature, allow_extra);

// Returns the number of tags defined by this signature, including optional
// ones.
size_t get_signature_tag_count(value_t self);

// Returns the index'th tag in this signature in the sorted tag order.
value_t get_signature_tag_at(value_t self, size_t index);

// Returns the parameter descriptor for the index'th parameter in sorted tag
// order.
value_t get_signature_parameter_at(value_t self, size_t index);

// The status of a match -- whether it succeeded and if not why.
typedef enum {
  // There was an argument we didn't expect.
  mrUnexpectedArgument,
  // Multiple arguments were passed for the same parameter.
  mrRedundantArgument,
  // This signature expected more arguments than were passed.
  mrMissingArgument,
  // A guard rejected an argument.
  mrGuardRejected,
  // The invocation matched.
  mrMatch,
  //  The invocation matched and had extra arguments which this signature allows.
  mrExtraMatch
} match_result_t;

// Returns true if the given match result represents a match.
bool match_result_is_match(match_result_t value);

// Indicates that no offset was produced for a given argument. This happens if
// the argument doesn't correspond to a parameter, that is if it's an extra
// argument.
static const size_t kNoOffset = ~((size_t) 0);

// Additional info about a match in addition to whether it was successful or not,
// including the score vector and parameter-argument mapping.
typedef struct {
  // On a successful match the scores will be stored here.
  score_t *scores;
  // On a successful match the parameter offsets will be stored here. Any
  // arguments that don't correspond to a parameter will be set to kNoOffset.
  size_t *offsets;
  // The capacity of the scores and offsets vectors.
  size_t capacity;
} match_info_t;

// Initializes a match info struct.
void match_info_init(match_info_t *info, score_t *scores, size_t *offsets,
    size_t capacity);

// Matches the given invocation against this signature. You should not base
// behavior on the exact failure type returned since there can be multiple
// failures and the choice of which one gets returned is arbitrary.
//
// The capacity of the match_info argument must be at least large enough to hold
// info about all the arguments. If the match succeeds it holds the info, if
// it fails the state is unspecified.
match_result_t match_signature(runtime_t *runtime, value_t self, value_t record,
    frame_t *frame, value_t space, match_info_t *match_info);

// The outcome of joining two score vectors. The values encode how they matched:
// if the first bit is set the target was strictly better at some point, if the
// second bit is set the source was strictly better at some point.
typedef enum {
  // The matches were equal.
  jsEqual = 0x0,
  // The target was strictly better than the source.
  jsWorse = 0x1,
  // The source was strictly better than the target.
  jsBetter = 0x2,
  // Neither was strictly better than the other, but they were different.
  jsAmbiguous = 0x3
} join_status_t;

// Joins two score vectors together, writing the result into the target vector.
// The returned value identifies what the outcome of the join was.
join_status_t join_score_vectors(score_t *target, score_t *source, size_t length);


// --- P a r a m e t e r ---

static const size_t kParameterSize = OBJECT_SIZE(3);
static const size_t kParameterGuardOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kParameterIsOptionalOffset = OBJECT_FIELD_OFFSET(1);
static const size_t kParameterIndexOffset = OBJECT_FIELD_OFFSET(2);

// This parameter's guard.
ACCESSORS_DECL(parameter, guard);

// Can this parameter be left empty?
INTEGER_ACCESSORS_DECL(parameter, is_optional);

// Parameter index.
INTEGER_ACCESSORS_DECL(parameter, index);


// --- G u a r d ---

// A parameter guard.

// How this guard matches.
typedef enum {
  // Match by value identity.
  gtEq,
  // Match by 'instanceof'.
  gtIs,
  // Always match.
  gtAny
} guard_type_t;

static const size_t kGuardSize = OBJECT_SIZE(2);
static const size_t kGuardTypeOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kGuardValueOffset = OBJECT_FIELD_OFFSET(1);

// The value of this guard used to match by gtId and gtIs and unused for gtAny.
ACCESSORS_DECL(guard, value);

// The type of match to perform for this guard.
TYPED_ACCESSORS_DECL(guard, guard_type_t, type);

// This guard matched perfectly.
static const score_t gsIdenticalMatch = 0;

// It's not an identical match but the closest possible instanceof-match.
static const score_t gsPerfectIsMatch = 1;

// Score that signifies that a guard didn't match at all.
static const score_t gsNoMatch = 0xFFFFFFFF;

// There was a match but only because extra arguments are allowed so anything
// more specific would match better.
static const score_t gsExtraMatch = 0xFFFFFFFE;

// The guard matched the given value but only because it matches any value so
// anything more specific would match better.
static const score_t gsAnyMatch = 0xFFFFFFFD;

// Matches the given guard against the given value, returning a score for how
// well it matched within the given method space.
score_t guard_match(runtime_t *runtime, value_t guard, value_t value,
    value_t method_space);

// Returns true if the given score represents a match.
bool is_score_match(score_t score);

// Compares which score is best. If a is better than b then it will compare
// smaller, equally good compares equal, and worse compares greater than.
int compare_scores(score_t a, score_t b);


// --- M e t h o d ---

static const size_t kMethodSize = OBJECT_SIZE(2);
static const size_t kMethodSignatureOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kMethodCodeOffset = OBJECT_FIELD_OFFSET(1);

// The method's signature, the arguments it matches.
ACCESSORS_DECL(method, signature);

// The implementation of the method.
ACCESSORS_DECL(method, code);


// --- M e t h o d   s p a c e ---

static const size_t kMethodSpaceSize = OBJECT_SIZE(2);
static const size_t kMethodSpaceInheritanceMapOffset = OBJECT_FIELD_OFFSET(0);
static const size_t kMethodSpaceMethodsOffset = OBJECT_FIELD_OFFSET(1);

// The size of the inheritance map in an empty method space.
static const size_t kInheritanceMapInitialSize = 16;

// The size of the method arra in the empty method space
static const size_t kMethodArrayInitialSize = 16;

// The mapping that defines the inheritance hierarchy within this method space.
ACCESSORS_DECL(method_space, inheritance_map);

// The methods defined within this method space.
ACCESSORS_DECL(method_space, methods);

// Records in the given method space that the subtype inherits directly from
// the supertype. Returns a signal if adding fails, for instance if we run
// out of memory to increase the size of the map.
value_t add_method_space_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype);

// Returns the array buffer of parents of the given protocol.
value_t get_protocol_parents(runtime_t *runtime, value_t space, value_t protocol);

// Add a method to this metod space. Returns a signal if adding fails, for
// instance if we run out of memory to increase the size of the map.
value_t add_method_space_method(runtime_t *runtime, value_t self,
    value_t method);

// Looks up a method in this method space given an invocation record and a stack
// frame. If the match is successful, as a side-effect stores an argument map
// that maps between the result's parameters and argument offsets on the stack.
value_t lookup_method_space_method(runtime_t *runtime, value_t space,
    value_t record, frame_t *frame, value_t *arg_map_out);


// --- I n v o c a t i o n   R e c o r d ---

static const size_t kInvocationRecordSize = OBJECT_SIZE(1);
static const size_t kInvocationRecordArgumentVectorOffset = OBJECT_FIELD_OFFSET(0);

// The array giving the mapping between tag sort order and argument evaulation
// order.
ACCESSORS_DECL(invocation_record, argument_vector);

// Returns the number of argument in this invocation record.
size_t get_invocation_record_argument_count(value_t self);

// Returns the index'th tag in this invocation record.
value_t get_invocation_record_tag_at(value_t self, size_t index);

// Returns the index'th argument offset in this invocation record.
size_t get_invocation_record_offset_at(value_t self, size_t index);

// Constructs an argument vector based on the given array of tags. For instance,
// if given ["c", "a", "b"] returns a vector corresponding to ["a": 1, "b": 0,
// "c": 2] (arguments are counted backwards, 0 being the lasst).
value_t build_invocation_record_vector(runtime_t *runtime, value_t tags);

// Returns the index'th argument to an invocation using this record in sorted
// tag order from the given frame.
value_t get_invocation_record_argument_at(value_t self, frame_t *frame, size_t index);

// Prints an invocation record with a set of arguments.
void print_invocation(value_t record, frame_t *frame);


#endif // _METHOD
