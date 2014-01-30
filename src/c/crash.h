// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Various utilities related to killing the runtime and handling when the
// runtime gets killed.


#ifndef _CRASH
#define _CRASH

#include "globals.h"

// Data used to construct the message displayed when the runtime aborts.
typedef struct {
  const char *file;
  int line;
  int condition_cause;
  const char *text;
} abort_message_t;

// Initializes the fields of an abort message.
void abort_message_init(abort_message_t *message, const char *file, int line,
    int condition_cause, const char *text);

// Type of abort functions.
typedef void (abort_function_t)(void *data, abort_message_t *message);

// A callback used to abort execution.
typedef struct {
  // The function to call on abort.
  abort_function_t *function;
  // Additional data to pass to the function.
  void *data;
} abort_callback_t;

// Initializes an abort callback.
void init_abort_callback(abort_callback_t *callback, abort_function_t *function,
    void *data);

// Invokes the given callback with the given arguments.
void call_abort_callback(abort_callback_t *callback,
    abort_message_t *message);

// Sets the abort callback to use across this process. This should only be used
// for testing. The specified callback is allowed to kill the vm, the state called
// "hard check failures", or keep it running known as "soft check failures".
// Returns the previous value such that it can be restored if necessary.
abort_callback_t *set_abort_callback(abort_callback_t *value);

// Returns the current global abort callback.
abort_callback_t *get_global_abort_callback();

// Sets up handling of crashes.
void install_crash_handler();

// Signals an error and kills the process.
void check_fail(const char *file, int line, const char *fmt, ...);

// In hard check failure mode signals an error and kills the process, in soft
// mode records the check and returns.
void cond_check_fail(const char *file, int line, int condition_cause,
    const char *fmt, ...);

#endif // _CRASH
