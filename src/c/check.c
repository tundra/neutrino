#include "globals.h"

// Abort when a check fails.
void check_fail(const char *file, int line) {
  printf("%s:%i: Check failed.", file, line);
  abort();
}
