//- Copyright 2013 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "crash.h"
#include "log.h"
#include "ook-inl.h"

#ifdef IS_GCC
#include "crash-posix-opt.c"
#else
#include "crash-fallback-opt.c"
#endif

// --- A b o r t ---

IMPLEMENTATION(default_abort_o, abort_o);

static abort_o kDefaultAbort;
static abort_o *global_abort = NULL;

// The default abort handler which prints the message to stderr and aborts
// execution.
static void default_abort(abort_o *self, abort_message_t *message) {
  log_message(llError, message->file, message->line, "%s", message->text);
  fflush(stderr);
  abort();
}

VTABLE(default_abort_o, abort_o) { default_abort };

abort_o *get_global_abort() {
  if (global_abort == NULL) {
    VTABLE_INIT(default_abort_o, &kDefaultAbort);
    global_abort = &kDefaultAbort;
  }
  return global_abort;
}

// Invokes the given callback with the given arguments.
void abort_call(abort_o *self, abort_message_t *message) {
  METHOD(self, abort)(self, message);
}

abort_o *set_global_abort(abort_o *value) {
  abort_o *result = get_global_abort();
  global_abort = value;
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
  abort_call(get_global_abort(), &message);
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
