#include "alloc.h"
#include "plankton.h"
#include "value-inl.h"

#include <string.h>

// The different plankton type tags.
typedef enum {
  pInt32 = 0,
  pString = 1,
  pArray = 2,
  pMap = 3,
  pNull = 4,
  pTrue = 5,
  pFalse = 6,
  pObject = 7,
  pReference = 8,
  pEnvironment = 9,
} plankton_tag_t;


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

// Serialize any (non-signal) value on the given buffer.
static value_t value_serialize(value_t value, byte_buffer_t *buf);

// Encodes an unsigned 32-bit integer.
static value_t encode_uint32(uint32_t value, byte_buffer_t *buf) {
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
  return encode_uint32(zig_zag, buf);
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

static value_t array_serialize(value_t value, byte_buffer_t *buf) {
  CHECK_FAMILY(ofArray, value);
  byte_buffer_append(buf, pArray);
  size_t length = get_array_length(value);
  encode_uint32(length, buf);
  for (size_t i = 0; i < length; i++) {
    TRY(value_serialize(get_array_at(value, i), buf));
  }
  return success();
}

static value_t map_serialize(value_t value, byte_buffer_t *buf) {
  CHECK_FAMILY(ofIdHashMap, value);
  byte_buffer_append(buf, pMap);
  size_t entry_count = get_id_hash_map_size(value);
  encode_uint32(entry_count, buf);
  id_hash_map_iter_t iter;
  id_hash_map_iter_init(&iter, value);
  size_t entries_written = 0;
  while (id_hash_map_iter_advance(&iter)) {
    value_t key;
    value_t value;
    id_hash_map_iter_get_current(&iter, &key, &value);
    TRY(value_serialize(key, buf));
    TRY(value_serialize(value, buf));
    entries_written++;
  }
  CHECK_EQ("serialized map length", entry_count, entries_written);
  return success();
}


static value_t object_serialize(value_t value, byte_buffer_t *buf) {
  CHECK_DOMAIN(vdObject, value);
  switch (get_object_family(value)) {
    case ofNull:
      return singleton_serialize(pNull, buf);
    case ofBool:
      return singleton_serialize(get_bool_value(value) ? pTrue : pFalse, buf);
    case ofArray:
      return array_serialize(value, buf);
    case ofIdHashMap:
      return map_serialize(value, buf);
    default:
      return new_signal(scInvalidInput);
  }
}

static value_t value_serialize(value_t data, byte_buffer_t *buf) {
  switch (get_value_domain(data)) {
    case vdInteger:
      return integer_serialize(data, buf);
    case vdObject:
      return object_serialize(data, buf);
    default:
      UNREACHABLE("value serialize");
      return new_signal(scUnsupportedBehavior);
  }
}

value_t plankton_serialize(runtime_t *runtime, value_t data) {
  // Write the data to a C heap blob.
  byte_buffer_t buf;
  byte_buffer_init(&buf, NULL);
  TRY(value_serialize(data, &buf));
  blob_t buffer_data;
  byte_buffer_flush(&buf, &buffer_data);
  // Allocate the blob object to hold the result.
  TRY_DEF(blob, new_heap_blob(runtime, blob_length(&buffer_data)));
  blob_t blob_data;
  get_blob_data(blob, &blob_data);
  // Copy the result into the heap blob.
  blob_copy_to(&buffer_data, &blob_data);
  byte_buffer_dispose(&buf);
  return blob;
}


// --- D e s e r i a l i z e ---

// Reads the next value from the stream.
static value_t value_deserialize(runtime_t *runtime, byte_stream_t *in);

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

static value_t array_deserialize(runtime_t *runtime, byte_stream_t *in) {
  size_t length = uint32_deserialize(in);
  TRY_DEF(result, new_heap_array(runtime, length));
  for (size_t i = 0; i < length; i++) {
    TRY_DEF(value, value_deserialize(runtime, in));
    set_array_at(result, i, value);
  }
  return result;
}

static value_t map_deserialize(runtime_t *runtime, byte_stream_t *in) {
  size_t entry_count = uint32_deserialize(in);
  TRY_DEF(result, new_heap_id_hash_map(runtime, 16));
  for (size_t i = 0; i < entry_count; i++) {
    TRY_DEF(key, value_deserialize(runtime, in));
    TRY_DEF(value, value_deserialize(runtime, in));
    set_id_hash_map_at(runtime, result, key, value);
  }
  return result;
}

static value_t value_deserialize(runtime_t *runtime, byte_stream_t *in) {
  switch (byte_stream_read(in)) {
    case pInt32:
      return int32_deserialize(in);
    case pNull:
      return runtime_null(runtime);
    case pTrue:
      return runtime_bool(runtime, true);
    case pFalse:
      return runtime_bool(runtime, false);
    case pArray:
      return array_deserialize(runtime, in);
    case pMap:
      return map_deserialize(runtime, in);
    default:
      return new_signal(scInvalidInput);
  }
}

value_t plankton_deserialize(runtime_t *runtime, value_t blob) {
  blob_t data;
  get_blob_data(blob, &data);
  byte_stream_t in;
  byte_stream_init(&in, &data);
  return value_deserialize(runtime, &in);
}
