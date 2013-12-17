# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Code that controls how the build works on different platforms.


from . import process
from abc import ABCMeta, abstractmethod
import os.path


# A toolchain is a set of tools used to build objects, executables, etc.
class Toolchain(object):
  __metaclass__ = ABCMeta

  def __init__(self, config):
    self.config = config

  def get_config_option(self, name):
    return self.config.get(name, None)

  # Look ma, gcc and msvc are sharing code!
  def get_print_env_command(self):
    command = "echo CFLAGS: %s" % (" ".join(self.get_config_flags()))
    return process.Command(command)

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

  # Returns the command that runs the given command line, printing the output
  # and also storing it in the given outpath, but deletes the outpath again and
  # fails if the command line fails. Sort of how you wish tee could work.
  @abstractmethod
  def get_safe_tee_command(self, command_line, outpath):
    pass


# The gcc toolchain. Clang is gcc-compatible so this works for clang too.
class Gcc(Toolchain):

  def get_config_flags(self):
    result = [
      "-Wall",
      "-Wextra",                    # More errors please.
      "-Wno-unused-parameter",      # Sometime you don't need all the params.
      "-Wno-unused-function",       # Not all header functions are used in all.
                                    #   the files that include them.
      "-Wno-unused-local-typedefs", # Some macros generate unused typedefs.
      "-std=c99",
    ]
    # Debug flags
    if self.get_config_option("debug"):
      result += ["-O0", "-g"]
    else:
      result += ["-O3"]
    # Checks en/dis-abled
    if self.get_config_option("checks"):
      result += ["-DENABLE_CHECKS=1"]
    # Strict errors
    if not self.get_config_option("warn"):
      result += ["-Werror"]
    return result

  def get_object_compile_command(self, output, inputs, includepaths):
    cflags = ["$(CFLAGS)"] + self.get_config_flags()
    for path in includepaths:
      cflags.append("-I%s" % process.shell_escape(path))
    command = "$(CC) %(cflags)s -c -o %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs)),
      "cflags": " ".join(cflags)
    }
    comment = "Building %s" % os.path.basename(output)
    return process.Command(command).set_comment(comment)

  def get_object_file_ext(self):
    return "o"

  def get_executable_compile_command(self, output, inputs):
    command = "$(CC) -o %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    comment = "Building executable %s" % os.path.basename(output)
    return process.Command(command).set_comment(comment)

  def get_executable_file_ext(self):
    return None

  def get_ensure_folder_command(self, folder):
    command = "mkdir -p %s" % process.shell_escape(folder)
    return process.Command(command)

  def get_clear_folder_command(self, folder):
    command = "rm -rf %s" % (process.shell_escape(folder))
    comment = "Clearing '%s'" % folder
    return process.Command(command).set_comment(comment)

  def get_safe_tee_command(self, command_line, outpath):
    params = {
      "command_line": command_line,
      "outpath": outpath
    }
    parts = [
      "%(command_line)s > %(outpath)s || echo > %(outpath)s.fail",
      "cat %(outpath)s",
      "if [ -f %(outpath)s.fail ]; then rm %(outpath)s %(outpath)s.fail; false; else true; fi",
    ]
    comment = "Running %(command_line)s" % params
    return process.Command(*[part % params for part in parts]).set_comment(comment)


# The Microsoft visual studio toolchain.
class MSVC(Toolchain):

  def get_config_flags(self):
    result = [
      "/nologo",
      "/Wall",
      "/wd4505", # Unreferenced local function
      "/wd4514", # Unreferenced inline function
      "/wd4127", # Conditional expression is a constant
      "/wd4820", # Padding added after data member
      "/wd4800", # Forcing value to bool
      "/wd4061", # Enum not explicitly handled by case
      "/wd4365", # Conversion, signed/unsigned mismatch
      "/wd4510", # Default constructor could not be generated
      "/wd4512", # Assignment operator could not be generated
      "/wd4610", # Struct can never be instantiated
      "/wd4245", # Conversion, signed/unsigned mismatch
      "/wd4100", # Unreferenced formal parameter
      "/wd4702", # Unreachable code
      "/wd4711", # Function selected for inline expansion
      "/wd4735", # Storing 64-bit float result in memory
      "/wd4710", # Function not inlined
      "/wd4738", # Storing 32-bit float result in memory

      # Maybe look into fixing these?
      "/wd4244", # Possibly lossy conversion from int64 to int32
      "/wd4242", # Possibly lossy conversion from int32 to int8 
      "/wd4146", # Unary minus applied to unsigned
      "/wd4996", # Function may be unsafe
      "/wd4826", # Conversion is sign-extended
      "/wd4310", # Cast truncates constant
    ]
    # Debug flags
    if self.get_config_option("debug"):
      result += ["/Od", "/Zi"]
    else:
      result += ["/Ox"]
    # Checks en/dis-abled
    if self.get_config_option("checks"):
      result += ["/DENABLE_CHECKS=1"]
    # Strict errors
    if not self.get_config_option("warn"):
      result += ["/WX"]
    return result

  def get_object_compile_command(self, output, inputs, includepaths):
    def build_source_argument(path):
      return "/Tp%s" % process.shell_escape(path)
    cflags = ["/c"] + self.get_config_flags()
    for path in includepaths:
      cflags.append("/I%s" % process.shell_escape(path))
    command = "$(CC) %(cflags)s /Fo%(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(build_source_argument, inputs)),
      "cflags": " ".join(cflags)
    }
    comment = "Building %s" % os.path.basename(output)
    return process.Command(command).set_comment(comment)

  def get_object_file_ext(self):
    return "obj"

  def get_executable_compile_command(self, output, inputs):
    command = "$(CC) /nologo /Fe%(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    comment = "Building executable %s" % os.path.basename(output)
    return process.Command(command).set_comment(comment)

  def get_executable_file_ext(self):
    return "exe"

  def get_ensure_folder_command(self, folder):
    # Windows mkdir doesn't have an equivalent to -p but we can use a bit of
    # logic instead.
    path = process.shell_escape(folder)
    action = "if not exist %(path)s mkdir %(path)s" % {"path": path}
    return process.Command(action)

  def get_clear_folder_command(self, folder):
    path = process.shell_escape(folder)
    comment = "Clearing '%s'" % path
    action = "if exist %(path)s rmdir /s /q %(path)s" % {"path": path}
    return process.Command(action).set_comment(comment)

  def get_safe_tee_command(self, command_line, outpath):
    params = {
      "command_line": command_line,
      "outpath": outpath
    }
    parts = [
      "%(command_line)s > %(outpath)s || echo > %(outpath)s.fail",
      "type %(outpath)s",
      "if exist %(outpath)s.fail (del %(outpath)s %(outpath)s.fail && exit 1) else (exit 0)",
    ]
    comment = "Running %(command_line)s" % params
    return process.Command(*[part % params for part in parts]).set_comment(comment)


# Returns the toolchain with the given name
def get_toolchain(name, config):
  if name == "gcc":
    return Gcc(config)
  elif name == "msvc":
    return MSVC(config)
  else:
    raise Exception("Unknown toolchain %s" % name)
