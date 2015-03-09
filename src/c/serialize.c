//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "alloc.h"
#include "freeze.h"
#include "runtime.h"
#include "safe-inl.h"
#include "serialize.h"
#include "try-inl.h"
#include "value-inl.h"
#include "plankton.h"


// --- B y t e   s t r e a m ---

// A stream that allows bytes to be read one at a time from a blob.
typedef struct {
  blob_t blob;
  size_t cursor;
} byte_stream_t;

// Initializes a stream to read from the beginning of the given blob.
static void byte_stream_init(byte_stream_t *stream, blob_t blob) {
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
} serialize_state_t;

object_factory_t new_object_factory(new_empty_object_t *new_empty_object,
    set_object_fields_t *set_object_fields, void *data) {
  object_factory_t result = {new_empty_object, set_object_fields, data};
  return result;
}

value_t value_mapping_apply(value_mapping_t *mapping, value_t value, runtime_t *runtime) {
  return (mapping->function)(value, runtime, mapping->data);
}

// Initialize serialization state.
static value_t serialize_state_init(serialize_state_t *state, runtime_t *runtime,
    pton_assembler_t *assm) {
  state->assm = assm;
  state->object_offset = 0;
  state->runtime = runtime;
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

static value_t map_contents_serialize(size_t entry_count, id_hash_map_iter_t *iter,
    serialize_state_t *state) {
  size_t entries_written = 0;
  while (id_hash_map_iter_advance(iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(iter, &key, &value);
    TRY(value_serialize(key, state));
    TRY(value_serialize(value, state));
    entries_written++;
  }
  CHECK_EQ("serialized map length", entry_count, entries_written);
  return success();
}

static value_t map_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofIdHashMap, value);
  size_t entry_count = get_id_hash_map_size(value);
  pton_assembler_begin_map(state->assm, entry_count);
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, value);
  return map_contents_serialize(entry_count, &iter, state);
}

static value_t string_serialize(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofUtf8, value);
  utf8_t contents = get_utf8_contents(value);
  pton_assembler_emit_default_string(state->assm, contents.chars, string_size(contents));
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
    value_t fields = get_instance_fields(value);
    size_t fieldc = get_id_hash_map_size(fields);
    pton_assembler_begin_seed(state->assm, 1, fieldc);
    pton_assembler_emit_null(state->assm);
    // Cycles are only allowed through the payload of an object so we only
    // register the object after the header has been written.
    register_serialized_object(value, state);
    id_hash_map_iter_t iter;
    id_hash_map_iter_init(&iter, fields);
    return map_contents_serialize(fieldc, &iter, state);
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
    case ofUtf8:
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

value_t plankton_serialize(runtime_t *runtime, value_t data) {
  pton_assembler_t *assm = pton_new_assembler();
  blob_t blob;
  memory_block_t code;
  TRY_FINALLY {
    serialize_state_t state;
    E_TRY(serialize_state_init(&state, runtime, assm));
    E_TRY(value_serialize(data, &state));
    code = pton_assembler_peek_code(assm);
    blob = new_blob(code.memory, code.size);
    E_TRY_DEF(result, new_heap_blob_with_data(runtime, blob));
    E_RETURN(result);
  } FINALLY {
    pton_dispose_assembler(assm);
  } YRT
}

void value_mapping_init(value_mapping_t *resolver,
    value_mapping_function_t function, void *data) {
  resolver->function = function;
  resolver->data = data;
}

// --- D e s e r i a l i z e ---

// Collection of state used when serializing data.
typedef struct {
  // The input blob.
  safe_value_t s_blob;
  // The buffer we're reading input from.
  byte_stream_t *in;
  // Map from object offsets we've seen to their values.
  value_t ref_map;
  // The offset of the next object we're going to write.
  size_t object_offset;
  // The runtime to use for heap allocation.
  runtime_t *runtime;
  // The factory used to construct object instances.
  object_factory_t *factory;
} deserialize_state_t;

// Initialize deserialization state.
static value_t deserialize_state_init(deserialize_state_t *state, runtime_t *runtime,
    object_factory_t *factory, safe_value_t s_blob, byte_stream_t *in) {
  state->s_blob = s_blob;
  state->in = in;
  state->object_offset = 0;
  state->runtime = runtime;
  state->factory = factory;
  TRY_SET(state->ref_map, new_heap_id_hash_map(runtime, 16));
  return success();
}

// Reads the next value from the stream.
static value_t value_deserialize(deserialize_state_t *state);

static value_t deserialize_garbage_collect(deserialize_state_t *state) {
  TRY(runtime_garbage_collect(state->runtime));
  state->in->blob = get_blob_data(deref(state->s_blob));
  return success();
}

#define E_S_RETRY()

#define E_TRY_DEF_TRY_AGAIN(POOL, NAME, EXPR)                                  \
  safe_value_t NAME;                                                           \
  do {                                                                         \
    value_t raw_##NAME = (EXPR);                                               \
    if (in_condition_cause(ccHeapExhausted, raw_##NAME)) {                     \
      E_TRY(deserialize_garbage_collect(state));                               \
      raw_##NAME = (EXPR);                                                     \
    }                                                                          \
    E_TRY(raw_##NAME);                                                         \
    NAME = protect(POOL, raw_##NAME);                                          \
  } while (false)


#define E_TRY_DEF_TRY_AGAIN(POOL, NAME, EXPR)                                  \
  safe_value_t NAME;                                                           \
  do {                                                                         \
    value_t raw_##NAME = (EXPR);                                               \
    if (in_condition_cause(ccHeapExhausted, raw_##NAME)) {                     \
      E_TRY(deserialize_garbage_collect(state));                               \
      raw_##NAME = (EXPR);                                                     \
    }                                                                          \
    E_TRY(raw_##NAME);                                                         \
    NAME = protect(POOL, raw_##NAME);                                          \
  } while (false)

static value_t array_deserialize(size_t length, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 1, pool);
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF_TRY_AGAIN(pool, s_result, new_heap_array(state->runtime, length));
    for (size_t i = 0; i < length; i++) {
      E_TRY_DEF(value, value_deserialize(state));
      set_array_at(deref(s_result), i, value);
    }
    E_TRY(ensure_frozen(state->runtime, deref(s_result)));
    E_RETURN(deref(s_result));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

static value_t map_entry_deserialize(safe_value_t s_map, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 2, pool);
  E_BEGIN_TRY_FINALLY();
    E_S_TRY_DEF(s_key, protect(pool, value_deserialize(state)));
    E_S_TRY_DEF(s_value, protect(pool, value_deserialize(state)));
    E_TRY(safe_set_id_hash_map_at(state->runtime, s_map, s_key, s_value));
    E_RETURN(success());
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

static value_t map_deserialize(size_t entry_count, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 1, pool);
  E_BEGIN_TRY_FINALLY();
    E_TRY_DEF_TRY_AGAIN(pool, s_result, new_heap_id_hash_map(state->runtime, 16));
    for (size_t i = 0; i < entry_count; i++)
      E_TRY(map_entry_deserialize(s_result, state));
    E_RETURN(deref(s_result));
  E_FINALLY();
    DISPOSE_SAFE_VALUE_POOL(pool);
  E_END_TRY_FINALLY();
}

static value_t default_string_deserialize(pton_instr_t *instr, deserialize_state_t *state) {
  utf8_t contents = new_string(
      (char*) instr->payload.default_string_data.contents,
      instr->payload.default_string_data.length);
  return new_heap_utf8(state->runtime, contents);
}

// Grabs and returns the next object index.
static size_t acquire_object_index(deserialize_state_t *state) {
  return state->object_offset++;
}

static value_t seed_deserialize(size_t headerc, size_t fieldc, deserialize_state_t *state) {
  size_t offset = acquire_object_index(state);
  // Read the header before creating the instance.
  TRY_DEF(header, value_deserialize(state));
  for (size_t i = 1; i < headerc; i++)
    // Ignore remaining headers.
    TRY(value_deserialize(state));
  object_factory_t *factory = state->factory;
  TRY_DEF(init_value, (factory->new_empty_object)(factory, state->runtime, header));
  TRY(set_id_hash_map_at(state->runtime, state->ref_map, new_integer(offset),
      init_value));
  TRY_DEF(payload, map_deserialize(fieldc, state));
  TRY_DEF(final_value, (factory->set_object_fields)(factory, state->runtime,
      header, init_value, payload));
  if (is_nothing(init_value)) {
    // If the initial value was nothing it means that we should use the value
    // returned from setting the contents. Hence we replace the initial value
    // in the reference map.
    TRY(set_id_hash_map_at(state->runtime, state->ref_map, new_integer(offset),
        final_value));
    return final_value;
  } else {
    return init_value;
  }
}

static value_t reference_deserialize(size_t offset, deserialize_state_t *state) {
  size_t index = state->object_offset - offset - 1;
  value_t result = get_id_hash_map_at(state->ref_map, new_integer(index));
  CHECK_FALSE("missing reference", in_condition_cause(ccNotFound, result));
  return result;
}

static value_t value_deserialize(deserialize_state_t *state) {
  byte_stream_t *in = state->in;
  blob_t blob = in->blob;
  size_t cursor = in->cursor;
  pton_instr_t instr;
  uint8_t *code = (uint8_t*) blob.data;
  if (!pton_decode_next_instruction(code + cursor, blob.size - cursor, &instr))
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
    case PTON_OPCODE_BEGIN_SEED:
      return seed_deserialize(instr.payload.seed_data.headerc,
          instr.payload.seed_data.fieldc, state);
    case PTON_OPCODE_REFERENCE:
      return reference_deserialize(instr.payload.reference_offset, state);
    default:
      return new_invalid_input_condition();
  }
}

value_t plankton_deserialize(runtime_t *runtime, object_factory_t *factory_or_null,
    safe_value_t s_blob) {
  // Make a byte stream out of the blob.
  byte_stream_t in;
  byte_stream_init(&in, get_blob_data(deref(s_blob)));
  // Use a failing environment accessor if the access pointer is null.
  object_factory_t factory;
  if (factory_or_null == NULL) {
    factory = runtime_default_object_factory();
  } else {
    factory = *factory_or_null;
  }
  deserialize_state_t state;
  TRY(deserialize_state_init(&state, runtime, &factory, s_blob, &in));
  return value_deserialize(&state);
}
