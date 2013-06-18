#include "alloc.h"
#include "plankton.h"
#include "value-inl.h"

#include <string.h>


static const byte_t kInt32Tag = 0;
static const byte_t kStringTag = 1;
static const byte_t kArrayTag = 2;
static const byte_t kMapTag = 3;
static const byte_t kNullTag = 4;
static const byte_t kTrueTag = 5;
static const byte_t kFalseTag = 6;
static const byte_t kObjectTag = 7;
static const byte_t kReferenceTag = 8;
static const byte_t kEnvironmentTag = 9;

// --- B y t e   b u f f e r ---

// Buffer for building a block of bytes incrementally.
typedef struct {
  // Size of data currently in the buffer.
  size_t length;
  // Length of the total buffer.
  size_t capacity;
  // The actual data.
  uint8_t *data;
  // The allocator to use to grab memory.
  allocator_t allocator;
} byte_buffer_t;

// Initialize a byte buffer. If an allocator is passed it will be used for
// all allocation, otherwise the default system allocator will be used.
static void byte_buffer_init(byte_buffer_t *buf, allocator_t *alloc_or_null) {
  if (alloc_or_null == NULL) {
    init_system_allocator(&buf->allocator);
  } else {
    buf->allocator = *alloc_or_null;
  }
  buf->length = 0;
  buf->capacity = 128;
  buf->data = allocator_malloc(&buf->allocator, buf->capacity);
}

// Disposes the given byte buffer.
static void byte_buffer_dispose(byte_buffer_t *buf) {
  allocator_free(&buf->allocator, (address_t) buf->data);
}

// Expands the buffer to make room for 'length' characters if necessary.
static void string_buffer_ensure_capacity(byte_buffer_t *buf, size_t length) {
  if (length < buf->capacity)
    return;
  size_t new_capacity = (length * 2);
  uint8_t *new_data = allocator_malloc(&buf->allocator, new_capacity);
  memcpy(new_data, buf->data, buf->length);
  allocator_free(&buf->allocator, (address_t) buf->data);
  buf->data = new_data;
  buf->capacity = new_capacity;
}

// Add a byte to the given buffer.
static void byte_buffer_append(byte_buffer_t *buf, uint8_t value) {
  string_buffer_ensure_capacity(buf, buf->length + 1);
  buf->data[buf->length] = value;
  buf->length++;
}

// Write the current contents to the given blob. The data in the blob will
// still be backed by this buffer so disposing this will make the blob invalid.
static void byte_buffer_flush(byte_buffer_t *buf, blob_t *blob_out) {
  blob_init(blob_out, buf->data, buf->length);
}


// --- S e r i a l i z e ---

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

value_t integer_serialize(value_t value, byte_buffer_t *buf) {
  CHECK_DOMAIN(vdInteger, value);
  byte_buffer_append(buf, kInt32Tag);
  // TODO: deal with full size integers, for now just trap loss of data.
  int64_t int_value = get_integer_value(value);
  int32_t trunc = (int32_t) int_value;
  CHECK_EQ(int_value, (int64_t) trunc);
  return encode_int32(trunc, buf);
}

value_t value_serialize(value_t data, byte_buffer_t *buf) {
  switch (get_value_domain(data)) {
    case vdInteger:
      return integer_serialize(data, buf);
    default:
      UNREACHABLE();
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

value_t plankton_deserialize(runtime_t *runtime, value_t blob) {
  return new_integer(0);
}
