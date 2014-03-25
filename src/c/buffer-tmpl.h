//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// This is a make-your-own template; define the appropriate macros and include
// this file and it will declare a buffer type that works with that type.

#ifndef BUFFER_TYPE
#error No buffer type defined
#endif

#ifndef MAKE_BUFFER_NAME
#error No buffer name maker defined
#endif

// Buffer for building a block of BUFFER_TYPE incrementally.
struct MAKE_BUFFER_NAME(t) {
  // Size of data currently in the buffer counted in elements, not bytes.
  size_t length;
  // The data block.
  memory_block_t memory;
};

// Initialize a buffer.
void MAKE_BUFFER_NAME(init)(MAKE_BUFFER_NAME(t) *buf);

// Disposes the given buffer.
void MAKE_BUFFER_NAME(dispose)(MAKE_BUFFER_NAME(t) *buf);

// Add an element to the given buffer.
void MAKE_BUFFER_NAME(append)(MAKE_BUFFER_NAME(t) *buf,
    BUFFER_TYPE value);

// Write the current contents to the given blob. The data in the blob will
// still be backed by this buffer so disposing this will make the blob invalid.
void MAKE_BUFFER_NAME(flush)(MAKE_BUFFER_NAME(t) *buf,
    blob_t *blob_out);

// A pointer to a location within a buffer that can be written to directly.
struct MAKE_BUFFER_NAME(cursor_t) {
  struct MAKE_BUFFER_NAME(t) *buf;
  size_t offset;
};

// Writes 0 to the next location and stores a cursor for writing to that
// location in the given out parameter. Obviously the cursor becomes invalid
// when the buffer is disposed.
void MAKE_BUFFER_NAME(append_cursor)(MAKE_BUFFER_NAME(t) *buf,
    MAKE_BUFFER_NAME(cursor_t) *cursor_out);

// Sets the value at the given location in a buffer.
void MAKE_BUFFER_NAME(cursor_set)(MAKE_BUFFER_NAME(cursor_t) *cursor,
    BUFFER_TYPE value);
