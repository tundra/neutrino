//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#ifndef _IO
#define _IO

#include "value.h"

/// ## Pipe

static const size_t kPipeSize = HEAP_OBJECT_SIZE(3);
static const size_t kPipeNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kPipeInOffset = HEAP_OBJECT_FIELD_OFFSET(1);
static const size_t kPipeOutOffset = HEAP_OBJECT_FIELD_OFFSET(2);

// The underlying native pipe.
ACCESSORS_DECL(pipe, native);

// This pipe's input stream.
ACCESSORS_DECL(pipe, in);

// This pipe's output stream.
ACCESSORS_DECL(pipe, out);


/// ## Out stream

static const size_t kOutStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kOutStreamNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kOutStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native out stream.
ACCESSORS_DECL(out_stream, native);

// Optional value kept alive by this out stream.
ACCESSORS_DECL(out_stream, lifeline);


/// ## In stream

static const size_t kInStreamSize = HEAP_OBJECT_SIZE(2);
static const size_t kInStreamNativeOffset = HEAP_OBJECT_FIELD_OFFSET(0);
static const size_t kInStreamLifelineOffset = HEAP_OBJECT_FIELD_OFFSET(1);

// The underlying native in stream.
ACCESSORS_DECL(in_stream, native);

// Optional value kept alive by this in stream.
ACCESSORS_DECL(in_stream, lifeline);

#endif // _IO
