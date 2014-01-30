// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "crash.h"
#include "log.h"

#ifdef IS_GCC
#include "crash-posix-opt.c"
#else
#include "crash-fallback-opt.c"
#endif

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

abort_callback_t *get_global_abort_callback() {
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
void call_abort_callback(abort_callback_t *callback,
    abort_message_t *message) {
  (callback->function)(callback->data, message);
}

abort_callback_t *set_abort_callback(abort_callback_t *value) {
  abort_callback_t *result = get_global_abort_callback();
  global_abort_callback = value;
  return result;
}

void abort_message_init(abort_message_t *message, const char *file, int line,
    int condition_cause, const char *text) {
  message->file = file;
  message->line = line;
  message->condition_cause = condition_cause;
  message->text = text;
}


// --- C h e c k   f a i l i n g ---

// Prints the message for a check failure on standard error.
static void vcheck_fail(const char *file, int line, int condition_cause,
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
  abort_message_init(&message, file, line, condition_cause, str.chars);
  call_abort_callback(get_global_abort_callback(), &message);
  string_buffer_dispose(&buf);
}

void check_fail(const char *file, int line, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  vcheck_fail(file, line, ccNothing, fmt, argp);
}

void cond_check_fail(const char *file, int line, int condition_cause, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  vcheck_fail(file, line, condition_cause, fmt, argp);
}
