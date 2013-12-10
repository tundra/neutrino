# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for building C code.


from . import process


# Compiles a source file into an object file.
class CompileObjectAction(process.Action):

  def __init__(self, includepaths):
    self.includepaths = includepaths

  def get_commands(self, platform, outpath, node):
    inpaths = node.get_input_paths(src=True)
    includes = [p.get_path() for p in self.includepaths]
    return platform.get_object_compile_command(outpath, inpaths,
      includepaths=includes)


# Compiles a set of object files into an executable.
class CompileExecutableAction(process.Action):

  def get_commands(self, platform, outpath, node):
    inpaths = node.get_input_paths(obj=True)
    return platform.get_executable_compile_command(outpath, inpaths)


# A build dependency node that represents an executable.
class ExecutableNode(process.Node):

  def __init__(self, name, context):
    super(ExecutableNode, self).__init__(name, context)

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.name)

  # Adds an object file to be compiled into this executable. Groups will be
  # flattened.
  def add_object(self, node):
    if node.is_group():
      for edge in node.get_direct_edges():
        self.add_object(edge.get_target())
    else:
      self.add_dependency(node, obj=True)

  def get_action(self):
    return CompileExecutableAction()


# A node representing a C source file.
class SourceNode(process.Node):

  def __init__(self, name, context, handle):
    super(SourceNode, self).__init__(name, context)
    self.handle = handle
    self.includes = set()

  def get_display_name(self):
    return self.handle.get_path()

  def get_input_file(self):
    return self.handle

  # Add a folder to the include paths required by this source file. Adding the
  # same path more than once is safe.
  def add_include(self, path):
    self.includes.add(path)

  # Returns a sorted list of the include paths for this source file.
  def get_includes(self):
    return sorted(list(self.includes))

  # Returns a node representing the object produced by compiling this C source
  # file.
  def get_object(self):
    name = self.get_name()
    ext = self.get_context().get_platform().get_object_file_ext()
    return self.context.get_or_create_node("%s.%s" % (name, ext), ObjectNode, self)


# A node representing a built object file.
class ObjectNode(process.Node):
  
  def __init__(self, name, context, source):
    super(ObjectNode, self).__init__(name, context)
    self.add_dependency(source, src=True)
    self.source = source

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.name)

  def get_action(self):
    return CompileObjectAction(self.source.get_includes())


# The tools for working with C. Available in mkmk files as "c".
class CTools(object):

  def __init__(self, context):
    self.context = context

  # Returns the source file under the current path with the given name.
  def get_source_file(self, name):
    handle = self.context.get_file(name)
    return self.context.get_or_create_node(name, SourceNode, handle)

  # Returns an empty executable node that can then be configured.
  def get_executable(self, name):
    return self.context.get_or_create_node(name, ExecutableNode)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return CTools(context)
