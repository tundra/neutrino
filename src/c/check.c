#include "globals.h"
#include "utils.h"

void check_fail(const char *file, int line, const char *fmt, ...) {
  // Write the error message into a string buffer.
  string_buffer_t buf;
  string_buffer_init(&buf, NULL);
  va_list argp;
  va_start(argp, fmt);
  string_buffer_vprintf(&buf, fmt, argp);
  va_end(argp);
  // Flush the string buffer.
  string_t str;
  string_buffer_flush(&buf, &str);
  // Print the formatted error message.
  fprintf(stderr, "%s:%i: %s\n", file, line, str.chars);
  fflush(stderr);
  string_buffer_dispose(&buf);
  abort();
}
