#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import os
import os.path
import subprocess
import re
import sys

def main():
  # Collection of all the lines along with a flag saying whether to censor the
  # line if a crash line is found. The flag is used to censor stack lines that
  # relate to printing the stack trace but _only_ if we recognize a call into
  # the crash library. If there is no call it's better to print everything,
  # otherwise we'll end up censoring the complete trace.
  lines = []
  crash_line_seen = False
  for raw_line in sys.stdin:
    # The stack-like lines we're looking for.
    match = re.match(r'^(.*\s)(\S*)\((.*)\) \[(0x[0-9a-fA-F]*)\](.*)$', raw_line)
    if not match:
      # The line didn't look like a stack entry.
      lines.append((False, raw_line))
      continue
    (prefix, executable, location, address, suffix) = match.groups()
    # Call addr2line to resolve the address within the executable
    addrline = subprocess.check_output(["addr2line", "-e", executable, address])
    addrline = addrline.strip()
    # Build the output line, replacing the executable/address with the output
    # from addr2line.
    line = "%s%s (%s)%s\n" % (prefix, location, addrline, suffix)
    lines.append((not crash_line_seen, line))
    # Then check if this is the call into the crash library. Doing it after
    # means that the crash line itself will be censored.
    is_crash_line = "crash.c" in addrline
    crash_line_seen = crash_line_seen or is_crash_line
  # Compress the absolute path here to '.' and censor crash lines.
  here = os.path.abspath(os.curdir)
  for (censor_if_crash_line_seen, line) in lines:
    if crash_line_seen and censor_if_crash_line_seen:
      continue
    short_line = line.replace(here, '.')
    print short_line,

if __name__ == '__main__':
  main()
