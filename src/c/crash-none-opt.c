// Copyright 2013 the Neutrino authors (see AUTHORS).
// Licensed under the Apache License, Version 2.0 (see LICENSE).

// Implementation of crash that does nothing.

void install_crash_handler() { }

void check_fail(const char *file, int line, const char *fmt, ...) { }

void sig_check_fail(const char *file, int line, int signal_cause,
    const char *fmt, ...) { }
