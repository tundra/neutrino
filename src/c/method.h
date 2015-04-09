//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Methods and method lookup. See details in method.md.


#ifndef _METHOD
#define _METHOD

#include "process.h"
#include "value-inl.h"

// --- S i g n a t u r e ---

static const size_t kSignatureSize = HEAP_OBJECT_SIZE(4);
static const size_t kSignatureTagsOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kSignatureParameterCountOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kSignatureMandatoryCountOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kSignatureAllowExtraOffset = HEAP_OBJECT_FIELD_OFFSET(3);

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
int64_t get_signature_tag_count(value_t self);

// Returns the index'th tag in this signature in the sorted tag order.
value_t get_signature_tag_at(value_t self, int64_t index);

// Returns the parameter descriptor for the index'th parameter in sorted tag
// order.
value_t get_signature_parameter_at(value_t self, int64_t index);

// The status of a match -- whether it succeeded and if not why.
typedef enum {
  // A match result that is distinct from all the others and never set by a
  // match function; can be used for initialization and testing.
  __mrNone__,
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
  value_t *scores;
  // On a successful match the parameter offsets will be stored here. Any
  // arguments that don't correspond to a parameter will be set to kNoOffset.
  size_t *offsets;
  // The capacity of the scores and offsets vectors.
  size_t capacity;
} match_info_t;

// Initializes a match info struct.
void match_info_init(match_info_t *info, value_t *scores, size_t *offsets,
    size_t capacity);

// "Static" information about an invocation. For most calls it will always be
// the same across all invocations at the same site.
typedef struct {
  // The ambience surrounding the invocation.
  value_t ambience;
  // Argument tags.
  value_t tags;
  // If this is a next call, the argument guards used to direct which are the
  // next methods.
  value_t next_guards;
} sigmap_input_layout_t;

sigmap_input_layout_t sigmap_input_layout_new(value_t ambience, value_t tags,
    value_t next_guards);

// Matches the given invocation, the arguments passed as a frame, against this
// signature. You should not base behavior on the exact failure type returned
// since there can be multiple failures and the choice of which one gets
// returned is arbitrary.
//
// The capacity of the match_info argument must be at least large enough to hold
// info about all the arguments. If the match succeeds it holds the info, if
// it fails the state is unspecified.
value_t match_signature_from_frame(value_t self, sigmap_input_layout_t *layout,
    frame_t *frame, value_t space, match_info_t *match_info,
    match_result_t *match_out);

// Matches the given invocation, the arguments passed as a call data object,
// against this signature. You should not base behavior on the exact failure
// type returned since there can be multiple failures and the choice of which
// one gets returned is arbitrary.
//
// The capacity of the match_info argument must be at least large enough to hold
// info about all the arguments. If the match succeeds it holds the info, if
// it fails the state is unspecified.
value_t match_signature_from_call_data(value_t self, sigmap_input_layout_t *layout,
    value_t call_data, value_t space, match_info_t *match_info,
    match_result_t *match_out);

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
join_status_t join_score_vectors(value_t *target, value_t *source, size_t length);


// --- P a r a m e t e r ---

static const size_t kParameterSize = HEAP_OBJECT_SIZE(4);
static const size_t kParameterGuardOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kParameterIsOptionalOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kParameterIndexOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kParameterTagsOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// This parameter's guard.
ACCESSORS_DECL(parameter, guard);

// This parameter's tags.
ACCESSORS_DECL(parameter, tags);

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

static const size_t kGuardSize = HEAP_OBJECT_SIZE(2);
static const size_t kGuardTypeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kGuardValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The value of this guard used to match by gtId and gtIs and unused for gtAny.
ACCESSORS_DECL(guard, value);

// The type of match to perform for this guard.
TYPED_ACCESSORS_DECL(guard, guard_type_t, type);

// Matches the given guard against the given value, returning a condition that
// indicates whether the match was successful and, if it was, storing the score
// in the out argument for how well it matched within the given method space.
value_t guard_match(value_t guard, value_t value, runtime_t *runtime,
    value_t methodspace, value_t *score_out);


// ## Method

// Flags that describe a method.
typedef enum {
  // This method delegates to a lambda. If lookup results in a method with this
  // flag the lookup process should take an extra step to resolve the method in
  // the subject lambda.
  mfLambdaDelegate = 0x01,
  // This method delegates to a block. If lookup results in a method with this
  // flag the lookup process should take an extra step to resolve the method in
  // the subject block's home methodspace.
  mfBlockDelegate = 0x02
} method_flag_t;

static const size_t kMethodSize = HEAP_OBJECT_SIZE(5);
static const size_t kMethodSignatureOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kMethodCodePtrOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kMethodSyntaxOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kMethodModuleFragmentOffset = HEAP_OBJECT_FIELD_OFFSET(3);
static const size_t kMethodFlagsOffset = HEAP_OBJECT_FIELD_OFFSET(4);

// The method's signature, the arguments it matches.
ACCESSORS_DECL(method, signature);

// The compiled method implementation. This field holds a freeze cheat that
// points to the compiled code, or not, depending on whether it's been compiled
// yet.
ACCESSORS_DECL(method, code_ptr);

// The syntax of the implementation of the method.
ACCESSORS_DECL(method, syntax);

// The module fragment that provides context for this method.
ACCESSORS_DECL(method, module_fragment);

// Flags describing this method.
ACCESSORS_DECL(method, flags);

// Compiles a method syntax tree into a method object.
value_t compile_method_ast_to_method(runtime_t *runtime, value_t method_ast,
    value_t fragment);


/// ## Signature map
///
/// A signature map is a mapping from signatures to values. Lookup works by
/// giving an invocation which is then matched against the map's signatures and
/// the value of the best match is the result. Method lookup is one example of
/// signature map lookup but the mechanism is more general than that.
///
/// Typically a signature map lookup happens across multiple maps, for instance
/// all the sets of methods imported into a particular scope.

static const size_t kSignatureMapSize = HEAP_OBJECT_SIZE(1);
static const size_t kSignatureMapEntriesOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The size of the method array buffer in an empty signature map.
static const size_t kMethodArrayInitialSize = 16;

// The signature+value entries in this signature map. The entries are stored as
// alternating signatures and values.
ACCESSORS_DECL(signature_map, entries);

// Adds a mapping to the given signature map, expanding it if necessary. Returns
// a condition on failure.
value_t add_to_signature_map(runtime_t *runtime, value_t map, value_t signature,
    value_t value);

INTERFACE(sigmap_output_o);
INTERFACE(sigmap_thunk_o);


// --- M e t h o d   s p a c e ---

static const size_t kMethodspaceSize = HEAP_OBJECT_SIZE(4);
static const size_t kMethodspaceInheritanceOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kMethodspaceMethodsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kMethodspaceParentOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kMethodspaceCachePtrOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// The size of the inheritance map in an empty method space.
static const size_t kInheritanceMapInitialSize = 16;

// The size of the imports array buffer in an empty method space
static const size_t kImportsArrayInitialSize = 16;

// The mapping that defines the inheritance hierarchy within this method space.
ACCESSORS_DECL(methodspace, inheritance);

// The methods defined within this method space.
ACCESSORS_DECL(methodspace, methods);

// The optional parent methodspace whose state is implicitly included in this
// one.
ACCESSORS_DECL(methodspace, parent);

// Freeze-cheat pointer to the cache used to speed up lookup.
ACCESSORS_DECL(methodspace, cache_ptr);

// Records in the given method space that the subtype inherits directly from
// the supertype. Returns a condition if adding fails, for instance if we run
// out of memory to increase the size of the map.
value_t add_methodspace_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype);

// Returns the array buffer of parents of the given type.
value_t get_type_parents(runtime_t *runtime, value_t space, value_t type);

// Add a method to this metod space. Returns a condition if adding fails, for
// instance if we run out of memory to increase the size of the map.
value_t add_methodspace_method(runtime_t *runtime, value_t self,
    value_t method);

// Given a selector, returns the methods that might match that selector.
value_t get_or_create_methodspace_selector_slice(runtime_t *runtime, value_t self,
    value_t selector);

// Clears any caches that depend on the current state of this methodspace.
// Ideally there wouldn't be any caches in a mutable methodspace but that'll
// have to be cleaned up later.
void invalidate_methodspace_caches(value_t self);

// Looks up a method in the given fragment given a set of inputs, including
// resolving lambda and block methods. If the match is successful, as a
// side-effect stores an argument map that maps between the result's parameters
// and argument offsets on the stack.
//
// TODO: this is an approximation of the intended lookup mechanism and should be
//   revised later on, for instance to not hard-code the subject origin lookup.
value_t lookup_method_full_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t *arg_map_out);

// Looks up a method in the given fragment given a set of inputs, including
// resolving lambda and block methods. If the match is successful, as a
// side-effect stores an argument map that maps between the result's parameters
// and argument offsets on the stack.
//
// TODO: this is an approximation of the intended lookup mechanism and should be
//   revised later on, for instance to not hard-code the subject origin lookup.
value_t lookup_method_full_from_value_array(sigmap_input_layout_t *layout,
    value_t values, value_t *arg_map_out);

// Looks up a value in a methodspace, taking input from the given frame.
value_t lookup_methodspace_method_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t methodspace, value_t *arg_map_out);

// Scans through the stack looking for signal handler methods, returning the
// best match if there is one otherwise a LookupError condition. The signal
// handler that contains the method is stored in handler_out.
value_t lookup_signal_handler_method_from_frame(sigmap_input_layout_t *layout,
    frame_t *frame, value_t *handler_out, value_t *arg_map_out);


/// ## Call tags
///
/// A call tags object is a mapping from parameter tag names to the offset
/// of the corresponding argument in the evaluation order. For instance, the
/// invocation
///
///     (z: $a, x: $b, y: $c)
///
/// would evaluate its argument in order `$a`, then `$b`, then `$c` so the
/// call tags would be
///
///     {z: 0, x: 1, y: 2}
///
/// except that evaluation order is counted backwards (that is, it is counted
/// by where the argument ends up on the stack relative to the top. So the
/// actual mapping is
///
///     {z: 2, x: 1, y: 0}
///
/// To make lookup more efficient, however, call tags are sorted by tag so in
/// reality it's a pair array of
///
///     [(x, 1), (y, 0), (z, 2)]
///
/// Because signatures are also sorted by tag they can be matched just by
/// scanning through both sequentially.

static const size_t kCallTagsSize = HEAP_OBJECT_SIZE(3);
static const size_t kCallTagsEntriesOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kCallTagsSubjectOffsetOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kCallTagsSelectorOffsetOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The array giving the mapping between tag sort order and argument evaulation
// order.
ACCESSORS_DECL(call_tags, entries);

// If one of the parameters is tagged with the selector key this holds the
// associated evaluation offset. If not it's nothing.
ACCESSORS_DECL(call_tags, selector_offset);

// If one of the parameters is tagged with the subject key this holds the
// associated evaluation offset. If not it's nothing.
ACCESSORS_DECL(call_tags, subject_offset);

// Returns the number of argument in this call tags object.
int64_t get_call_tags_entry_count(value_t self);

// Returns the index'th tag in this call tag set.
value_t get_call_tags_tag_at(value_t self, int64_t index);

// Returns the index'th argument offset in this call tag set.
int64_t get_call_tags_offset_at(value_t self, int64_t index);

// Constructs an argument vector based on the given array of tags. For instance,
// if given ["c", "a", "b"] returns a vector corresponding to ["a": 1, "b": 0,
// "c": 2] (arguments are counted backwards, 0 being the last).
value_t build_call_tags_entries(runtime_t *runtime, value_t tags);

// Check that the tags in the given call tags entry array are all unique, that
// is, no value occurs more than once. Having the same tag appear more than once
// is bad because not only is it invalid according to the language but because
// we sort the tags using a sort function whose behavior is undefined on equal
// values it opens the possibility of some really subtle bugs.
void check_call_tags_entries_unique(value_t tags);

// Prints a call tags object with a set of arguments.
void print_call_on(value_t tags, frame_t *frame, string_buffer_t *out);


/// ## Call data
///
/// Call data encapsulates both the tags and arguments of a call.

static const size_t kCallDataSize = HEAP_OBJECT_SIZE(2);
static const size_t kCallDataTagsOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kCallDataValuesOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// This call's tags.
ACCESSORS_DECL(call_data, tags);

// The arguments to this call as an array.
ACCESSORS_DECL(call_data, values);

// Returns the value given for the index'th parameter in this call data.
value_t get_call_data_value_at(value_t self, int64_t param_index);


// --- O p e r a t i o n ---

// The different types of operations that are possible.
typedef enum {
  // An assignment: $this.foo := 4
  otAssign = 1,
  // Function call: $fun(1, 2)
  otCall = 2,
  // Collection indexing: $elms[4]
  otIndex = 3,
  // Infix operation: $foo.bar(), $a + $b
  otInfix = 4,
  // Prefix operation: !$foo
  otPrefix = 5,
  // Property access: $p.x
  otProperty = 6,
  // Suffix operation: $foo!
  otSuffix = 7
} operation_type_t;

static const size_t kOperationSize = HEAP_OBJECT_SIZE(2);
static const size_t kOperationTypeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOperationValueOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The tag enum that identifies which kind of operation this is.
INTEGER_ACCESSORS_DECL(operation, type);

// The optional value that provides data for the operation.
ACCESSORS_DECL(operation, value);

// Prints the beginning of an invocation for this kind of operation. For
// instance, the beginning of an infix operation "foo" would be ".foo(". The
// beginning of an index operation would be "[".
void operation_print_open_on(value_t self, value_t transport,
    print_on_context_t *context);

// Prints the end of an invocation for this kind of operation. For instance, the
// end of an infix operation "foo" would be ")". The end of an index operation
// would be "]".
void operation_print_close_on(value_t self, print_on_context_t *context);


// --- B u i l t i n   m a r k e r ---

static const size_t kBuiltinMarkerSize = HEAP_OBJECT_SIZE(1);
static const size_t kBuiltinMarkerNameOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The name of the builtin that applies to the element annotated with this
// marker.
ACCESSORS_DECL(builtin_marker, name);


/// ## Builtin implementation

static const size_t kBuiltinImplementationSize = HEAP_OBJECT_SIZE(4);
static const size_t kBuiltinImplementationNameOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kBuiltinImplementationCodeOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kBuiltinImplementationArgumentCountOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kBuiltinImplementationMethodFlagsOffset = HEAP_OBJECT_FIELD_OFFSET(3);

// The name of this builtin.
ACCESSORS_DECL(builtin_implementation, name);

// The code block that implements this builtin.
ACCESSORS_DECL(builtin_implementation, code);

// The number of positional arguments expected by the implementation of this
// builtin.
INTEGER_ACCESSORS_DECL(builtin_implementation, argument_count);

// The code block that implements this builtin.
ACCESSORS_DECL(builtin_implementation, method_flags);


#endif // _METHOD
