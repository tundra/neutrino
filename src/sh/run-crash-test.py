#!/usr/bin/python
# Copyright 2015 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Runs a crash test, checking that the process terminates normally.

import difflib
import subprocess
import sys

def main():
  [exe, arg] = sys.argv[1:]
  command = [exe, '"%s"' % arg]
  process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (stdout, stderr) = process.communicate()
  exit_code = process.returncode
  if exit_code in [0, 1]:
    sys.exit(0)
  else:
    sys.exit(1)

if __name__ == '__main__':
  main()
