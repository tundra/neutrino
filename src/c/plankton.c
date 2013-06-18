#include "plankton.h"

#include <string.h>

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
  buf->data = (uint8_t*) allocator_malloc(&buf->allocator, buf->capacity);
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
  uint8_t *new_data = (uint8_t*) allocator_malloc(&buf->allocator,
      new_capacity);
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

value_t plankton_serialize(value_t data) {
  byte_buffer_t buf;
  byte_buffer_init(&buf, NULL);
  byte_buffer_dispose(&buf);
  return new_integer(0);
}

value_t plankton_deserialize(value_t blob) {
  return new_integer(0);
}
