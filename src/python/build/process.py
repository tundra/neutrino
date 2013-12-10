# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import os.path
import re
import sys


class Action(object):

  def __init__(self):
    pass


def shell_escape(s):
  return re.sub(r'([\s:])', r"\\\g<1>", s)


class MakefileTarget(object):

  def __init__(self, output, inputs, commands):
    self.output = output
    self.inputs = inputs
    self.commands = commands

  def get_output_path(self):
    return self.output.get_path()

  def write(self, out):
    outpath = self.output.get_path()
    inpaths = [i.get_path() for i in self.inputs]
    out.write("%(outpath)s: %(inpaths)s\n\t%(commands)s\n\n" % {
      "outpath": shell_escape(outpath),
      "inpaths": " ".join(map(shell_escape, inpaths)),
      "commands": "\n\t".join(self.commands)
    })


class Makefile(object):

  def __init__(self):
    self.targets = {}

  def add_target(self, output, inputs, commands):
    target = MakefileTarget(output, inputs, commands)
    self.targets[output.get_path()] = target

  def write(self, out):
    for name in sorted(self.targets.keys()):
      target = self.targets[name]
      target.write(out)
    


# A segmented name. This is sort of like a relative file path but avoids any
# ambiguities that might be caused by multiple relative paths pointing to the
# same thing and differences in path separators etc.
class Name(object):

  def __init__(self, parts):
    self.parts = tuple(parts)

  def append(self, *subparts):
    return Name(self.parts + subparts)

  def get_parts(self):
    return self.parts

  def get_last_part(self):
    return self.parts[-1]

  @staticmethod
  def of(*parts):
    return Name(parts)

  def __hash__(self):
    return ~hash(self.parts)

  def __eq__(self, that):
    return (self.parts == that.parts)

  def __cmp__(self, that):
    return cmp(self.parts, that.parts)

  def __str__(self):
    return "|".join(self.parts)


# An abstract file wrapper that encapsulates various file operations.
class AbstractFile(object):

  def __init__(self, path):
    self.path = path

  @staticmethod
  def at(path):
    if os.path.isdir(path):
      return Folder(path)
    elif os.path.isfile(path):
      return RegularFile(path)
    else:
      return MissingFile(path)

  # Returns the folder that contains this file.
  def get_parent(self):
    return AbstractFile.at(os.path.dirname(self.path))

  # Returns a file representing the child under this folder with the given path.
  def get_child(self, *child_path):
    return AbstractFile.at(os.path.join(self.path, *child_path))

  # Returns the raw underlying string path.
  def get_path(self):
    return self.path


class MissingFile(AbstractFile):

  def __init__(self, path):
    super(MissingFile, self).__init__(path)

  def __str__(self):
    return "Missing(%s)" % self.get_path()


# A wrapper around a regular file.
class RegularFile(AbstractFile):

  def __init__(self, path):
    super(RegularFile, self).__init__(path)

  def __str__(self):
    return "File(%s)" % self.get_path()


# A wrapper around a folder.
class Folder(AbstractFile):

  def __init__(self, path):
    super(Folder, self).__init__(path)

  def __str__(self):
    return "Folder(%s)" % self.get_path()


# An abstract build node.
class Node(object):

  def __init__(self, name, context):
    self.context = context
    self.name = name
    self.deps = []
    self.extra_deps = []
    self.full_name = self.context.get_full_name().append(self.name)

  # Returns the name of this node, the last part of the full name of this node.
  def get_name(self):
    return self.name

  # Returns the full absolute name of this node.
  def get_full_name(self):
    return self.full_name

  # Returns the display name that best describes this node. This is only used
  # for describing the node in output, it makes no semantic difference.
  def get_display_name(self):
    return self.get_full_name()

  def __str__(self):
    return "%s(%s, %s)" % (type(self).__name__, self.context, self.name)

  def add_dependency(self, node, quiet=False):
    if quiet:
      self.extra_deps.append(node)
    else:
      self.deps.append(node)

  def get_dependencies(self):
    return self.deps

  def get_flat_dependencies(self):
    return Node.flatten_dependencies(self.get_dependencies())

  def get_flat_extra_dependencies(self):
    return Node.flatten_dependencies(self.extra_deps)

  @staticmethod
  def flatten_dependencies(deps):
    for dep in deps:
      if dep.is_group():
        for subdep in dep.get_flat_dependencies():
          yield subdep
      else:
        yield dep

  # Returns the file that represents the output of processing this node. If
  # the node doesn't require processing returns None.
  def get_output_file(self):
    return None

  # Returns the file that represents this file when used as input to actions.
  def get_input_file(self):
    result = self.get_output_file()
    assert not result is None
    return result

  def get_context(self):
    return self.context

  def is_group(self):
    return False


class GroupNode(Node):

  def add_element(self, node):
    self.add_dependency(node)

  def is_group(self):
    return True


# A context that handles loading an individual mkmk file. Toplevel functions in
# the mkmk becomes method calls on this object.
class ConfigContext(object):

  def __init__(self, env, home, full_name):
    self.env = env
    self.home = home
    self.full_name = full_name

  def get_node(self, name, Class, *args):
    node_name = self.get_full_name().append(name)
    return self.env.get_node(node_name, Class, self, *args)

  # Builds the environment dictionary containing all the toplevel functions in
  # the mkmk. Methods whose names start with handle_ are automatically made
  # available.
  def get_script_environment(self):
    # Create a copy of the tools environment provided by the shared env.
    result = dict(self.env.get_tools(self))
    for (name, value) in dict(self.__class__.__dict__).items():
      match = re.match(r'^handle_(\w+)$', name)
      if match:
        result[match.group(1)] = getattr(self, name)
    return result

  # Includes the given mkmk into the set of dependencies for this build process.
  def handle_include(self, *rel_mkmk_path):
    full_mkmk = self.home.get_child(*rel_mkmk_path)
    rel_parent_path = rel_mkmk_path[:-1]
    full_name = self.full_name.append(*rel_parent_path)
    mkmk_home = full_mkmk.get_parent()
    subcontext = ConfigContext(self.env, mkmk_home, full_name)
    subcontext.load(full_mkmk)

  # Returns a file object for the given path relative to the current context.
  def handle_get_file(self, *file_path):
    return self.get_file(*file_path)

  def handle_get_group(self, name):
    return self.get_node(name, GroupNode)

  def handle_get_external(self, *names):
    return self.env.get_existing_node(Name.of(*names))

  def handle_get_root(self):
    return self.env.get_root()

  def handle_get_bindir(self):
    return self.env.get_bindir()

  def get_file(self, *file_path):
    return self.home.get_child(*file_path)

  def get_platform(self):
    return self.env.get_platform()

  # Does the actual work of loading the mkmk file this context corresponds to.
  def load(self, mkmk_file):
    with open(mkmk_file.get_path()) as handle:
      code = compile(handle.read(), mkmk_file.get_path(), "exec")
      exec(code, self.get_script_environment())

  # Returns the full name of the script represented by this context.
  def get_full_name(self):
    return self.full_name

  def handle_get_outdir_file(self, name, ext=None):
    return self.get_outdir_file(name, ext)

  def get_outdir_file(self, name, ext=None):
    if ext:
      full_out_name = self.get_full_name().append("%s.%s" %  (name, ext))
    else:
      full_out_name = self.get_full_name().append(name)
    return self.env.get_bindir().get_child(*full_out_name.get_parts())

  def __str__(self):
    return "Context(%s)" % self.home


# The static environment that is shared and constant between all contexts and
# across the lifetime of the build process.
class Environment(object):

  def __init__(self, root, bindir, platform):
    self.root = root
    self.bindir = bindir
    self.platform = platform
    self.modules = None
    self.nodes = {}

  def get_node(self, full_name, Class, *args):
    if full_name in self.nodes:
      return self.nodes[full_name]
    new_node = Class(full_name.get_last_part(), *args)
    self.nodes[full_name] = new_node
    return new_node

  def get_existing_node(self, full_name):
    return self.nodes[full_name]

  def get_root(self):
    return self.root

  def get_platform(self):
    return self.platform

  # Returns the map of tools for the given context.
  def get_tools(self, context):
    result = {}
    for (name, module) in self.get_modules():
      tools = module.get_tools(context)
      result[name] = tools
    return result

  # Returns a list of the python modules supported by this environment.
  def get_modules(self):
    if self.modules is None:
      self.modules = list(Environment.generate_tool_modules())
    return self.modules

  def get_bindir(self):
    return self.bindir

  def write_dot_graph(self, out):
    def dot_escape(s):
      return re.sub(r'\W', '_', s)
    out.write("digraph G {\n")
    out.write("  rankdir=LR;\n")
    for node in self.nodes.values():
      full_name = node.get_full_name()
      escaped = dot_escape(str(full_name))
      display_name = node.get_display_name()
      out.write("  %s [label=\"%s\"];\n" % (escaped, display_name))
      for dep in node.deps:
        dep_name = dep.get_full_name()
        escaped_dep = dot_escape(str(dep_name))
        out.write("    %s -> %s;\n" % (escaped, escaped_dep))
    out.write("}\n")

  def write_makefile(self, out):
    makefile = Makefile()
    for node in self.nodes.values():
      output = node.get_output_file()
      if not output:
        continue
      inputs = [dep.get_input_file() for dep in node.get_flat_dependencies()]
      extras = [ext.get_input_file() for ext in node.get_flat_extra_dependencies()]
      action = node.get_action()
      outpath = output.get_path()
      inpaths = [i.get_path() for i in inputs]
      mkdir_command = self.platform.get_ensure_folder_command(output.get_parent().get_path())
      process_command = action.get_commands(self.platform, outpath, inpaths)
      makefile.add_target(output, inputs + extras, mkdir_command + process_command)
    makefile.write(out)

  # Lists all the tool modules.
  @staticmethod
  def generate_tool_modules():
    from . import c
    from . import toc
    yield ('c', c)
    yield ('toc', toc)


def main(root, bindir, dot=None, makefile=None, toolchain="gcc"):
  from . import platform
  root_mkmk = AbstractFile.at(root)
  # The path that contains the .mkmk file.
  root_mkmk_home = root_mkmk.get_parent()
  gcc = platform.get_toolchain(toolchain)
  bindir = AbstractFile.at(bindir)
  env = Environment(root_mkmk_home, bindir, gcc)
  context = ConfigContext(env, root_mkmk_home, Name.of())
  context.load(root_mkmk)
  if dot:
    env.write_dot_graph(open(dot, "wt"))
  if makefile:
    env.write_makefile(open(makefile, "wt"))
  