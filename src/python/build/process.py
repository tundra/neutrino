# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from abc import ABCMeta, abstractmethod
import os
import os.path
import re
import sys


# A command to be executed on the command-line along with a comment. In non-
# verbose mode only the comment will be displayed while building.
class Command(object):

  def __init__(self, *parts):
    self.parts = parts
    self.comment = None

  def set_comment(self, comment):
    self.comment = comment
    return self

  @staticmethod
  def empty():
    return Command()

  def get_actions(self, env):
    parts = list(self.parts)
    if not env.get_platform().get_config_option('noisy'):
      parts = ["@%s" % a for a in parts]
    if self.comment:
      parts = ["@echo '%s'" % self.comment] + parts
    return parts


# Escapes a string such that it can be passed as an argument in a shell command.
def shell_escape(s):
  return re.sub(r'([\s:()])', r"\\\g<1>", s)


# An individual target within a makefile.
class MakefileTarget(object):

  def __init__(self, output, inputs, commands):
    self.output = output
    self.inputs = inputs
    self.commands = commands

  # Returns the string output path for this target.
  def get_output_path(self):
    return self.output

  # Write this target, in Makefile syntax, to the given output stream.
  def write(self, out):
    out.write("%(outpath)s: %(inpaths)s\n\t%(commands)s\n\n" % {
      "outpath": shell_escape(self.output),
      "inpaths": " ".join(map(shell_escape, self.inputs)),
      "commands": "\n\t".join(self.commands)
    })


# The contents of a complete makefile. A makefile object is meant to be dumb and
# basically only concerned with accumulating and then printing the makefile
# source. Any nontrivial logic is the responsibility of whoever is building the
# makefile object.
class Makefile(object):

  def __init__(self):
    self.targets = {}
    self.phonies = set()

  # Add a target that builds the given output from the given inputs by invoking
  # the given commands in sequence.
  def add_target(self, output, inputs, commands, is_phony):
    target = MakefileTarget(output, inputs, commands)
    self.targets[output] = target
    if is_phony:
      self.phonies.add(output)

  # Write this makefile in Makefile syntax to the given stream.
  def write(self, out):
    for name in sorted(self.targets.keys()):
      target = self.targets[name]
      target.write(out)
    # Mike Moffit says: list *all* the phonies.
    if self.phonies:
      out.write(".PHONY: %s\n\n" % " ".join(sorted(list(self.phonies))))


# A segmented name. This is sort of like a relative file path but avoids any
# ambiguities that might be caused by multiple relative paths pointing to the
# same thing and differences in path separators etc. Names compare
# lexicographically and are equal if their parts are structurally equal.
class Name(object):

  def __init__(self, parts):
    self.parts = tuple(parts)

  # Returns a new name that consists of this name followed by the given parts.
  def append(self, *subparts):
    return Name(self.parts + subparts)

  # Returns a tuple that holds the parts of this name.
  def get_parts(self):
    return self.parts

  # Returns the last part, for instance the last part of a|b|c is "c".
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

  def __init__(self, path, parent):
    self.path = path
    self.parent = parent
    self.children = {}
    self.attribs = {}

  # Creates a new file object of the appropriate type depending on what kind of
  # file the path points to.
  @staticmethod
  def at(path, parent=None):
    if os.path.isdir(path):
      return Folder(path, parent)
    elif os.path.isfile(path):
      return RegularFile(path, parent)
    else:
      return MissingFile(path, parent)

  # Returns the folder that contains this file.
  def get_parent(self):
    if self.parent is None:
      self.parent = AbstractFile.at(os.path.dirname(self.path))
    return self.parent

  # Returns a file representing the child under this folder with the given path.
  def get_child(self, *child_path):
    current = self
    for part in child_path:
      current = current.get_local_child(part)
    return current

  # Like get_child but only takes a single argument so only files directly under
  # this folder can be accessed this way.
  def get_local_child(self, part):
    if not part in self.children:
      child = AbstractFile.at(os.path.join(self.path, part), parent=self)
      self.children[part] = child
    return self.children[part]

  # Returns the raw underlying (relative) string path.
  def get_path(self):
    return self.path

  # Is this file handle backed by a physical file?
  def exists(self):
    return os.path.exists(self.get_path())

  # Returns an in-memory attribute associated with this file, computing it using
  # the given thunk if it doesn't already exist.
  def get_attribute(self, name, thunk):
    if not name in self.attribs:
      self.attribs[name] = thunk(self)
    return self.attribs[name]

  def __cmp__(self, that):
    return cmp(self.path, that.path)

  # There has to be two comparison functions because cmp doesn't work in python
  # 3.
  #
  # Python 3: out users' time is cheap so why make an effort to be compatible?
  def __lt__(self, that):
    return self.path < that.path


# A wrapper that represents a file that doesn't exist yet. Using files that
# don't exist is fine but if you try to interact with them in any nontrivial
# way, for instance read them, it won't work.
class MissingFile(AbstractFile):

  def __init__(self, path, parent):
    super(MissingFile, self).__init__(path, parent)

  def __str__(self):
    return "Missing(%s)" % self.get_path()


# A wrapper around a regular file.
class RegularFile(AbstractFile):

  def __init__(self, path, parent):
    super(RegularFile, self).__init__(path, parent)
    self.lines = None

  def __str__(self):
    return "File(%s)" % self.get_path()

  # Returns an open file handle for the contents of this file.
  def open(self, mode):
    return open(self.get_path(), mode)

  # Returns the contents of this file as a list of strings, one for each line.
  def read_lines(self):
    if self.lines is None:
      self.lines = []
      with self.open("rt") as source:
        for line in source:
          self.lines.append(line)
    return self.lines


# A wrapper around a folder.
class Folder(AbstractFile):

  def __init__(self, path, parent):
    super(Folder, self).__init__(path, parent)

  def __str__(self):
    return "Folder(%s)" % self.get_path()


# An abstract build node. Build nodes are the basic unit of dependencies in the
# build system. They may or may not correspond to a physical file. You don't
# create Nodes directly, instead each type of node is represented by a subclass
# of Node which is what you actually create.
class Node(object):
  __metaclass__ = ABCMeta

  def __init__(self, name, context):
    self.context = context
    self.name = name
    self.edges = []
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

  # Adds an edge from this node to another target node. The edge will be
  # annotated with any keyword arguments specified.
  def add_dependency(self, target, **annots):
    self.edges.append(Edge(target, annots))

  # Returns the raw set of edges, including groups, emanating from this node.
  def get_direct_edges(self):
    return self.edges

  # Generates all edges emanating from this node, flattening groups.
  def get_all_flat_edges(self):
    for edge in self.edges:
      target = edge.get_target()
      for sub_edge in target.flatten_through_edge(edge):
        yield sub_edge

  # Generated the edges emanating from this node, flattening groups. Only edges
  # that match the given annotations are returned.
  def get_flat_edges(self, **annots):
    for edge in self.edges:
      target = edge.get_target()
      for transitive in target.get_flat_edges_through(edge, annots):
        yield transitive

  # Given an edge into this node, generates the set of outgoing edges it should
  # be flattened into that also match the given annotations. This is how groups
  # are implemented: an edge into a group is expanded into the edges pointing
  # through the group.
  def get_flat_edges_through(self, edge, annots):
    # The default behavior is to just yield the edge itself since only groups
    # have special behavior that cause edges to be flattened.
    if edge.has_annotations(annots):
      yield edge

  # Returns the string file paths of all the dependencies from this node that
  # have been annotated in the specified way. Edges with additional annotations
  # are included in the result so specifying an empty annotation set will give
  # all the dependencies.
  def get_input_paths(self, **annots):
    return [n.get_input_file().get_path() for n in self.get_input_nodes(**annots)]

  # Returns the nodes of all the dependencies from this node.
  def get_input_nodes(self, **annots):
    edges = self.get_flat_edges(**annots)
    return [e.get_target() for e in edges]

  # Returns the string file path of the file to output for this node. If this
  # node has no associated output file returns None.
  def get_output_path(self):
    output_file = self.get_output_file()
    if output_file is None:
      return None
    else:
      return output_file.get_path()

  # Returns the file that represents this file when used as input to actions.
  def get_input_file(self):
    result = self.get_output_file()
    assert not result is None
    return result

  # Returns the file that represents the output of processing this node. If
  # the node doesn't require processing returns None.
  def get_output_file(self):
    return None

  # Returns the name of the target produced by this node.
  @abstractmethod
  def get_output_target(self):
    pass

  # Returns the context this node was created within.
  def get_context(self):
    return self.context

  # A hook subclasses can use to add extra, otherwise untracked, files that a
  # node depends on.
  def get_computed_dependencies(self):
    return []

  # Should the corresponding makefile target be marked as phony?
  def is_phony(self):
    return False

  def __str__(self):
    return "%s(%s, %s)" % (type(self).__name__, self.context, self.name)


# A node that has no corresponding physical file.
class VirtualNode(Node):
  __metaclass__ = ABCMeta

  def get_output_target(self):
    return self.get_name()

  def get_command_line(self, platform):
    return None

  def is_phony(self):
    return True


# A node associated with a physical file.
class PhysicalNode(Node):

  def get_output_target(self):
    output_file = self.get_output_file()
    if output_file:
      return output_file.get_path()
    else:
      return None


# A "dumb" node representing a file. If you need any kind of file type specific
# behavior use a subclass of PhysicalNode instead.
class FileNode(PhysicalNode):
  
  def __init__(self, name, context, handle):
    super(FileNode, self).__init__(name, context)
    self.handle = handle

  def get_input_file(self):
    return self.handle

  def get_run_command_line(self, platform):
    return self.handle.get_path()


# A node representing the execution of a custom command.
class CustomExecNode(PhysicalNode):

  def __init__(self, name, context, subject):
    super(CustomExecNode, self).__init__(name, context)
    self.subject = subject
    self.args = []
    self.title = None

  def get_output_file(self):
    return self.get_context().get_outdir_file(self.subject)

  def get_command_line(self, platform):
    runner = self.get_runner_command(platform)
    outpath = self.get_output_path()
    args = " ".join(self.get_arguments())
    raw_command_line = "%s %s" % (runner, args)
    if self.should_tee_output():
      result = platform.get_safe_tee_command(raw_command_line, outpath)
    else:
      result = Command(raw_command_line)
    if self.title is None:
      title = "Running %s" % self.get_full_name()
    else:
      title = self.title
    result.set_comment(title)
    return result

  # Returns the executable to run.
  def get_runner_command(self, platform):
    [runner_node] = self.get_input_nodes(runner=True)
    return runner_node.get_run_command_line(platform)

  # Should the contents of the output file be printed on successful completion?
  def should_tee_output(self):
    return False

  # Sets the executable used to run this node case.
  def set_runner(self, node):
    self.add_dependency(node, runner=True)
    return self

  # Sets the title of the node, the message to print when running it.
  def set_title(self, title):
    self.title = title
    return self

  # Sets the (string) arguments to pass to the runner.
  def set_arguments(self, *args):
    self.args = args
    return self

  # Returns the argument list to pass when executing this node.
  def get_arguments(self):
    return self.args


# A custom executable node that calls a system command.
class SystemExecNode(CustomExecNode):

  def __init__(self, name, context, subject):
    super(SystemExecNode, self).__init__(name, context, subject)
    self.command = None

  def set_command(self, command):
    self.command = command
    return self

  def get_runner_command(self, platform):
    return self.command


# A dependency between nodes. An edge is like a pointer from one node to another
# but additionally carries a set of annotations that control what the pointer
# means. For instance, an object file may depend on both a source file and its
# headers but the compilation command the produces the object only requires the
# source file to be passed, not the headers. In that case the dependencies would
# be annotated differently to account for this difference in how they should be
# handled.
#
# Unlike Node there is only one Edge type which can be used directly.
class Edge(object):

  def __init__(self, target, annots):
    self.target = target
    self.annots = annots

  # Returns the target node for this edge.
  def get_target(self):
    return self.target

  # Returns this node's annotations as a dict.
  def get_annotations(self):
    return self.annots

  # Returns true if this edge is annotated as specified by the given query. Any
  # annotation mentioned in the query must have the same value as in the query,
  # any additional annotations not mentioned are ignored.
  def has_annotations(self, query):
    for (key, value) in query.items():
      if not (self.annots.get(key, None) == value):
        return False
    return True


# A node that works as a stand-in for a set of other nodes. If you know you're
# going to be using the same set of nodes in a bunch of places creating a single
# group node to represent them is a convenient way to handle that.
class GroupNode(VirtualNode):

  # Adds a member to this group.
  def add_member(self, node):
    self.add_dependency(node)

  # A group node doesn't produce any actual output, it is resolved directly into
  # anything that depends on it.
  def get_output_target(self):
    return None

  def get_flat_edges_through(self, edge, annots):
    # If the edge we used to get here has the annotations then we consider them
    # to be satisfied and remove any restrictions on the following nodes.
    if edge.has_annotations(annots):
      annots = {}
    for target in self.get_flat_edges(**annots):
      yield target


# A different name for a group of nodes. Similar to a group except that a target
# is produces so the alias can be built independently of any physical targets.
class AliasNode(GroupNode):

  # Unlike a normal group an alias causes a target to be generated.
  def get_output_target(self):
    return self.get_name()


# A function marked to be exported into build scripts.
class ExportedFunction(object):

  def __init__(self, delegate):
    self.delegate = delegate

  # This gets called by python to produce a bound version of this function.
  def __get__(self, holder, type):
    # We just let the delegate bind itself since we don't need to be able to
    # recognize the bound method, only the function that produces it.
    return self.delegate.__get__(holder, type)


# Annotation used to identify which of ConfigContext's methods are exposed to
# build scripts as toplevel functions.
def export_to_build_scripts(fun):
  return ExportedFunction(fun)


# A context that handles loading an individual mkmk file. Toplevel functions in
# the mkmk becomes method calls on this object. The context is responsible for
# providing convenient utilities for the build scripts, either directly as
# toplevel functions or indirectly through the tool sets (like "c" and "toc")
# and for holding context information for a given mkmk file.
class ConfigContext(object):

  def __init__(self, env, home, full_name):
    self.env = env
    self.home = home
    self.full_name = full_name

  # Builds the environment dictionary containing all the toplevel functions in
  # the mkmk.
  def get_script_environment(self):
    # Create a copy of the tools environment provided by the shared env.
    result = dict(self.env.get_tools(self))
    for (name, value) in dict(self.__class__.__dict__).items():
      if isinstance(value, ExportedFunction):
        result[name] = getattr(self, name)
    return result

  # Includes the given mkmk into the set of dependencies for this build process.
  @export_to_build_scripts
  def include(self, *rel_mkmk_path):
    full_mkmk = self.home.get_child(*rel_mkmk_path)
    rel_parent_path = rel_mkmk_path[:-1]
    full_name = self.full_name.append(*rel_parent_path)
    mkmk_home = full_mkmk.get_parent()
    subcontext = ConfigContext(self.env, mkmk_home, full_name)
    subcontext.load(full_mkmk)

  # Returns a group node with the given name, creating it if it doesn't already
  # exist.
  @export_to_build_scripts
  def get_group(self, name):
    return self.get_or_create_node(name, GroupNode)

  # Returns a node representing a dependency defined outside this context. Note
  # that the external node must already exist, if it doesn't the import order
  # should be changed to make sure nodes are created in the order they're
  # needed. This means that you can't make circular dependencies which is a
  # problem we can solve if it ever becomes necessary.
  @export_to_build_scripts
  def get_external(self, *names):
    return self.env.get_node(Name.of(*names))

  # Returns a file object representing the root of the source tree, that is,
  # the folder that contains the root .mkmk file.
  @export_to_build_scripts
  def get_root(self):
    return self.env.get_root()

  # Returns a file object representing the root of the build output directory.
  @export_to_build_scripts
  def get_bindir(self):
    return self.env.get_bindir()

  # Returns the file object representing the file with the given path under the
  # current folder.
  @export_to_build_scripts
  def get_file(self, *file_path):
    return self.home.get_child(*file_path)

  # Returns a node representing a source file with the given name.
  @export_to_build_scripts
  def get_source_file(self, file_path):
    file = self.home.get_child(file_path)
    return self.get_or_create_node(file_path, FileNode, file)

  # Returns a node representing the output of running a custom command.
  @export_to_build_scripts
  def get_custom_exec_file(self, file_path):
    return self.get_or_create_node(file_path, CustomExecNode, file_path)

  # Returns a node representing the output of running a system command.
  @export_to_build_scripts
  def get_system_exec_file(self, file_path):
    return self.get_or_create_node(file_path, SystemExecNode, file_path)

  # Creates a source file that represents the given source file.
  @export_to_build_scripts
  def wrap_source_file(self, file):
    return FileNode(None, self, file)

  # Adds a toplevel make alias for the given node.
  @export_to_build_scripts
  def add_alias(self, name, *nodes):
    # Aliases have two names, one fully qualified and one just the basic name.
    alias = self.get_or_create_node(name, AliasNode)
    basic_name = Name.of(name)
    self.env.add_node(basic_name, alias)
    for node in nodes:
      alias.add_member(node)
    return alias

  # If a node with the given name already exists within this context returns it,
  # otherwise creates a new node by invoking the given class object with the
  # given arguments and registers the result under the given name.
  def get_or_create_node(self, name, Class, *args):
    node_name = self.get_full_name().append(name)
    return self.env.get_or_create_node(node_name, Class, self, *args)

  # Returns the platform object for this build.
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

  # Returns a file in the output directory with the given name and, optionally,
  # extension.
  def get_outdir_file(self, name, ext=None):
    if ext:
      full_out_name = self.get_full_name().append("%s.%s" %  (name, ext))
    else:
      full_out_name = self.get_full_name().append(name)
    return self.env.get_bindir().get_child(*full_out_name.get_parts())

  def __str__(self):
    return "Context(%s)" % self.home


# Abstract superclass of the tool sets loaded implicitly into each context.
class ToolSet(object):

  def __init__(self, context):
    self.context = context

  # Returns the context this tool set belongs to.
  def get_context(self):
    return self.context


# The static environment that is shared and constant between all contexts and
# across the lifetime of the build process. The environment is responsible for
# keeping track of nodes and dependencies, and for providing the context-
# independent functionality used by the contexts. Basically, the contexts expose
# the environment's functionality to the tools and build scripts in a convenient
# way, and the environment exposes the results of running those scripts to the
# output generator.
class Environment(object):

  def __init__(self, root, bindir, platform):
    self.root = root
    self.bindir = bindir
    self.platform = platform
    self.modules = None
    self.nodes = {}

  # If there is already a node registered under the given name returns it,
  # otherwise creates and registers a new one by calling the given constructor
  # with the given arguments.
  def get_or_create_node(self, full_name, Class, *args):
    if full_name in self.nodes:
      return self.nodes[full_name]
    new_node = Class(full_name.get_last_part(), *args)
    return self.add_node(full_name, new_node)

  def add_node(self, full_name, node):
    self.nodes[full_name] = node
    return node

  # Returns the node with the given full name, which must already exist.
  def get_node(self, full_name):
    return self.nodes[full_name]

  # Returns a handle to the root folder.
  def get_root(self):
    return self.root

  # Returns a handle to the binary output folder.
  def get_bindir(self):
    return self.bindir

  # Returns the platform object.
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

  # Writes the dependency graph in dot format to the given out stream.
  def write_dot_graph(self, out):
    # Escape a string such that it can be used in a dot file without breaking
    # the format.
    def dot_escape(s):
      return re.sub(r'\W', '_', s)
    # Convert an individual key/value edge annotation to a suitably concise
    # string.
    def annot_to_string(key, value):
      if value is True:
        return dot_escape(key)
      elif value is False:
        return "!%s" % dot_escape(key)
      else:
        return "%s: %s" % (dot_escape(key), dot_escape(value))
    # Convert a set of annotations to a string that can be used as a label.
    def annots_to_string(annots):
      return " ".join([annot_to_string(k, v) for (k, v) in annots.items()])
    out.write("digraph G {\n")
    out.write("  rankdir=LR;\n")
    for node in self.nodes.values():
      full_name = node.get_full_name()
      escaped = dot_escape(str(full_name))
      display_name = node.get_display_name()
      out.write("  %s [label=\"%s\"];\n" % (escaped, display_name))
      for edge in node.get_direct_edges():
        target = edge.get_target()
        target_name = target.get_full_name()
        escaped_target = dot_escape(str(target_name))
        label = ""
        annots = edge.get_annotations()
        if annots:
          label = " [label=\"%s\"]" % annots_to_string(annots)
        out.write("    %s -> %s%s;\n" % (escaped, escaped_target, label))
    out.write("}\n")

  # Writes the nodes loaded into this environment in Makefile syntax to the
  # given out stream.
  def write_makefile(self, out, bindir):
    makefile = Makefile()
    for node in self.nodes.values():
      output_target = node.get_output_target()
      if not output_target:
        # If the node has no output target there's nothing to do to generate it.
        continue
      all_edges = node.get_flat_edges()
      direct_input_files = [e.get_target().get_input_file() for e in all_edges]
      extra_input_files = node.get_computed_dependencies()
      input_files = direct_input_files + extra_input_files
      input_paths = [f.get_path() for f in input_files]
      commands = []
      output_file = node.get_output_file()
      # If there's a file to produce make sure the parent folder exists.
      if not output_file is None:
        output_parent = output_file.get_parent().get_path()
        mkdir_command = self.platform.get_ensure_folder_command(output_parent)
        commands += mkdir_command.get_actions(self)
      process_command = node.get_command_line(self.platform)
      if not process_command is None:
        commands += process_command.get_actions(self)
      makefile.add_target(output_target, input_paths, commands, node.is_phony())
    clean_command = self.platform.get_clear_folder_command(bindir.get_path())
    clean_actions = clean_command.get_actions(self)
    makefile.add_target("clean", [], clean_actions, True)
    makefile.write(out)

  # Lists all the tool modules and the names under which they should be exposed
  # to build scripts.
  @staticmethod
  def generate_tool_modules():
    from . import c
    from . import n
    from . import py
    from . import test
    from . import toc
    yield ('c', c)
    yield ('n', n)
    yield ('py', py)
    yield ('test', test)
    yield ('toc', toc)


# Ensures that the parent folder of the given path exists.
def ensure_parent(path):
  parent = os.path.dirname(path)
  if not os.path.exists(parent):
    os.makedirs(parent)


def main(root, bindir, config, dot=None, makefile=None, toolchain="gcc"):
  from . import platform
  root_mkmk = AbstractFile.at(root)
  # The path that contains the .mkmk file.
  root_mkmk_home = root_mkmk.get_parent()
  gcc = platform.get_toolchain(toolchain, config)
  bindir = AbstractFile.at(bindir)
  env = Environment(root_mkmk_home, bindir, gcc)
  context = ConfigContext(env, root_mkmk_home, Name.of())
  context.load(root_mkmk)
  if dot:
    env.write_dot_graph(open(dot, "wt"))
  if makefile:
    ensure_parent(makefile)
    env.write_makefile(open(makefile, "wt"), bindir)
  