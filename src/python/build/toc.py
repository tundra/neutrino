#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from . import process
import re
import sys


class GenerateTocAction(process.Action):

  def __init__(self, generator):
    self.generator = generator
  
  def get_commands(self, platform, outfile, infiles):
    command = "%(generator)s %(infiles)s > %(outfile)s" % {
      "generator": self.generator.get_path(),
      "infiles": " ".join(infiles),
      "outfile": outfile
    }
    return [command]


class TocNode(process.Node):

  def __init__(self, name, context):
    super(TocNode, self).__init__(name, context)
    self.generator = None

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.get_name())

  def get_action(self):
    return GenerateTocAction(self.generator)

  def set_generator(self, generator):
    self.generator = generator


class TocTools(object):

  def __init__(self, context):
    self.context = context

  def get_toc_file(self, name):
    return self.context.get_node(name, TocNode)


def get_tools(context):
  return TocTools(context)


def main(args):
  headers = []
  for arg in args:
    for line in open(arg):
      match = re.match(r"TEST\((.*)\)", line)
      if match:
        headers.append(match.group(1))
  for header in headers:
    print("DECLARE_TEST(%s);" % header)
  print("ENUMERATE_TESTS_HEADER {")
  for header in headers:
    print("  ENUMERATE_TEST(%s);" % header)
  print("}")

if __name__ == '__main__':
  main(sys.argv[1:])
