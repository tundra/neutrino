#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Runs an individual nunit test.

import glob
import optparse
import os
import subprocess
import sys
from tempfile import SpooledTemporaryFile
from plankton import options


# Holds state for the main entry-point
class Main(object):

  def __init__(self):
    self.flags = None

  def main(self):
    self.parse_flags()
    try:
      self.run_test()
    except subprocess.CalledProcessError, e:
      sys.exit(e.returncode)

  # Runs the test.
  def run_test(self):
    modules = self.find_modules()
    # Compile the test file. If compilation fails this will throw an exception,
    data = subprocess.check_output([
      "./src/python/neutrino/main.py",
      "--file", self.flags.test
    ] + modules)
    # Write the compiled output to a temporary file so it gets a file descriptor
    # and can be passed to the executable.
    tmp = SpooledTemporaryFile()
    tmp.write(data)
    tmp.flush()
    tmp.seek(0)
    try:
      # Take input from stdin. Better would be to pass the command as a list
      # but the executables seem to see different input when run with a list of
      # arguments than with a space-separated string.
      command = "%s -" % self.flags.runner
      output = subprocess.check_output(command, shell=True, stdin=tmp)
    finally:
      tmp.close()
    open(self.flags.out, "wt").write(output)

  # Returns the arguments that specify which modules to make available to the
  # tests.
  def find_modules(self):
    modules = []
    for name in self.flags.modules:
      modules += ['--module', name]
    return modules

  def parse_flags(self):
    parsed = options.base64_to_options(sys.argv[1])
    self.flags = parsed.get_flags()


if __name__ == '__main__':
  Main().main()
