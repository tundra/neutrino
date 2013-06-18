#include "alloc.h"
#include "plankton.h"
#include "value-inl.h"

#include <string.h>


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

value_t plankton_serialize(runtime_t *runtime, value_t data) {
  // Write the data to a C heap blob.
  byte_buffer_t buf;
  byte_buffer_init(&buf, NULL);
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
