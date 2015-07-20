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
struct serialize_state_t {
  // The plankton assembler.
  pton_assembler_t *assm;
  // Map from objects we've seen to their index.
  value_t ref_map;
  // The offset of the next object we're going to write.
  size_t object_offset;
  // The runtime to use for heap allocation.
  runtime_t *runtime;
};

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

value_t serialize_array(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofArray, value);
  int64_t length = get_array_length(value);
  pton_assembler_begin_array(state->assm, (uint32_t) length);
  for (int64_t i = 0; i < length; i++) {
    TRY(value_serialize(get_array_at(value, i), state));
  }
  return success();
}

static value_t map_contents_serialize(uint32_t entry_count, id_hash_map_iter_t *iter,
    serialize_state_t *state) {
  uint32_t entries_written = 0;
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

value_t serialize_id_hash_map(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofIdHashMap, value);
  uint32_t entry_count = (uint32_t) get_id_hash_map_size(value);
  pton_assembler_begin_map(state->assm, entry_count);
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, value);
  return map_contents_serialize(entry_count, &iter, state);
}

// Skip this tag when serializing a reified arguments set?
static bool skip_reified_tag(value_t tag) {
  return in_family(ofKey, tag);
}

value_t serialize_reified_arguments(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofReifiedArguments, value);
  value_t values = get_reified_arguments_values(value);
  value_t tags = get_reified_arguments_tags(value);
  uint32_t raw_argc = (uint32_t) get_call_tags_entry_count(tags);
  uint32_t argc = 0;
  // First count how may tags are not going to be skipped.
  for (uint32_t i = 0; i < raw_argc; i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    if (!skip_reified_tag(tag))
      argc++;
  }
  // Then serialize those tags.
  pton_assembler_begin_map(state->assm, argc);
  for (uint32_t i = 0; i < raw_argc; i++) {
    value_t tag = get_call_tags_tag_at(tags, i);
    if (skip_reified_tag(tag))
      continue;
    int64_t offset = get_call_tags_offset_at(tags, i);
    value_t value = get_array_at(values, offset);
    TRY(value_serialize(tag, state));
    TRY(value_serialize(value, state));
  }
  return success();
}

value_t serialize_utf8(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofUtf8, value);
  utf8_t contents = get_utf8_contents(value);
  pton_assembler_emit_default_string(state->assm, contents.chars,
      (uint32_t) string_size(contents));
  return success();
}

value_t serialize_operation(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofOperation, value);
  pton_assembler_begin_seed(state->assm, 1, 2);
  pton_assembler_emit_default_string(state->assm, kOperationHeader,
      (uint32_t) strlen(kOperationHeader));
  pton_assembler_emit_default_string(state->assm, "type", 4);
  pton_assembler_emit_int64(state->assm, get_operation_type(value));
  pton_assembler_emit_default_string(state->assm, "value", 5);
  value_serialize(get_operation_value(value), state);
  return success();
}

static void register_serialized_object(value_t value, serialize_state_t *state) {
  size_t offset = state->object_offset;
  state->object_offset++;
  set_id_hash_map_at(state->runtime, state->ref_map, value, new_integer(offset));
}

value_t serialize_instance(value_t value, serialize_state_t *state) {
  CHECK_FAMILY(ofInstance, value);
  value_t ref = get_id_hash_map_at(state->ref_map, value);
  if (in_condition_cause(ccNotFound, ref)) {
    value_t fields = get_instance_fields(value);
    uint32_t fieldc = (uint32_t) get_id_hash_map_size(fields);
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
    int64_t delta = get_integer_value(ref);
    int64_t offset = state->object_offset - delta - 1;
    pton_assembler_emit_reference(state->assm, offset);
    return success();
  }
}

static value_t heap_object_serialize(value_t self, serialize_state_t *state) {
  CHECK_DOMAIN(vdHeapObject, self);
  family_behavior_t *behavior = get_heap_object_family_behavior(self);
  if (behavior->serialize == NULL) {
    return new_family_not_serializable_condition(behavior->family);
  } else {
    return (behavior->serialize)(self, state);
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
      return heap_object_serialize(data, state);
    case vdCustomTagged:
      return custom_tagged_serialize(data, state);
    default:
      UNREACHABLE("value serialize");
      return new_unsupported_behavior_condition(domain, __ofUnknown__,
          ubPlanktonSerialize);
  }
}

value_t plankton_serialize_to_blob(runtime_t *runtime, value_t data) {
  blob_t blob;
  pton_assembler_t *assm = NULL;
  TRY_FINALLY {
    E_TRY(plankton_serialize_to_data(runtime, data, &blob, &assm));
    E_TRY_DEF(result, new_heap_blob_with_data(runtime, blob));
    E_RETURN(result);
  } FINALLY {
    if (assm != NULL)
      pton_dispose_assembler(assm);
  } YRT
}

value_t plankton_serialize_to_data(runtime_t *runtime, value_t data,
    blob_t *blob_out, pton_assembler_t **assm_out) {
  pton_assembler_t *assm = pton_new_assembler();
  blob_t code;
  TRY_FINALLY {
    serialize_state_t state;
    E_TRY(serialize_state_init(&state, runtime, assm));
    E_TRY(value_serialize(data, &state));
    code = pton_assembler_peek_code(assm);
    *blob_out = blob_new(code.start, code.size);
    *assm_out = assm;
    assm = NULL;
    E_RETURN(success());
  } FINALLY {
    if (assm != NULL)
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
  blob_t *data;
  size_t cursor;
  // Map from object offsets we've seen to their values.
  safe_value_t s_ref_map;
  // The offset of the next object we're going to write.
  uint64_t object_offset;
  // The runtime to use for heap allocation.
  runtime_t *runtime;
  // The factory used to construct object instances.
  object_factory_t *factory;
} deserialize_state_t;

// Initialize deserialization state.
static value_t deserialize_state_init(deserialize_state_t *state, runtime_t *runtime,
    object_factory_t *factory, blob_t *data) {
  state->data = data;
  state->cursor = 0;
  state->object_offset = 0;
  state->runtime = runtime;
  state->factory = factory;
  state->s_ref_map = runtime_protect_value(runtime, new_heap_id_hash_map(runtime, 16));
  return success();
}

static void deserialize_state_dispose(deserialize_state_t *state) {
  safe_value_destroy(state->runtime, state->s_ref_map);
}

// Reads the next value from the stream.
static value_t value_deserialize(deserialize_state_t *state);

// Try doing the given expression, retry if the heap becomes exhausted. Note:
// only use this if the function you're calling doesn't handle heap exhaustion.
// For instance, don't retry calls to deserialize functions because they only
// fail for non-exhaustion reasons.
#define E_RETRY(RUNTIME, EXPR) __GENERIC_RETRY__(P_FLAVOR, RUNTIME, EXPR, P_RETURN)

// Try and possibly retry evaluating the given expression and if successful
// protect the result using the given pool and store it in a new variable with
// the given name. Same rules apply as for E_RETRY.
#define E_RETRY_DEF_PROTECT(POOL, RUNTIME, NAME, EXPR)                         \
  safe_value_t NAME;                                                           \
  do {                                                                         \
    __GENERIC_RETRY_DEF__(P_FLAVOR, RUNTIME, __erdp_value__, EXPR, P_RETURN);  \
    NAME = protect(POOL, __erdp_value__);                                      \
  } while (false)

static value_t array_deserialize(size_t length, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 1, pool);
  TRY_FINALLY {
    E_RETRY_DEF_PROTECT(pool, state->runtime, s_result, new_heap_array(state->runtime, length));
    for (size_t i = 0; i < length; i++) {
      E_TRY_DEF(value, value_deserialize(state));
      set_array_at(deref(s_result), i, value);
    }
    E_TRY(ensure_frozen(state->runtime, deref(s_result)));
    E_RETURN(deref(s_result));
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

static value_t map_entry_deserialize(safe_value_t s_map, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 2, pool);
  TRY_FINALLY {
    E_S_TRY_DEF(s_key, protect(pool, value_deserialize(state)));
    E_S_TRY_DEF(s_value, protect(pool, value_deserialize(state)));
    E_TRY(safe_set_id_hash_map_at(state->runtime, s_map, s_key, s_value));
    E_RETURN(success());
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

static value_t map_deserialize(size_t entry_count, deserialize_state_t *state) {
  CREATE_SAFE_VALUE_POOL(state->runtime, 1, pool);
  TRY_FINALLY {
    E_RETRY_DEF_PROTECT(pool, state->runtime, s_result, new_heap_id_hash_map(state->runtime, 16));
    for (size_t i = 0; i < entry_count; i++)
      E_TRY(map_entry_deserialize(s_result, state));
    E_RETURN(deref(s_result));
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

static value_t default_string_deserialize(pton_instr_t *instr, deserialize_state_t *state) {
  utf8_t contents = new_string(
      (char*) instr->payload.default_string_data.contents,
      instr->payload.default_string_data.length);
  return new_heap_utf8(state->runtime, contents);
}

// Grabs and returns the next object index.
static uint64_t acquire_object_index(deserialize_state_t *state) {
  return state->object_offset++;
}

static value_t seed_deserialize(size_t headerc, size_t fieldc, deserialize_state_t *state) {
  uint64_t offset = acquire_object_index(state);
  CREATE_SAFE_VALUE_POOL(state->runtime, 5, pool);
  TRY_FINALLY {
    // Read the header before creating the instance.
    E_S_TRY_DEF(s_header, protect(pool, value_deserialize(state)));
    for (size_t i = 1; i < headerc; i++)
      // Ignore remaining headers.
      E_TRY(value_deserialize(state));
    object_factory_t *factory = state->factory;
    E_RETRY_DEF_PROTECT(pool, state->runtime, s_init_value,
        (factory->new_empty_object)(factory, state->runtime, deref(s_header)));
    E_RETRY(state->runtime, safe_set_id_hash_map_at(state->runtime, state->s_ref_map,
        protect_immediate(new_integer(offset)), s_init_value));
    E_S_TRY_DEF(s_payload, protect(pool, map_deserialize(fieldc, state)));
    E_RETRY_DEF_PROTECT(pool, state->runtime, s_final_value,
        (factory->set_object_fields)(factory, state->runtime, deref(s_header),
            deref(s_init_value), deref(s_payload)));
    if (safe_value_is_nothing(s_init_value)) {
      // If the initial value was nothing it means that we should use the value
      // returned from setting the contents. Hence we replace the initial value
      // in the reference map.
      E_RETRY(state->runtime, safe_set_id_hash_map_at(state->runtime,
          state->s_ref_map, protect_immediate(new_integer(offset)), s_final_value));
      E_RETURN(deref(s_final_value));
    } else {
      E_RETURN(deref(s_init_value));
    }
  } FINALLY {
    DISPOSE_SAFE_VALUE_POOL(pool);
  } YRT
}

static value_t reference_deserialize(uint64_t offset, deserialize_state_t *state) {
  int64_t index = state->object_offset - offset - 1;
  value_t result = get_id_hash_map_at(deref(state->s_ref_map), new_integer(index));
  return in_condition_cause(ccNotFound, result)
      ? new_condition_with_details(ccUnknownReference, (int32_t) index)
      : result;
}

static value_t value_deserialize(deserialize_state_t *state) {
  blob_t blob = *state->data;
  size_t cursor = state->cursor;
  pton_instr_t instr;
  uint8_t *code = (uint8_t*) blob.start;
  if (!pton_decode_next_instruction(code + cursor, blob.size - cursor, &instr))
    return new_invalid_input_condition();
  state->cursor += instr.size;
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

// Deserialize the given raw blob. Subtle note: the blob is passed by ref not
// value and the implementation assumes that its state may change during
// parsing. The data it contains should not change but it is okay for it to
// point to different arrays as long as those arrays always contain the same
// data (think: for the array to be moved by gc).
static value_t plankton_deserialize_data_ptr(runtime_t *runtime,
    object_factory_t *factory_or_null, blob_t *in) {
  // Use a failing environment accessor if the access pointer is null.
  object_factory_t factory;
  if (factory_or_null == NULL) {
    factory = runtime_default_object_factory();
  } else {
    factory = *factory_or_null;
  }
  deserialize_state_t state;
  TRY(deserialize_state_init(&state, runtime, &factory, in));
  TRY_FINALLY {
    E_RETURN(value_deserialize(&state));
  } FINALLY {
    deserialize_state_dispose(&state);
  } YRT
}

// If a gc happens during deserialization we fix up the blob such that it can
// always be accessed directly and you'll get the heap blob's data.
static opaque_t on_gc_during_deserialize(opaque_t opaque_s_blob,
    opaque_t opaque_data, opaque_t opaque_runtime) {
  safe_value_t *s_blob = (safe_value_t*) o2p(opaque_s_blob);
  blob_t *data = (blob_t*) o2p(opaque_data);
  *data = get_blob_data(deref(*s_blob));
  return o0();
}

value_t plankton_deserialize_blob(runtime_t *runtime,
    object_factory_t *factory_or_null, safe_value_t s_blob) {
  // Make a byte stream out of the blob.
  blob_t data = get_blob_data(deref(s_blob));
  runtime_observer_t observer = runtime_observer_empty();
  TRY_FINALLY {
    // Install a runtime observer that fixes up the data blob when a gc
    // happens. Admittedly this is unsavory but for the same path to be able
    // to handle raw streams in the C heap and blobs in the neutrino heap it's
    // really convenient to make the neutrino blob look like it's just a
    // normal stream.
    observer.on_gc_done = unary_callback_new_2(on_gc_during_deserialize,
        p2o(&s_blob), p2o(&data));
    runtime_push_observer(runtime, &observer);
    E_RETURN(plankton_deserialize_data_ptr(runtime, factory_or_null, &data));
  } FINALLY {
    runtime_pop_observer(runtime, &observer);
    callback_destroy(observer.on_gc_done);
  } YRT
}

value_t plankton_deserialize_data(runtime_t *runtime,
    object_factory_t *factory_or_null, blob_t data) {
  return plankton_deserialize_data_ptr(runtime, factory_or_null, &data);
}
