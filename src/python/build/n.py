# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for building neutrino code.


from . import process
import plankton.options


# A node representing a neutrino source file.
class NSourceNode(process.PhysicalNode):

  def __init__(self, name, context, handle):
    super(NSourceNode, self).__init__(name, context)
    self.handle = handle

  def get_input_file(self):
    return self.handle


class NLibrary(process.PhysicalNode):
  
  # Adds a source file that should be included in this library.
  def add_source(self, node):
    self.add_dependency(node, src=True)
    return self

  # Sets the compiler executable to use when building this library.
  def set_compiler(self, compiler):
    self.add_dependency(compiler, compiler=True)
    return self

  def get_output_file(self):
    name = self.get_name()
    filename = "%s.nl" % name
    return self.get_context().get_outdir_file(filename)

  def get_command_line(self, platform):
    [compiler_node] = self.get_input_nodes(compiler=True)
    compile_command_line = compiler_node.get_run_command_line(platform)
    outpath = self.get_output_path()
    files = self.get_input_paths(src=True)
    options = plankton.options.Options()
    options.add_flag("build-library", {
      "out": outpath,
      "modules": files
    })
    command = "%s --compile %s" % (compile_command_line, options.base64_encode())
    return process.Command(command)


# The tools for working with neutrino. Available in mkmk files as "n".
class NTools(process.ToolSet):

  # Returns the source file under the current path with the given name.
  def get_source_file(self, name):
    handle = self.context.get_file(name)
    return self.get_context().get_or_create_node(name, NSourceNode, handle)

  # Returns a neutrino library file under the current path.
  def get_library(self, name):
    return self.get_context().get_or_create_node(name, NLibrary)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return NTools(context)
