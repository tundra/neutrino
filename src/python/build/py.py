# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for building C code.


from . import process


# A node representing a python source file.
class PythonSourceNode(process.PhysicalNode):

  def __init__(self, name, context, handle):
    super(PythonSourceNode, self).__init__(name, context)
    self.handle = handle
    self.pythonpath = set()

  def get_input_file(self):
    return self.handle

  def add_pythonpath(self, handle):
    self.pythonpath.add(handle.get_path())
    return self

  def get_run_command_line(self, platform):
    path = ":".join(sorted(list(self.pythonpath)))
    command = "python %s" % self.handle.get_path()
    if path:
      command = "PYTHONPATH=\"$PYTHONPATH:%s\" %s" % (path, command)
    return command


# The tools for working with C. Available in mkmk files as "c".
class PythonTools(process.ToolSet):

  # Returns the source file under the current path with the given name.
  def get_source_file(self, name):
    handle = self.context.get_file(name)
    return self.get_context().get_or_create_node(name, PythonSourceNode, handle)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return PythonTools(context)
