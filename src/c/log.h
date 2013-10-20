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

// Call the given callback for each log level and log level character.
#define ENUM_LOG_LEVELS(F)                                                     \
  F(Error,   E)                                                                \
  F(Warning, W)                                                                \
  F(Info,    I)

// Log levels, used to select which logging statements to emit.
typedef enum {
  __llFirst__ = -1
#define __DECLARE_LOG_LEVEL__(Name, C) , ll##Name
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
  const char *file;
  int line;
  log_level_t level;
  string_t *message;
  string_t *timestamp;
} log_entry_t;

// Sets all the required fields in a log entry struct.
void log_entry_init(log_entry_t *entry, const char *file, int line,
    log_level_t level, string_t *message, string_t *timestamp);

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
  if (LOG_LEVEL >= llWarning)                                                  \
    log_message(llWarning, __FILE__, __LINE__, FMT, __VA_ARGS__);              \
} while (false)

// Emits an error if the static log level is at least error, otherwise does
// nothing (including doesn't evaluate arguments).
#define ERROR(FMT, ...) do {                                                   \
  if (LOG_LEVEL >= llError)                                                    \
    log_message(llError, __FILE__, __LINE__, FMT, __VA_ARGS__);                \
} while (false)

// Emits an info if the static log level is at least info, otherwise does
// nothing (including doesn't evaluate arguments).
#define INFO(FMT, ...) do {                                                    \
  if (LOG_LEVEL >= llInfo)                                                     \
    log_message(llInfo, __FILE__, __LINE__, FMT, __VA_ARGS__);                 \
} while (false)

#endif // _LOG
