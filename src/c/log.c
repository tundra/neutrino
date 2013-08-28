// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.h"
#include "utils.h"

#include <time.h>

// Returns the initial for the given log level.
static const char *get_log_level_char(log_level_t level) {
  switch (level) {
#define __LEVEL_CASE__(Name, C) case ll##Name: return #C;
    ENUM_LOG_LEVELS(__LEVEL_CASE__)
#undef __LEVEL_CASE__
    default:
      return "?";
  }
}

// Returns the full name of the given log level.
static const char *get_log_level_name(log_level_t level) {
  switch (level) {
#define __LEVEL_CASE__(Name, C) case ll##Name: return #Name;
    ENUM_LOG_LEVELS(__LEVEL_CASE__)
#undef __LEVEL_CASE__
    default:
      return "?";
  }
}

void log_message(log_level_t level, const char *file, int line, const char *fmt,
    ...) {
  va_list argp;
  va_start(argp, fmt);
  vlog_message(level, file, line, fmt, argp);
  va_end(argp);
}

void vlog_message(log_level_t level, const char *file, int line, const char *fmt,
    va_list argp) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Format the timestamp.
  time_t current_time;
  time(&current_time);
  struct tm local_time = *localtime(&current_time);
  char timestamp[128];
  strftime(timestamp, 128, "%d%m%H%M%S", &local_time);
  // Print the result.
  fprintf(stderr, "%s:%i: %s: %s [%s%s]\n", file, line, get_log_level_name(level),
      str.chars, get_log_level_char(level), timestamp);
  string_buffer_dispose(&buf);
}
