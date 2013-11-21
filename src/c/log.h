// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Logging framework.


#ifndef _LOG
#define _LOG

#include "globals.h"
#include "utils.h"

#include <stdarg.h>

// Log statements below this level are stripped from the code.
#define LOG_LEVEL llInfo

// Which stream should the log entry be reported on?
typedef enum {
  lsStdout,
  lsStderr
} log_stream_t;

// Call the given callback for each log level, log level character, log level
// value, and destination log stream. The more serious the error the higher the
// value will be.
#define ENUM_LOG_LEVELS(F)                                                     \
  F(Info,    I, 1, lsStdout)                                                   \
  F(Warning, W, 2, lsStderr)                                                   \
  F(Error,   E, 3, lsStderr)

// Expands to a true value if the current log level is the specified value or
// less severe, implying that log messages to the given level should be
// reported.
#define LOG_LEVEL_AT_LEAST(llLevel) (LOG_LEVEL <= llLevel)

// Log levels, used to select which logging statements to emit.
typedef enum {
  __llFirst__ = -1
#define __DECLARE_LOG_LEVEL__(Name, C, V, S) , ll##Name = V
  ENUM_LOG_LEVELS(__DECLARE_LOG_LEVEL__)
#undef __DECLARE_LOG_LEVEL__
} log_level_t;

// Logs a message at the given log level.
void log_message(log_level_t level, const char *file, int line, const char *fmt,
    ...);

// Va_list version of log message.
void vlog_message(log_level_t level, const char *file, int line, const char *fmt,
    va_list argp);

// The data that makes up an entry in the log.
typedef struct {
  log_stream_t destination;
  const char *file;
  int line;
  log_level_t level;
  string_t *message;
  string_t *timestamp;
} log_entry_t;

// Sets all the required fields in a log entry struct.
void log_entry_init(log_entry_t *entry, log_stream_t destination, const char *file,
    int line, log_level_t level, string_t *message, string_t *timestamp);

// Type of log functions.
typedef void (log_function_t)(void *data, log_entry_t *entry);

// A callback used to issue log messages.
typedef struct {
  // The function to call on logging.
  log_function_t *function;
  // Additional data to pass to the function.
  void *data;
} log_callback_t;

// Initializes a log callback.
void init_log_callback(log_callback_t *callback, log_function_t *function,
    void *data);

// Sets the log callback to use across this process. This should only be used
// for testing. Returns the previous value such that it can be restored if
// necessary.
log_callback_t *set_log_callback(log_callback_t *value);

// Emits a warning if the static log level is at least warning, otherwise does
// nothing (including doesn't evaluate arguments).
#define WARN(FMT, ...) do {                                                    \
  if (LOG_LEVEL_AT_LEAST(llWarning))                                           \
    log_message(llWarning, __FILE__, __LINE__, FMT, __VA_ARGS__);              \
} while (false)

// Emits an error if the static log level is at least error, otherwise does
// nothing (including doesn't evaluate arguments).
#define ERROR(FMT, ...) do {                                                   \
  if (LOG_LEVEL_AT_LEAST(llError))                                             \
    log_message(llError, __FILE__, __LINE__, FMT, __VA_ARGS__);                \
} while (false)

// Emits an info if the static log level is at least info, otherwise does
// nothing (including doesn't evaluate arguments).
#define INFO(FMT, ...) do {                                                    \
  if (LOG_LEVEL_AT_LEAST(llInfo))                                              \
    log_message(llInfo, __FILE__, __LINE__, FMT, __VA_ARGS__);                 \
} while (false)

// Works the same as INFO but any occurrences of this macro will cause the
// linter to choke if you try to submit. If you use this when debugging/tracing
// by hest the linter will help you remember to get rid of all the debug print
// statements before submitting.
#define HEST(FMT, ...) INFO(FMT, __VA_ARGS__)

#endif // _LOG
