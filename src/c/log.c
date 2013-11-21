// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "log.h"
#include "utils.h"

#include <time.h>

// Returns the initial for the given log level.
static const char *get_log_level_char(log_level_t level) {
  switch (level) {
#define __LEVEL_CASE__(Name, C, V, S) case ll##Name: return #C;
    ENUM_LOG_LEVELS(__LEVEL_CASE__)
#undef __LEVEL_CASE__
    default:
      return "?";
  }
}

// Returns the full name of the given log level.
static const char *get_log_level_name(log_level_t level) {
  switch (level) {
#define __LEVEL_CASE__(Name, C, V, S) case ll##Name: return #Name;
    ENUM_LOG_LEVELS(__LEVEL_CASE__)
#undef __LEVEL_CASE__
    default:
      return "?";
  }
}

// Returns the destination stream of the given log level.
static log_stream_t get_log_level_destination(log_level_t level) {
  switch (level) {
#define __LEVEL_CASE__(Name, C, V, S) case ll##Name: return S;
    ENUM_LOG_LEVELS(__LEVEL_CASE__)
#undef __LEVEL_CASE__
    default:
      return lsStderr;
  }
}

void log_message(log_level_t level, const char *file, int line, const char *fmt,
    ...) {
  va_list argp;
  va_start(argp, fmt);
  vlog_message(level, file, line, fmt, argp);
  va_end(argp);
}

static log_callback_t kDefaultLogCallback;
static log_callback_t *global_log_callback = NULL;

// The default abort handler which prints the message to stderr and aborts
// execution.
static void default_log(void *data, log_entry_t *entry) {
  if (entry->file == NULL) {
    // This is typically used for testing where including the filename and line
    // makes the output unpredictable.
    fprintf(stderr, "%s: %s\n",
        get_log_level_name(entry->level), entry->message->chars);
  } else {
    fprintf(stderr, "%s:%i: %s: %s [%s%s]\n", entry->file, entry->line,
        get_log_level_name(entry->level), entry->message->chars,
        get_log_level_char(entry->level), entry->timestamp->chars);
  }
}

// Returns the current global abort callback.
static log_callback_t *get_global_log_callback() {
  if (global_log_callback == NULL) {
    init_log_callback(&kDefaultLogCallback, default_log, NULL);
    global_log_callback = &kDefaultLogCallback;
  }
  return global_log_callback;
}

void init_log_callback(log_callback_t *callback, log_function_t *function,
    void *data) {
  callback->function = function;
  callback->data = data;
}

log_callback_t *set_log_callback(log_callback_t *value) {
  log_callback_t *result = get_global_log_callback();
  global_log_callback = value;
  return result;
}

void log_entry_init(log_entry_t *entry, log_stream_t destination,
    const char *file, int line, log_level_t level, string_t *message,
    string_t *timestamp) {
  entry->destination = destination;
  entry->file = file;
  entry->line = line;
  entry->level = level;
  entry->message = message;
  entry->timestamp = timestamp;
}


void vlog_message(log_level_t level, const char *file, int line, const char *fmt,
    va_list argp) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t message_str;
  string_buffer_flush(&buf, &message_str);
  // Format the timestamp.
  time_t current_time;
  time(&current_time);
  struct tm local_time = *localtime(&current_time);
  char timestamp[128];
  size_t timestamp_chars = strftime(timestamp, 128, "%d%m%H%M%S", &local_time);
  string_t timestamp_str = {timestamp_chars, timestamp};
  log_stream_t destination = get_log_level_destination(level);
  // Print the result.
  log_entry_t entry;
  log_entry_init(&entry, destination, file, line, level, &message_str,
      &timestamp_str);
  log_callback_t *callback = get_global_log_callback();
  (callback->function)(callback->data, &entry);
  string_buffer_dispose(&buf);
}
