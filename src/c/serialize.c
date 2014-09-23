//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "runtime.h"
#include "safe-inl.h"
#include "serialize.h"
#include "try-inl.h"
#include "value-inl.h"
#include "plankton.h"


// --- B y t e   s t r e a m ---

// A stream that allows bytes to be read one at a time from a blob.
typedef struct {
  blob_t *blob;
  size_t cursor;
} byte_stream_t;

// Initializes a stream to read from the beginning of the given blob.
void byte_stream_init(byte_stream_t *stream, blob_t *blob) {
  stream->blob = blob;
  stream->cursor = 0;
}

// Returns true iff data can be read from this stream.
bool byte_stream_has_more(byte_stream_t *stream) {
  return stream->cursor < blob_byte_length(stream->blob);
}

// Returns the next byte from the given byte stream.
byte_t byte_stream_read(byte_stream_t *stream) {
  CHECK_TRUE("byte stream empty", byte_stream_has_more(stream));
  byte_t result = blob_byte_at(stream->blob, stream->cursor);
  stream->cursor++;
  return result;
}


// --- S e r i a l i z e ---

// Collection of state used when serializing data.
typedef struct {
  // The plankton assembler.
  pton_assembler_t *assm;
  // Map from objects we've seen to their index.
  value_t ref_map;
  // The offset of the next object we're going to write.
  size_t object_offset;
  // The runtime to use for heap allocation.
  runtime_t *runtime;
  // The resolver used to determine when to reference an object through the
  // environment.
  value_mapping_t *resolver;
} serialize_state_t;

value_t value_mapping_apply(value_mapping_t *mapping, value_t value, runtime_t *runtime) {
  return (mapping->function)(value, runtime, mapping->data);
}

// Initialize serialization state.
static value_t serialize_state_init(serialize_state_t *state, runtime_t *runtime,
    value_mapping_t *resolver, pton_assembler_t *assm) {
  state->assm = assm;
  state->object_offset = 0;
  state->runtime = runtime;
  state->resolver = resolver;
  TRY_SET(state->ref_map, new_heap_id_hash_map(runtime, 16));
  return success();
}

// Serialize any (non-condition) value on the given buffer.
static value_t value_serialize(value_t value, serialize_state_t *state);

static value_t integer_serialize(value_t value, pton_assembler_t *assm) {
  CHECK_DOMAIN(vdInteger, value);
  int64_t int_value = get_integer_value(value);
  pton_assembler_emit_int64(assm, int_value);
  return success();
}

static value_t array_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofArray, value);
  size_t length = get_array_length(value);
  pton_assembler_begin_array(state->assm, length);
  for (size_t i = 0; i < length; i++) {
    TRY(value_serialize(get_array_at(value, i), state));
  }
  return success();
}

static value_t map_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofIdHashMap, value);
  size_t entry_count = get_id_hash_map_size(value);
  pton_assembler_begin_map(state->assm, entry_count);
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, value);
  size_t entries_written = 0;
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    TRY(value_serialize(key, state));
    TRY(value_serialize(value, state));
    entries_written++;
  }
  CHECK_EQ("serialized map length", entry_count, entries_written);
  return success();
}

static value_t string_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofString, value);
  string_t contents;
  get_string_contents(value, &contents);
  pton_assembler_emit_default_string(state->assm, contents.chars, contents.length);
  return success();
}

static void register_serialized_object(value_t value, serialize_state_t *state) {
  size_t offset = state->object_offset;
  state->object_offset++;
  set_id_hash_map_at(state->runtime, state->ref_map, value, new_integer(offset));
}

static value_t instance_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofInstance, value);
  value_t ref = get_id_hash_map_at(state->ref_map, value);
  if (in_condition_cause(ccNotFound, ref)) {
    // We haven't seen this object before. First we check if it should be an
    // environment object.
    value_t raw_resolved = value_mapping_apply(state->resolver, value, state->runtime);
    if (in_condition_cause(ccNothing, raw_resolved)) {
      // It's not an environment object. Just serialize it directly.
      pton_assembler_begin_object(state->assm);
      pton_assembler_emit_null(state->assm);
      // Cycles are only allowed through the payload of an object so we only
      // register the object after the header has been written.
      register_serialized_object(value, state);
      return value_serialize(get_instance_fields(value), state);
    } else {
      TRY_DEF(resolved, raw_resolved);
      pton_assembler_begin_environment_reference(state->assm);
      TRY(value_serialize(resolved, state));
      register_serialized_object(value, state);
      return success();
    }
  } else {
    // We've already seen this object; write a reference back to the last time
    // we saw it.
    size_t delta = get_integer_value(ref);
    size_t offset = state->object_offset - delta - 1;
    pton_assembler_emit_reference(state->assm, offset);
    return success();
  }
}

static value_t object_serialize(value_t value, serialize_state_t *state) {
  CHECK_DOMAIN(vdHeapObject, value);
  switch (get_heap_object_family(value)) {
    case ofArray:
      return array_serialize(value, state);
    case ofIdHashMap:
      return map_serialize(value, state);
    case ofString:
      return string_serialize(value, state);
    case ofInstance:
      return instance_serialize(value, state);
    default:
      return new_invalid_input_condition();
  }
}

static value_t custom_tagged_serialize(value_t value, serialize_state_t *state) {
  CHECK_DOMAIN(vdCustomTagged, value);
  switch (get_custom_tagged_phylum(value)) {
    case tpNull:
      pton_assembler_emit_null(state->assm);
      return success();
    case tpBoolean:
      pton_assembler_emit_bool(state->assm, get_boolean_value(value));
      return success();
    default:
      return new_invalid_input_condition();
  }
}

static value_t value_serialize(value_t data, serialize_state_t *state) {
  value_domain_t domain = get_value_domain(data);
  switch (domain) {
    case vdInteger:
      return integer_serialize(data, state->assm);
    case vdHeapObject:
      return object_serialize(data, state);
    case vdCustomTagged:
      return custom_tagged_serialize(data, state);
    default:
      UNREACHABLE("value serialize");
      return new_unsupported_behavior_condition(domain, __ofUnknown__,
          ubPlanktonSerialize);
  }
}

// Never resolve.
static value_t nothing_mapping(value_t value, runtime_t *runtime, void *data) {
  return new_condition(ccNothing);
}

value_t plankton_serialize(runtime_t *runtime, value_mapping_t *resolver_or_null,
    value_t data) {
  pton_assembler_t *assm = pton_new_assembler();
  E_BEGIN_TRY_FINALLY();
    // Use the empty resolver if the resolver pointer is null.
    value_mapping_t resolver;
    if (resolver_or_null == NULL) {
      value_mapping_init(&resolver, nothing_mapping, NULL);
    } else {
      resolver = *resolver_or_null;
    }
    serialize_state_t state;
    E_TRY(serialize_state_init(&state, runtime, &resolver, assm));
    E_TRY(value_serialize(data, &state));
    memory_block_t code = pton_assembler_peek_code(assm);
    blob_t blob;
    blob_init(&blob, code.memory, code.size);
    E_TRY_DEF(result, new_heap_blob_with_data(runtime, &blob));
    E_RETURN(result);
  E_FINALLY();
    pton_dispose_assembler(assm);
  E_END_TRY_FINALLY();
}

void value_mapping_init(value_mapping_t *resolver,
    value_mapping_function_t function, void *data) {
  resolver->function = function;
  resolver->data = data;
}

// --- D e s e r i a l i z e ---

// Collection of state used when serializing data.
typedef struct {
  // The buffer we're reading input from.
  byte_stream_t *in;
  // Map from object offsets we've seen to their values.
  value_t ref_map;
  // The offset of the next object we're going to write.
  size_t object_offset;
  // The runtime to use for heap allocation.
  runtime_t *runtime;
  // Environment access used to resolve environment references.
  value_mapping_t *access;
} deserialize_state_t;

// Initialize deserialization state.
static value_t deserialize_state_init(deserialize_state_t *state, runtime_t *runtime,
    value_mapping_t *access, byte_stream_t *in) {
  state->in = in;
  state->object_offset = 0;
  state->runtime = runtime;
  state->access = access;
  TRY_SET(state->ref_map, new_heap_id_hash_map(runtime, 16));
  return success();
}


// Reads the next value from the stream.
static value_t value_deserialize(deserialize_state_t *state);

static value_t array_deserialize(size_t length, deserialize_state_t *state) {
  TRY_DEF(result, new_heap_array(state->runtime, length));
  for (size_t i = 0; i < length; i++) {
    TRY_DEF(value, value_deserialize(state));
    set_array_at(result, i, value);
  }
  return result;
}

static value_t map_deserialize(size_t entry_count, deserialize_state_t *state) {
  TRY_DEF(result, new_heap_id_hash_map(state->runtime, 16));
  for (size_t i = 0; i < entry_count; i++) {
    TRY_DEF(key, value_deserialize(state));
    TRY_DEF(value, value_deserialize(state));
    set_id_hash_map_at(state->runtime, result, key, value);
  }
  return result;
}

static value_t default_string_deserialize(pton_instr_t *instr, deserialize_state_t *state) {
  string_t contents;
  contents.chars = (char*) instr->payload.default_string_data.contents;
  contents.length = instr->payload.default_string_data.length;
  return new_heap_string(state->runtime, &contents);
}

// Grabs and returns the next object index.
static size_t acquire_object_index(deserialize_state_t *state) {
  return state->object_offset++;
}

static value_t object_deserialize(deserialize_state_t *state) {
  size_t offset = acquire_object_index(state);
  // Read the header before creating the instance.
  TRY_DEF(header, value_deserialize(state));
  TRY_DEF(result, new_heap_object_with_type(state->runtime, header));
  TRY(set_id_hash_map_at(state->runtime, state->ref_map, new_integer(offset),
      result));
  TRY_DEF(payload, value_deserialize(state));
  TRY(set_heap_object_contents(state->runtime, result, payload));
  return result;
}

static value_t environment_deserialize(deserialize_state_t *state) {
  TRY_DEF(key, value_deserialize(state));
  size_t offset = acquire_object_index(state);
  TRY_DEF(result, value_mapping_apply(state->access, key, state->runtime));
  TRY(set_id_hash_map_at(state->runtime, state->ref_map, new_integer(offset),
      result));
  return result;
}

#include "utils/log.h"

int count = 0;
static value_t reference_deserialize(size_t offset, deserialize_state_t *state) {
  size_t index = state->object_offset - offset - 1;
  value_t result = get_id_hash_map_at(state->ref_map, new_integer(index));
  CHECK_FALSE("missing reference", in_condition_cause(ccNotFound, result));
  return result;
}

static value_t value_deserialize(deserialize_state_t *state) {
  byte_stream_t *in = state->in;
  blob_t *blob = in->blob;
  size_t cursor = in->cursor;
  pton_instr_t instr;
  if (!pton_decode_next_instruction(blob->data + cursor, blob->byte_length - cursor,
      &instr))
    return new_invalid_input_condition();
  in->cursor += instr.size;
  switch (instr.opcode) {
    case PTON_OPCODE_INT64:
      return new_integer(instr.payload.int64_value);
    case PTON_OPCODE_NULL:
      return null();
    case PTON_OPCODE_BOOL:
      return new_boolean(instr.payload.bool_value);
    case PTON_OPCODE_BEGIN_ARRAY:
      return array_deserialize(instr.payload.array_length, state);
    case PTON_OPCODE_BEGIN_MAP:
      return map_deserialize(instr.payload.map_size, state);
    case PTON_OPCODE_DEFAULT_STRING:
      return default_string_deserialize(&instr, state);
    case PTON_OPCODE_BEGIN_OBJECT:
      return object_deserialize(state);
    case PTON_OPCODE_REFERENCE:
      return reference_deserialize(instr.payload.reference_offset, state);
    case PTON_OPCODE_BEGIN_ENVIRONMENT_REFERENCE:
      return environment_deserialize(state);
    default:
      return new_invalid_input_condition();
  }
}

// Always report invalid input.
static value_t unknown_input_mapping(value_t value, runtime_t *runtime, void *data) {
  return new_heap_unknown(runtime, RSTR(runtime, environment_reference), value);
}

value_t plankton_deserialize(runtime_t *runtime, value_mapping_t *access_or_null,
    value_t blob) {
  // Make a byte stream out of the blob.
  blob_t data;
  get_blob_data(blob, &data);
  byte_stream_t in;
  byte_stream_init(&in, &data);
  // Use a failing environment accessor if the access pointer is null.
  value_mapping_t access;
  if (access_or_null == NULL) {
    value_mapping_init(&access, unknown_input_mapping, NULL);
  } else {
    access = *access_or_null;
  }
  deserialize_state_t state;
  TRY(deserialize_state_init(&state, runtime, &access, &in));
  return value_deserialize(&state);
}
