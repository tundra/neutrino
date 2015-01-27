#!/usr/bin/python
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Runs a message test, comparing the output with the expected output.

import difflib
import subprocess
import sys

def main():
  # Run the command, building the output.
  command = sys.argv[2:]
  print " ".join(command)
  process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (stdout, stderr) = process.communicate()
  output_found = stdout + stderr
  output_path = sys.argv[1]
  # Read the expected output.
  output_expected = open(output_path, "rt").read()
  if output_found == output_expected:
    sys.exit(0)
  # If there's a difference print it as a diff.
  diff = difflib.unified_diff(output_expected.splitlines(), output_found.splitlines())
  for line in diff:
    print line
  # Exit with an error returncode.
  sys.exit(1)

if __name__ == '__main__':
  main()
