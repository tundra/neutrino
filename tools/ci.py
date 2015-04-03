#!/usr/bin/python


import argparse
import re
import shutil
import logging
import csv
import os.path
import platform
import subprocess


_TRAMPOLINE = "ci.py"
_FLAGS = "ci.csv"


# Would rather use a custom logger here but for some reason when I create one
# nothing of any kind gets logged. Classic python.
logging.basicConfig(level=logging.INFO)


class Ci(object):

  def __init__(self, argv):
    self.exe = argv[0]
    self.command = argv[1]
    self.argv = argv

  # Build the option parser to use when this script is called not as a
  # trampoline.
  def build_option_parser(self):
    parser = argparse.ArgumentParser()
    commands = sorted(self.get_handlers().keys())
    parser.add_argument('command', choices=commands)
    parser.add_argument('--config', default=None, help='The root configuration')
    parser.add_argument('--always-clean', default=False, action="store_true",
      help='Always clean before running')
    parser.add_argument('--sticky-init-flags', default="", action="store")
    return parser

  def parse_options(self):
    return self.build_option_parser().parse_args(self.argv[1:])

  # Returns a dict mapping command names to their handler methods (methods that
  # start with "handle_").
  def get_handlers(self):
    result = {}
    for (name, value) in Ci.__dict__.items():
      match = re.match(r'handle_(.*)', name)
      if match is None:
        continue
      action = match.group(1)
      result[action] = value
    return result

  # Performs the "begin" action: copies this file to the current directory (such
  # that it can be called through "python ci.py", the syntax of which is the
  # same across platforms) and creates ci.csv with the options passed to this
  # command.
  def handle_begin(self):
    options = self.parse_options()
    shutil.copyfile(self.exe, _TRAMPOLINE)
    tools = os.path.dirname(self.exe)
    with open(_FLAGS, "wb") as file:
      writer = csv.writer(file)
      writer.writerow(["config", options.config])
      writer.writerow(["always_clean", options.always_clean])
      writer.writerow(["sticky_init_flags", options.sticky_init_flags])
      writer.writerow(["tools", tools])
    # Run enter_devenv for good measure. We'll also be doing this when running
    # the individual steps but begin may be run with broader permissions (for
    # instance on travis) and may be allowed to do things that the run steps
    # will then just check have been done.
    self.sh(self.get_run_enter_devenv(tools))

  def get_flags(self):
    flags = {}
    with open(_FLAGS, "rb") as file:
      reader = csv.reader(file)
      for row in reader:
        flags[row[0]] = row[1]
    return flags

  def get_shell_script_name(self, base):
    if self.is_windows():
      return "%s.bat" % base
    else:
      return "%s.sh" % base

  def is_windows(self):
    return platform.system() == "Windows"

  def handle_run(self):
    flags = self.get_flags()
    rest = self.argv[2:]
    if "--" in rest:
      index = rest.index("--")
      init_flags = rest[:index]
      run_flags = rest[index+1:]
    else:
      init_flags = rest
      run_flags = []
    always_clean = flags["always_clean"] == "True"
    sticky_init_flags = flags["sticky_init_flags"]
    # On windows the $0 argument to mkmk gets messed up so we pass the command
    # explicitly. On linux it's fine automatically but there's no harm in
    # passing it anyway, that's one less special case.
    run_mkmk_init = ["mkmk", "init", "--config", flags["config"], "--self", "mkmk", sticky_init_flags] + init_flags
    build_script = "build.bat" if self.is_windows() else "./build.sh"
    run_clean_opt = [[build_script, "clean"]] if always_clean else []
    run_build = [build_script] + run_flags
    commands = (
      [self.get_run_enter_devenv(flags['tools'])] +
      [run_mkmk_init] +
      run_clean_opt +
      [run_build])
    self.sh(*commands)

  def sh(self, *command_lists):
    commands = [" ".join(c) for c in command_lists]
    command = " &&  ".join(commands)
    logging.info("Running [%s]", command)
    if self.is_windows():
      subprocess.check_call(command, shell=True)
    else:
      subprocess.check_call(["bash", "-c", command])

  # Returns the command to run to enter a devenv.
  def get_run_enter_devenv(self, tools):
    enter_devenv_base = os.path.join(tools, "enter-devenv")
    enter_devenv = self.get_shell_script_name(enter_devenv_base)
    if self.is_windows():
      return ["call", enter_devenv]
    else:
      return ["source", enter_devenv]


  def main(self):
    handlers = self.get_handlers()
    if not self.command in handlers:
      logging.error("Unknown command %s", self.command)
      sys.exit(1)
    handler = handlers[self.command]
    return handler(self)


if __name__ == '__main__':
  import sys
  Ci(sys.argv).main()
