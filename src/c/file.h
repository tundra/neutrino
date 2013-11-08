// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Utilities for working with files.


#ifndef _FILE
#define _FILE

#include "value-inl.h"

#include <stdio.h>

// Reads the full contents of a file as given by a FILE handle into a blob.
value_t read_handle_to_blob(runtime_t *runtime, FILE *handle);

// Reads the full contents of a named file.
value_t read_file_to_blob(runtime_t *runtime, string_t *filename);

#endif // _FILE
