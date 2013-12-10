# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from . import process


class CompileObjectAction(process.Action):

  def __init__(self, includepaths):
    self.includepaths = includepaths

  def get_commands(self, platform, outpath, inpaths):
    includes = [p.get_path() for p in self.includepaths]
    return platform.get_object_compile_command(outpath, inpaths,
      includepaths=includes)


class CompileExecutableAction(process.Action):

  def get_commands(self, platform, outpath, inpaths):
    return platform.get_executable_compile_command(outpath, inpaths)


class ExecutableNode(process.Node):

  def __init__(self, name, context):
    super(ExecutableNode, self).__init__(name, context)

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.name)

  def add_library(self, lib_node):
    self.add_dependency(lib_node)

  def get_action(self):
    return CompileExecutableAction()


# A node representing a source file.
class SourceNode(process.Node):

  def __init__(self, name, context, handle):
    super(SourceNode, self).__init__(name, context)
    self.handle = handle

  def get_display_name(self):
    return self.handle.get_path()

  def get_input_file(self):
    return self.handle

  def get_object(self):
    name = self.get_name()
    ext = self.get_context().get_platform().get_object_file_ext()
    return self.context.get_node("%s.%s" % (name, ext), ObjectNode, self)


# A node representing a built object file.
class ObjectNode(process.Node):
  
  def __init__(self, name, context, source):
    super(ObjectNode, self).__init__(name, context)
    self.add_dependency(source)
    self.includepaths = []

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.name)

  def add_include_path(self, path):
    self.includepaths.append(path)

  def get_action(self):
    return CompileObjectAction(self.includepaths)


class CTools(object):

  def __init__(self, context):
    self.context = context

  def get_source_file(self, name):
    handle = self.context.get_file(name)
    return self.context.get_node(name, SourceNode, handle)

  def get_executable(self, name):
    return self.context.get_node(name, ExecutableNode)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return CTools(context)
