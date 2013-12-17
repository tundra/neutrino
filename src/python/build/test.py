# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for running tests.


import os
from . import process


# A build dependency node that represents running a test case.
class ExecTestCaseNode(process.PhysicalNode):

  def __init__(self, name, context, subject):
    super(ExecTestCaseNode, self).__init__(name, context)
    self.subject = subject
    self.args = []

  def get_output_file(self):
    (subject_root, subject_ext) = os.path.splitext(self.subject)
    filename = "%s.run" % subject_root
    return self.get_context().get_outdir_file(filename)

  def get_command_line(self, platform):
    [runner] = self.get_input_paths(runner=True)
    outpath = self.get_output_path()
    args = " ".join(self.get_arguments())
    raw_command_line = "%s %s" % (runner, args)
    return platform.get_safe_tee_command(raw_command_line, outpath)

  # Sets the executable used to run this test case.
  def set_runner(self, node):
    self.add_dependency(node, runner=True)
    return self

  # Sets the (string) arguments to pass to the runner to execute the test.
  def set_arguments(self, *args):
    self.args = args
    return self

  # Returns the argument list to pass when executing this test.
  def get_arguments(self):
    return self.args


# The tools for working with C. Available in mkmk files as "c".
class TestTools(process.ToolSet):

  # Returns an empty executable node that can then be configured.
  def get_exec_test_case(self, name):
    return self.get_context().get_or_create_node(name + ":test", ExecTestCaseNode,
      name)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return TestTools(context)
