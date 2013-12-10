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

// After handling the signal here, propagate it so that it doesn't get swallowed.
void propagate_signal(int signum);

// Processes crashes.
static void crash_handler(int signum) {
  print_stack_trace(stdout, signum);
  propagate_signal(signum);
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
static void default_abort(void *data, abort_message_t *message) {
  log_message(llError, message->file, message->line, "%s", message->text);
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
static void call_abort_callback(abort_callback_t *callback,
    abort_message_t *message) {
  (callback->function)(callback->data, message);
}

abort_callback_t *set_abort_callback(abort_callback_t *value) {
  abort_callback_t *result = get_global_abort_callback();
  global_abort_callback = value;
  return result;
}

void abort_message_init(abort_message_t *message, const char *file, int line,
    int signal_cause, const char *text) {
  message->file = file;
  message->line = line;
  message->signal_cause = signal_cause;
  message->text = text;
}


// --- C h e c k   f a i l i n g ---

// Prints the message for a check failure on standard error.
static void vcheck_fail(const char *file, int line, int signal_cause,
    const char *fmt, va_list argp) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Print the formatted error message.
  abort_message_t message;
  abort_message_init(&message, file, line, signal_cause, str.chars);
  call_abort_callback(get_global_abort_callback(), &message);
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


#ifdef OS_POSIX
#include "crash-execinfo-opt.c"
#else
#include "crash-fallback-opt.c"
#endif