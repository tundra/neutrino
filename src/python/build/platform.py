# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Code that controls how the build works on different platforms.


from . import process
from abc import ABCMeta, abstractmethod


# A toolchain is a set of tools used to build objects, executables, etc.
class Toolchain(object):
  __metaclass__ = ABCMeta

  # Returns the command for compiling a source file into an object.
  @abstractmethod
  def get_object_compile_command(self, output, inputs, includepaths):
    pass

    # Returns the file extension to use for generated object files.
  @abstractmethod
  def get_object_file_ext(self):
    pass

  # Returns the command for compiling a set of object files into an executable.
  @abstractmethod
  def get_executable_compile_command(self, output, inputs):
    pass

  # Returns the command for ensuring that the folder with the given name
  # exists.
  @abstractmethod
  def get_ensure_folder_command(self, folder):
    pass


# The gcc toolchain. Clang is gcc-compatible so this works for clang too.
class Gcc(Toolchain):

  def get_object_compile_command(self, output, inputs, includepaths):
    cflags = ["-std=c99"]
    for path in includepaths:
      cflags.append("-I%s" % process.shell_escape(path))
    command = "$(CC) %(cflags)s -c -o %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs)),
      "cflags": " ".join(cflags)
    }
    return [command]

  def get_object_file_ext(self):
    return "o"

  def get_executable_compile_command(self, output, inputs):
    command = "$(CC) -o %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    return [command]

  def get_ensure_folder_command(self, folder):
    return ["mkdir -p %s" % process.shell_escape(folder)]



# The Microsoft visual studio toolchain.
class MSVC(Toolchain):

  def get_object_compile_command(self, output, inputs, includepaths):
    command = "$(CC) /EHsc /Fo %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    return [command]

  def get_object_file_ext(self):
    return "obj"

  def get_executable_compile_command(self, output, inputs):
    # TODO: implement once object compilation works.
    return []

  def get_ensure_folder_command(self, folder):
    # Windows mkdir doesn't have an equivalent to -p but we can use a bit of
    # logic instead.
    path = process.shell_escape(folder)
    return ["if not exist %(path)s mkdir %(path)s" % {"path": path}]


# Returns the toolchain with the given name
def get_toolchain(name):
  if name == "gcc":
    return Gcc()
  elif name == "msvc":
    return MSVC()
  else:
    raise Exception("Unknown toolchain %s" % name)
