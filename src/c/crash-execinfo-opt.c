//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

// Implementation of crash dumps that uses execinfo.

#include <execinfo.h>

static const size_t kMaxStackSize = 128;

void print_stack_trace(FILE *out, int signum) {
  fprintf(out, "# Received condition %i\n", signum);
  void *raw_frames[kMaxStackSize];
  size_t size = backtrace(raw_frames, kMaxStackSize);
  char **frames = backtrace_symbols(raw_frames, size);
  for (size_t i = 0; i < size; i++) {
    fprintf(out, "# - %s\n", frames[i]);
  }
  free(frames);
  fflush(out);
}

void propagate_condition(int signum) {
  kill(getpid(), signum);
}
