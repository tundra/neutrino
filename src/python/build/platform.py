# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from . import process


class Toolchain(object):
  pass


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

  def get_executable_compile_command(self, output, inputs):
    command = "$(CC) -o %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    return [command]

  def get_ensure_folder_command(self, folder):
    return ["mkdir -p %s" % process.shell_escape(folder)]

  def get_object_file_ext(self):
    return "o"


class MSVC(Toolchain):

  def get_executable_compile_command(self, output, inputs):
    return []

  def get_object_compile_command(self, output, inputs, includepaths):
    command = "$(CC) /EHsc /Fo %(output)s %(inputs)s" % {
      "output": process.shell_escape(output),
      "inputs": " ".join(map(process.shell_escape, inputs))
    }
    return [command]

  def get_ensure_folder_command(self, folder):
    path = process.shell_escape(folder)
    return ["if not exist %(path)s mkdir -p %(path)s" % {"path": path}]

  def get_object_file_ext(self):
    return "obj"


# Returns the toolchain with the given name
def get_toolchain(name):
  if name == "gcc":
    return Gcc()
  elif name == "msvc":
    return MSVC()
  else:
    raise Exception("Unknown toolchain %s" % name)
