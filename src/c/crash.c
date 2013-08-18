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
#include <execinfo.h>


// --- S i g n a l   h a n d l i n g ---

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


// --- A b o r t ---

static abort_callback_t kDefaultAbortCallback;
static abort_callback_t *global_abort_callback = NULL;

// The default abort handler which prints the message to stderr and aborts
// execution.
static void default_abort(void *data, const char *file, int line, int signal_cause,
    const char *message) {
  log_message(llError, file, line, "%s", message);
  fflush(stderr);
  abort();
}

// Returns the current global abort callback.
static abort_callback_t *get_global_abort_callback() {
  if (global_abort_callback == NULL) {
    init_abort_callback(&kDefaultAbortCallback, default_abort, NULL);
    global_abort_callback = &kDefaultAbortCallback;
  }
  return global_abort_callback;
}

void init_abort_callback(abort_callback_t *callback, abort_function_t *function,
    void *data) {
  callback->function = function;
  callback->data = data;
}

// Invokes the given callback with the given arguments.
static void call_abort_callback(abort_callback_t *callback, const char *file,
    int line, int signal_cause, const char *message) {
  (callback->function)(callback->data, file, line, signal_cause, message);
}

abort_callback_t *set_abort_callback(abort_callback_t *value) {
  abort_callback_t *result = get_global_abort_callback();
  global_abort_callback = value;
  return result;
}


// --- C h e c k   f a i l i n g ---

// Prints the message for a check failure on standard error.
static void vcheck_fail(const char *file, int line, int signal_cause,
    const char *fmt, va_list argp) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Print the formatted error message.
  call_abort_callback(get_global_abort_callback(), file, line, signal_cause, str.chars);
  string_buffer_dispose(&buf);
}

void check_fail(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  vcheck_fail(file, line, scNothing, fmt, argp);
}

void sig_check_fail(const char *file, int line, int signal_cause, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  vcheck_fail(file, line, signal_cause, fmt, argp);
}
