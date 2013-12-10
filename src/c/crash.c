// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "crash.h"

#ifdef IS_GCC
#include "crash-posix-opt.c"
#else
#include "crash-none-opt.c"
#endif
