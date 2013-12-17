# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Tools for building C code.


import os.path
from . import process
import re


# A build dependency node that represents an executable.
class ExecutableNode(process.PhysicalNode):

  def __init__(self, name, context):
    super(ExecutableNode, self).__init__(name, context)

  def get_output_file(self):
    name = self.get_name()
    ext = self.get_context().get_platform().get_executable_file_ext()
    if ext:
      filename = "%s.%s" % (name, ext)
    else:
      filename = name
    return self.get_context().get_outdir_file(filename)

  # Adds an object file to be compiled into this executable. Groups will be
  # flattened.
  def add_object(self, node):
    self.add_dependency(node, obj=True)

  def get_command_line(self, platform):
    outpath = self.get_output_path()
    inpaths = self.get_input_paths(obj=True)
    return platform.get_executable_compile_command(outpath, inpaths)


# A node representing a C source file.
_HEADER_PATTERN = re.compile(r'#include\s+"([^"]+)"')
class SourceNode(process.PhysicalNode):

  def __init__(self, name, context, handle):
    super(SourceNode, self).__init__(name, context)
    self.handle = handle
    self.includes = set()
    self.headers = None

  def get_display_name(self):
    return self.handle.get_path()

  def get_input_file(self):
    return self.handle

  # Returns the list of names included into the given file. Used to calculate
  # the transitive includes.
  def get_include_names(self, handle):
    return handle.get_attribute("include names", SourceNode.scan_for_include_names)

  # Scans a file for includes. You generally don't want to call this directly
  # because it's slow, instead use get_include_names which caches the result on
  # the file handle.
  @staticmethod
  def scan_for_include_names(handle):
    result = set()
    for line in handle.read_lines():
      match = _HEADER_PATTERN.match(line)
      if match:
        name = match.group(1)
        result.add(name)
    return sorted(list(result))

  # Returns the list of headers included (including transitively) into this
  # source file.
  def get_included_headers(self):
    if self.headers is None:
      self.headers = self.calc_included_headers()
    return self.headers
  
  # Calculates the list of handles of files included by this source file.
  def calc_included_headers(self):
    headers = set()
    files_scanned = set()
    folders = [self.handle.get_parent()] + list(self.includes)
    names_seen = set()
    # Scans the contents of the given file handle for includes, recursively
    # resolving them as they're encountered.
    def scan_file(handle):
      if (not handle.exists()) or (handle.get_path() in files_scanned):
        return
      files_scanned.add(handle.get_path())
      for name in self.get_include_names(handle):
        resolve_include(name)
    # Looks for the source of a given include in the include paths and if found
    # recursively scans the file for includes.
    def resolve_include(name):
      if name in names_seen:
        return
      names_seen.add(name)
      for parent in folders:
        candidate = parent.get_child(name)
        if candidate.exists():
          if not candidate in headers:
            headers.add(candidate)
            scan_file(candidate)
          return
    scan_file(self.handle)
    return sorted(list(headers))

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
    return self.context.get_or_create_node("%s:object" % name, ObjectNode, self)


# A node representing a built object file.
class ObjectNode(process.PhysicalNode):
  
  def __init__(self, name, context, source):
    super(ObjectNode, self).__init__(name, context)
    self.add_dependency(source, src=True)
    self.source = source

  def get_source(self):
    return self.source

  def get_output_file(self):
    source_name = self.get_source().get_name()
    (source_name_root, source_name_ext) = os.path.splitext(source_name)
    ext = self.get_context().get_platform().get_object_file_ext()
    object_name = "%s.%s" % (source_name_root, ext)
    return self.get_context().get_outdir_file(object_name)

  def get_command_line(self, platform):
    includepaths = self.source.get_includes()
    outpath = self.get_output_path()
    inpaths = self.get_input_paths(src=True)
    includes = [p.get_path() for p in includepaths]
    return platform.get_object_compile_command(outpath, inpaths,
      includepaths=includes)

  def get_computed_dependencies(self):
    return self.get_source().get_included_headers()


# Node that represents the action of printing the build environment to stdout.
class EnvPrinterNode(process.VirtualNode):

  def get_command_line(self, platform):
    return platform.get_print_env_command()


# The tools for working with C. Available in mkmk files as "c".
class CTools(process.ToolSet):

  # Returns the source file under the current path with the given name.
  def get_source_file(self, name):
    handle = self.context.get_file(name)
    return self.get_context().get_or_create_node(name, SourceNode, handle)

  # Returns an empty executable node that can then be configured.
  def get_executable(self, name):
    return self.get_context().get_or_create_node(name, ExecutableNode)

  def get_env_printer(self, name):
    return self.get_context().get_or_create_node(name, EnvPrinterNode)


# Entry-point used by the framework to get the tool set for the given context.
def get_tools(context):
  return CTools(context)
