#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'python'))
import plankton
import neutrino


class LibTool(object):

  def main(self, args):
    options = plankton.options.Options.base64_decode(args[0])
    for subcommand in options.get_arguments():
      self.dispatch_subcommand(subcommand)

  # Dispatches a subcommand to the appropriate handler.
  def dispatch_subcommand(self, command):
    full_args = command.get_arguments()
    name = full_args[0]
    rest_args = full_args[1:]
    kwargs = command.get_flag_map()
    handler = getattr(self, 'handle_%s' % name)
    handler(*rest_args, **kwargs)

  # Implements the index subcommand.
  def handle_index(self, filename):
    library = self.load_library(filename)
    print "Index of %s:" % filename
    for path in sorted(library.modules.keys()):
      module = library.modules[path]
      print "  %s => %s" % (path, module)
      for fragment in module.fragments:
        print "    %s: %s" % (fragment.stage, fragment)
        for require in fragment.imports:
          print "      import %s" % require

  # Reads a library from the file with the given name.
  def load_library(self, filename):
    contents = open(filename, "rb").read()
    decoder = plankton.Decoder()
    return decoder.decode(bytearray(contents))


if __name__ == '__main__':
  LibTool().main(sys.argv[1:])
