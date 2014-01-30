// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "crash.h"
#include "globals.h"
#include "log.h"
#include "utils.h"
#include "value.h"

#define __USE_POSIX
#include <signal.h>
#include <unistd.h>


// --- S i g n a l   h a n d l i n g ---

// Print a stack trace if the platform supports it.
void print_stack_trace(FILE *out, int signum);

// After handling the condition here, propagate it so that it doesn't get swallowed.
void propagate_condition(int signum);

// Processes crashes.
static void crash_handler(int signum) {
  print_stack_trace(stdout, signum);
  propagate_condition(signum);
}

void install_crash_handler() {
  signal(SIGSEGV, crash_handler);
  signal(SIGABRT, crash_handler);
}

// For now always use execinfo. Later on some more logic can be built in to deal
// with the case where execinfo isn't available.
#include "crash-execinfo-opt.c"
