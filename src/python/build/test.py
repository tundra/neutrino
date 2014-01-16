# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for running tests.


import os
from . import process


# A build dependency node that represents running a test case.
class ExecTestCaseNode(process.CustomExecNode):

  def __init__(self, name, context, subject):
    super(ExecTestCaseNode, self).__init__(name, context, subject)

  def get_output_file(self):
    (subject_root, subject_ext) = os.path.splitext(self.subject)
    filename = "%s.run" % subject_root
    return self.get_context().get_outdir_file(filename)

  def should_tee_output(self):
    return True



# The tools for working with C. Available in mkmk files as "c".
class TestTools(process.ToolSet):

  # Returns an empty executable node that can then be configured.
  def get_exec_test_case(self, name):
    return self.get_context().get_or_create_node(name + ":test", ExecTestCaseNode,
      name)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return TestTools(context)
