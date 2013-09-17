// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "plankton.h"
#include "value-inl.h"


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
  return stream->cursor < blob_length(stream->blob);
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
  // The buffer we're writing the output to.
  byte_buffer_t *buf;
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
    value_mapping_t *resolver, byte_buffer_t *buf) {
  state->buf = buf;
  state->object_offset = 0;
  state->runtime = runtime;
  state->resolver = resolver;
  TRY_SET(state->ref_map, new_heap_id_hash_map(runtime, 16));
  return success();
}

// Serialize any (non-signal) value on the given buffer.
static value_t value_serialize(value_t value, serialize_state_t *state);

value_t plankton_wire_encode_uint32(byte_buffer_t *buf, uint32_t value) {
  while (value > 0x7F) {
    // As long as the value doesn't fit in 7 bits chop off 7 bits and mark
    // them with a high 1 to indicate that there's more coming.
    byte_t part = value & 0x7F;
    byte_buffer_append(buf, part | 0x80);
    value >>= 7;
  }
  byte_buffer_append(buf, value);
  return success();
}

// Zig-zag encodes a 32-bit signed integer.
static value_t encode_int32(int32_t value, byte_buffer_t *buf) {
  uint32_t zig_zag = (value << 1) ^ (value >> 31);
  return plankton_wire_encode_uint32(buf, zig_zag);
}

static value_t integer_serialize(value_t value, byte_buffer_t *buf) {
  CHECK_DOMAIN(vdInteger, value);
  byte_buffer_append(buf, pInt32);
  // TODO: deal with full size integers, for now just trap loss of data.
  int64_t int_value = get_integer_value(value);
  int32_t trunc = (int32_t) int_value;
  CHECK_EQ("plankton integer overflow", int_value, (int64_t) trunc);
  return encode_int32(trunc, buf);
}

static value_t singleton_serialize(plankton_tag_t tag, byte_buffer_t *buf) {
  byte_buffer_append(buf, tag);
  return success();
}

static value_t array_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofArray, value);
  byte_buffer_append(state->buf, pArray);
  size_t length = get_array_length(value);
  plankton_wire_encode_uint32(state->buf, length);
  for (size_t i = 0; i < length; i++) {
    TRY(value_serialize(get_array_at(value, i), state));
  }
  return success();
}

static value_t map_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofIdHashMap, value);
  byte_buffer_append(state->buf, pMap);
  size_t entry_count = get_id_hash_map_size(value);
  plankton_wire_encode_uint32(state->buf, entry_count);
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

value_t plankton_wire_encode_string(byte_buffer_t *buf, string_t *str) {
  size_t length = string_length(str);
  plankton_wire_encode_uint32(buf, length);
  for (size_t i = 0; i < length; i++)
    byte_buffer_append(buf, string_char_at(str, i));
  return success();
}

static value_t string_serialize(byte_buffer_t *buf, value_t value) {
  CHECK_FAMILY(ofString, value);
  string_t contents;
  get_string_contents(value, &contents);
  byte_buffer_append(buf, pString);
  return plankton_wire_encode_string(buf, &contents);
}

static void register_serialized_object(value_t value, serialize_state_t *state) {
  size_t offset = state->object_offset;
  state->object_offset++;
  set_id_hash_map_at(state->runtime, state->ref_map, value, new_integer(offset));
}

static value_t instance_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofInstance, value);
  value_t ref = get_id_hash_map_at(state->ref_map, value);
  if (is_signal(scNotFound, ref)) {
    // We haven't seen this object before. First we check if it should be an
    // environment object.
    value_t raw_resolved = value_mapping_apply(state->resolver, value, state->runtime);
    if (is_signal(scNothing, raw_resolved)) {
      // It's not an environment object. Just serialize it directly.
      byte_buffer_append(state->buf, pObject);
      byte_buffer_append(state->buf, pNull);
      // Cycles are only allowed through the payload of an object so we only
      // register the object after the header has been written.
      register_serialized_object(value, state);
      return value_serialize(get_instance_fields(value), state);
    } else {
      TRY_DEF(resolved, raw_resolved);
      byte_buffer_append(state->buf, pEnvironment);
      TRY(value_serialize(resolved, state));
      register_serialized_object(value, state);
      return success();
    }
  } else {
    // We've already seen this object; write a reference back to the last time
    // we saw it.
    size_t delta = get_integer_value(ref);
    size_t offset = state->object_offset - delta - 1;
    byte_buffer_append(state->buf, pReference);
    plankton_wire_encode_uint32(state->buf, offset);
    return success();
  }
}

static value_t object_serialize(value_t value, serialize_state_t *state) {
  CHECK_DOMAIN(vdObject, value);
  switch (get_object_family(value)) {
    case ofNull:
      return singleton_serialize(pNull, state->buf);
    case ofBoolean:
      return singleton_serialize(get_boolean_value(value) ? pTrue : pFalse, state->buf);
    case ofArray:
      return array_serialize(value, state);
    case ofIdHashMap:
      return map_serialize(value, state);
    case ofString:
      return string_serialize(state->buf, value);
    case ofInstance:
      return instance_serialize(value, state);
    default:
      return new_signal(scInvalidInput);
  }
}

static value_t value_serialize(value_t data, serialize_state_t *state) {
  switch (get_value_domain(data)) {
    case vdInteger:
      return integer_serialize(data, state->buf);
    case vdObject:
      return object_serialize(data, state);
    default:
      UNREACHABLE("value serialize");
      return new_signal(scUnsupportedBehavior);
  }
}

// Never resolve.
static value_t nothing_mapping(value_t value, runtime_t *runtime, void *data) {
  return new_signal(scNothing);
}

value_t plankton_serialize(runtime_t *runtime, value_mapping_t *resolver_or_null,
    value_t data) {
  // Write the data to a C heap blob.
  byte_buffer_t buf;
  byte_buffer_init(&buf);
  // Use the empty resolver if the resolver pointer is null.
  value_mapping_t resolver;
  if (resolver_or_null == NULL) {
    value_mapping_init(&resolver, nothing_mapping, NULL);
  } else {
    resolver = *resolver_or_null;
  }
  serialize_state_t state;
  TRY(serialize_state_init(&state, runtime, &resolver, &buf));
  TRY(value_serialize(data, &state));
  blob_t buffer_data;
  byte_buffer_flush(&buf, &buffer_data);
  TRY_DEF(result, new_heap_blob_with_data(runtime, &buffer_data));
  byte_buffer_dispose(&buf);
  return result;
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

static uint32_t uint32_deserialize(byte_stream_t *in) {
  byte_t current = 0xFF;
  uint32_t result = 0;
  byte_t offset = 0;
  while ((current & 0x80) != 0) {
    current = byte_stream_read(in);
    result = result | ((current & 0x7f) << offset);
    offset += 7;
  }
  return result;
}

// Reads an int32 into an integer object.
static value_t int32_deserialize(byte_stream_t *in) {
  uint32_t zig_zag = uint32_deserialize(in);
  int32_t value = (zig_zag >> 1) ^ (-(zig_zag & 1));
  return new_integer(value);
}

static value_t array_deserialize(deserialize_state_t *state) {
  size_t length = uint32_deserialize(state->in);
  TRY_DEF(result, new_heap_array(state->runtime, length));
  for (size_t i = 0; i < length; i++) {
    TRY_DEF(value, value_deserialize(state));
    set_array_at(result, i, value);
  }
  return result;
}

static value_t map_deserialize(deserialize_state_t *state) {
  size_t entry_count = uint32_deserialize(state->in);
  TRY_DEF(result, new_heap_id_hash_map(state->runtime, 16));
  for (size_t i = 0; i < entry_count; i++) {
    TRY_DEF(key, value_deserialize(state));
    TRY_DEF(value, value_deserialize(state));
    set_id_hash_map_at(state->runtime, result, key, value);
  }
  return result;
}

static value_t string_deserialize(deserialize_state_t *state) {
  string_buffer_t buf;
  string_buffer_init(&buf);
  size_t length = uint32_deserialize(state->in);
  for (size_t i = 0; i < length; i++)
    string_buffer_putc(&buf, byte_stream_read(state->in));
  string_t contents;
  string_buffer_flush(&buf, &contents);
  TRY_DEF(result, new_heap_string(state->runtime, &contents));
  string_buffer_dispose(&buf);
  return result;
}

// Grabs and returns the next object index.
static size_t acquire_object_index(deserialize_state_t *state) {
  return state->object_offset++;
}

static value_t object_deserialize(deserialize_state_t *state) {
  size_t offset = acquire_object_index(state);
  // Read the header before creating the instance.
  TRY_DEF(header, value_deserialize(state));
  TRY_DEF(result, new_object_with_type(state->runtime, header));
  TRY(set_id_hash_map_at(state->runtime, state->ref_map, new_integer(offset),
      result));
  TRY_DEF(payload, value_deserialize(state));
  TRY(set_object_contents(state->runtime, result, payload));
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

static value_t reference_deserialize(deserialize_state_t *state) {
  size_t offset = uint32_deserialize(state->in);
  size_t index = state->object_offset - offset - 1;
  value_t result = get_id_hash_map_at(state->ref_map, new_integer(index));
  CHECK_FALSE("missing reference", is_signal(scNotFound, result));
  return result;
}

static value_t value_deserialize(deserialize_state_t *state) {
  switch (byte_stream_read(state->in)) {
    case pInt32:
      return int32_deserialize(state->in);
    case pNull:
      return ROOT(state->runtime, null);
    case pTrue:
      return runtime_bool(state->runtime, true);
    case pFalse:
      return runtime_bool(state->runtime, false);
    case pArray:
      return array_deserialize(state);
    case pMap:
      return map_deserialize(state);
    case pString:
      return string_deserialize(state);
    case pObject:
      return object_deserialize(state);
    case pReference:
      return reference_deserialize(state);
    case pEnvironment:
      return environment_deserialize(state);
    default:
      return new_signal(scInvalidInput);
  }
}

// Always report invalid input.
static value_t invalid_input_mapping(value_t value, runtime_t *runtime, void *data) {
  return new_signal(scInvalidInput);
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
    value_mapping_init(&access, invalid_input_mapping, NULL);
  } else {
    access = *access_or_null;
  }
  deserialize_state_t state;
  TRY(deserialize_state_init(&state, runtime, &access, &in));
  return value_deserialize(&state);
}
