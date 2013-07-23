#include "crash.h"
#include "globals.h"

#define __USE_POSIX
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>

static const size_t kMaxStackSize = 128;

// Dump a stack trace to the given file.
static void print_stack_trace(FILE *out, int signum) {
  fprintf(out, "# Received signal %i\n", signum);
  void *raw_frames[kMaxStackSize];
  size_t size = backtrace(raw_frames, kMaxStackSize);
  char **frames = backtrace_symbols(raw_frames, size);
  for (size_t i = 0; i < size; i++) {
    fprintf(out, "# - %s\n", frames[i]);
  }
  free(frames);
  fflush(out);
}

// Processes crashes.
static void crash_handler(int signum) {
  print_stack_trace(stdout, signum);
  // Propagate the signal when we're done with it.
  kill(getpid(), signum);
}

void install_crash_handler() {
  signal(SIGSEGV, crash_handler);
  signal(SIGABRT, crash_handler);
}
