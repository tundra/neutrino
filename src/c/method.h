//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Methods and method lookup. See details in method.md.


#ifndef _METHOD
#define _METHOD

#include "process.h"
#include "value-inl.h"

INTERFACE(sigmap_input_o);

// Matches the index'th argument to this call against the given guard, storing
// the result in the score_out parameter. If the match fails for whatever reason
// a signal is returned.
typedef value_t (*sigmap_input_match_value_at_m)(sigmap_input_o *self, size_t index,
    value_t guard, value_t space, value_t *score_out);

struct sigmap_input_o_vtable_t {
  sigmap_input_match_value_at_m match_value_at;
};

// Input to a signature map lookup. This interface represents a potentially
// partial input, that is, input where you can match against the values but not
// necessarily get access to them.
struct sigmap_input_o {
  INTERFACE_HEADER(sigmap_input_o);
  // The ambience we're looking up within.
  value_t ambience;
  // Cache of the ambience's runtime.
  runtime_t *runtime;
  // The call tags that describe the invocation being looked up.
  value_t tags;
  // Cache of the number of arguments.
  size_t argc;
};

// Initializes an input struct appropriately.
void sigmap_input_init(sigmap_input_o *self, value_t ambience, value_t tags);

// Returns the number of arguments of this call.
size_t sigmap_input_get_argument_count(sigmap_input_o *self);

// Returns the tag of the index'th argument in sorted order.
value_t sigmap_input_get_tag_at(sigmap_input_o *self, size_t index);

// Returns the stack offset of the index'th argument in sorted tag order.
size_t sigmap_input_get_offset_at(sigmap_input_o *self, size_t index);

INTERFACE(total_sigmap_input_o);

// Returns the value of the index'th argument in sorted tag order.
typedef value_t (*total_sigmap_input_get_value_at_m)(total_sigmap_input_o *self,
    size_t index);

struct total_sigmap_input_o_vtable_t {
  sigmap_input_o_vtable_t super;
  total_sigmap_input_get_value_at_m get_value_at;
};

// A total sigmap input is one that is not partial so that you can get access
// to the values of all arguments.
struct total_sigmap_input_o {
  SUB_INTERFACE_HEADER(total_sigmap_input_o, sigmap_input_o);
};

// Returns the index'th argument value in sorted tag order.
value_t total_sigmap_input_get_value_at(total_sigmap_input_o *self, size_t index);

IMPLEMENTATION(frame_sigmap_input_o, total_sigmap_input_o);

// Total sigmap input that gets the values of arguments from a frame.
struct frame_sigmap_input_o {
  IMPLEMENTATION_HEADER(frame_sigmap_input_o, total_sigmap_input_o);
  frame_t *frame;
};

// Creates a new frame sigmap input.
frame_sigmap_input_o frame_sigmap_input_new(value_t ambience, value_t tags,
    frame_t *frame);

IMPLEMENTATION(call_data_sigmap_input_o, total_sigmap_input_o);

// Total sigmap input that gets the values of arguments from a call data
// object.
struct call_data_sigmap_input_o {
  IMPLEMENTATION_HEADER(call_data_sigmap_input_o, total_sigmap_input_o);
  value_t call_data;
};

// Creates a new call data sigmap input.
call_data_sigmap_input_o call_data_sigmap_input_new(value_t ambience,
    value_t call_data);

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
size_t get_signature_tag_count(value_t self);

// Returns the index'th tag in this signature in the sorted tag order.
value_t get_signature_tag_at(value_t self, size_t index);

// Returns the parameter descriptor for the index'th parameter in sorted tag
// order.
value_t get_signature_parameter_at(value_t self, size_t index);

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

// Matches the given invocation against this signature. You should not base
// behavior on the exact failure type returned since there can be multiple
// failures and the choice of which one gets returned is arbitrary.
//
// The capacity of the match_info argument must be at least large enough to hold
// info about all the arguments. If the match succeeds it holds the info, if
// it fails the state is unspecified.
value_t match_signature(value_t self, sigmap_input_o *input, value_t space,
    match_info_t *match_info, match_result_t *match_out);

// Matches the given tags against this signature. If this signature can be
// matched successfully against some invocation with these tags this will store
// a successful match value in the match out parameter, otherwise it will store
// a match failure value.
value_t match_signature_tags(value_t self, value_t tags,
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
value_t guard_match(value_t guard, value_t value, sigmap_input_o *lookup_input,
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
static const size_t kMethodCodeOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kMethodSyntaxOffset = HEAP_OBJECT_FIELD_OFFSET(2);
static const size_t kMethodModuleFragmentOffset = HEAP_OBJECT_FIELD_OFFSET(3);
static const size_t kMethodFlagsOffset = HEAP_OBJECT_FIELD_OFFSET(4);

// The method's signature, the arguments it matches.
ACCESSORS_DECL(method, signature);

// The compiled method implementation. This may or may not be set.
ACCESSORS_DECL(method, code);

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

// Opaque datatype used during signature map lookup.
FORWARD(sigmap_state_t);
INTERFACE(sigmap_output_o);
INTERFACE(sigmap_thunk_o);

// A callback called by do_signature_map_lookup to perform the traversal that
// produces the signature maps to lookup within.
typedef value_t (sigmap_state_callback_t)(sigmap_state_t *state);

// Prepares a signature map lookup and then calls the callback which must
// traverse the signature maps to include in the lookup and invoke
// continue_signature_map_lookup for each of them. When the callback returns
// this function completes the lookup and returns the result or a condition as
// appropriate.
value_t do_sigmap_lookup(sigmap_thunk_o *callback, sigmap_input_o *input,
    sigmap_output_o *output);

// Includes the given signature map in the lookup associated with the given
// lookup state.
value_t continue_sigmap_lookup(sigmap_state_t *state, value_t sigmap,
    value_t space);

// Returns the argument map that describes the location of the arguments of the
// signature map lookup match recorded in the given lookup state. If there is
// no match recorded an arbitrary non-condition value will be returned.
value_t get_sigmap_lookup_argument_map(sigmap_state_t *state);


// --- M e t h o d   s p a c e ---

static const size_t kMethodspaceSize = HEAP_OBJECT_SIZE(3);
static const size_t kMethodspaceInheritanceOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kMethodspaceMethodsOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kMethodspaceImportsOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The size of the inheritance map in an empty method space.
static const size_t kInheritanceMapInitialSize = 16;

// The size of the imports array buffer in an empty method space
static const size_t kImportsArrayInitialSize = 16;

// The mapping that defines the inheritance hierarchy within this method space.
ACCESSORS_DECL(methodspace, inheritance);

// The methods defined within this method space.
ACCESSORS_DECL(methodspace, methods);

// The method spaces imported by this one.
ACCESSORS_DECL(methodspace, imports);

// Records in the given method space that the subtype inherits directly from
// the supertype. Returns a condition if adding fails, for instance if we run
// out of memory to increase the size of the map.
value_t add_methodspace_inheritance(runtime_t *runtime, value_t self,
    value_t subtype, value_t supertype);

// Records in the given method space that it imports the given other methodspace.
value_t add_methodspace_import(runtime_t *runtime, value_t self, value_t imported);

// Returns the array buffer of parents of the given type.
value_t get_type_parents(runtime_t *runtime, value_t space, value_t type);

// Add a method to this metod space. Returns a condition if adding fails, for
// instance if we run out of memory to increase the size of the map.
value_t add_methodspace_method(runtime_t *runtime, value_t self,
    value_t method);

// Looks up a method in the given fragment given a set of inputs. This is the
// full lookup that also searches the subject's origin. If the match is
// successful, as a side-effect stores an argument map that maps between the
// result's parameters and argument offsets on the stack.
//
// TODO: this is an approximation of the intended lookup mechanism and should be
//   revised later on, for instance to not hard-code the subject origin lookup.
value_t lookup_method_full_with_helper(total_sigmap_input_o *input,
    value_t fragment, value_t helper, value_t *arg_map_out);

value_t lookup_methodspace_method(sigmap_input_o *input, value_t methodspace,
    value_t *arg_map_out);

// Scans through the stack looking for signal handler methods, returning the
// best match if there is one otherwise a LookupError condition. The signal
// handler that contains the method is stored in handler_out.
value_t lookup_signal_handler_method(sigmap_input_o *input, frame_t *frame,
    value_t *handler_out, value_t *arg_map_out);

// Given a module fragment, returns the cache of methodspaces to look up within.
// If the cache has not yet been created, create it. Creating the cache may
// cause a condition to be returned.
value_t get_or_create_module_fragment_methodspaces_cache(runtime_t *runtime,
    value_t fragment);


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

static const size_t kCallTagsSize = HEAP_OBJECT_SIZE(1);
static const size_t kCallTagsEntriesOffset = HEAP_OBJECT_FIELD_OFFSET(0);

// The array giving the mapping between tag sort order and argument evaulation
// order.
ACCESSORS_DECL(call_tags, entries);

// Returns the number of argument in this call tags object.
size_t get_call_tags_entry_count(value_t self);

// Returns the index'th tag in this call tag set.
value_t get_call_tags_tag_at(value_t self, size_t index);

// Returns the index'th argument offset in this call tag set.
size_t get_call_tags_offset_at(value_t self, size_t index);

// Constructs an argument vector based on the given array of tags. For instance,
// if given ["c", "a", "b"] returns a vector corresponding to ["a": 1, "b": 0,
// "c": 2] (arguments are counted backwards, 0 being the last).
value_t build_call_tags_entries(runtime_t *runtime, value_t tags);

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
value_t get_call_data_value_at(value_t self, size_t param_index);


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
void operation_print_open_on(value_t self, print_on_context_t *context);

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


// ## Virtuals

// Function called with additional matches that are not strictly better or worse
// than the best seen so far.
typedef value_t (*sigmap_output_add_ambiguous_m)(sigmap_output_o *self, value_t value);

// Function called the first time a result is found that is strictly better
// than any matches previously seen.
typedef value_t (*sigmap_output_add_better_m)(sigmap_output_o *self, value_t value);

// Returns the result of this lookup.
typedef value_t (*sigmap_output_get_result_m)(sigmap_output_o *self);

// Resets the lookup state.
typedef void (*sigmap_output_reset_m)(sigmap_output_o *self);

// Collection of virtual functions for working with a result collector.
struct sigmap_output_o_vtable_t {
  sigmap_output_add_ambiguous_m add_ambiguous;
  sigmap_output_add_better_m add_better;
  sigmap_output_get_result_m get_result;
  sigmap_output_reset_m reset;
};

// Virtual signature map lookup result collector.
struct sigmap_output_o {
  INTERFACE_HEADER(sigmap_output_o);
};

#endif // _METHOD
