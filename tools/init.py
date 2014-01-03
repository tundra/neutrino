#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import platform
import sys


# Flags to pass through to mkmk when regenerating the makefile.
_MKMK_FLAGS = [
  ('debug', False, 'store_true'),
  ('nochecks', False, 'store_true'),
  ('warn', False, 'store_true'),
  ('noisy', False, 'store_true'),
]


# Returns the default value to use for the language.
def get_default_shell():
  system = platform.system()
  if system == "Linux":
    return "sh"
  elif system == "Windows":
    return "bat"
  else:
    return None


_SH_TEMPLATE = """\
#!/bin/sh
# This file was generated by the init tool invoked as
#
#     %(init_tool)s %(init_args)s
#
# You generally don't want to edit it by hand. Instead, either change the init 
# tool or copy it to another name and then edit that to avoid your changes being
# randomly overwritten.

# Bail out on the first error.
set -e

# MD5 code of the init script that was used to generate this script.
INIT_MD5="%(init_md5)s"

# Check whether init.py has changed. If it has regenerate this file and run it
# again.
INIT_CHANGED=$(%(init_tool)s --has_changed $INIT_MD5)
if [ $INIT_CHANGED = "Changed" ]; then
  echo \# The workspace initializer has changed. Reinitializing by running
  echo \#
  echo \#   %(init_tool)s %(init_args)s
  echo \#
  %(init_tool)s %(init_args)s
  echo \# Build script regenerated. Running the new script like so:
  echo \#
  echo \#   "$0" "$*"
  echo \#
  "$0" "$*"
  exit $?
fi

# Rebuild makefile every time.
%(mkmk_tool)s --root "%(root_mkmk)s" --bindir "%(bindir)s" --makefile "%(Makefile.mkmk)s" %(variant_flags)s

# Delegate to the resulting makefile.
make -f "%(Makefile.mkmk)s" $*
"""


_BAT_TEMPLATE = """\
@echo off

REM This file was generated by the init tool invoked as
REM
REM     %(init_tool)s %(init_args)s
REM
REM You generally don't want to edit it by hand. Instead, either change the init 
REM tool or copy it to another name and then edit that to avoid your changes
REM being randomly overwritten.

for /f "tokens=*" %%%%a in (
  'python %(init_tool)s --has_changed "%(init_md5)s"'
) do (
  set init_changed=%%%%a
)

if "%%init_changed%%" == "Changed" (
  echo # The workspace initializer has changed. Reinitializing by running
  echo #
  echo #   python %(init_tool)s %(init_args)s
  python %(init_tool)s %(init_args)s
  if %%errorlevel%% neq 0 exit /b %%errorlevel%%
  echo #
  echo # Build script regenerated. Running the new script like so:
  echo #
  echo #   %%0 %%*
  call %%0 %%*
) else (
  python %(mkmk_tool)s --root "%(root_mkmk)s" --bindir "%(bindir)s" --makefile "%(Makefile.mkmk)s" --toolchain msvc %(variant_flags)s
  if %%errorlevel%% neq 0 exit /b %%errorlevel%%

  nmake /nologo -f "%(Makefile.mkmk)s" %%*
  if %%errorlevel%% neq 0 exit /b %%errorlevel%%
)
"""


# Map from shell names to the data used to generate their build scripts.
_SHELLS = {
  "sh": (_SH_TEMPLATE, "%s.sh"),
  "bat": (_BAT_TEMPLATE, "%s.bat")
}


# Parse command-line options, returning just the flags and discarding the args.
def parse_options(argv):
  import optparse
  parser = optparse.OptionParser()
  parser.add_option('--bindir', default='out')
  parser.add_option('--shell', default=get_default_shell())
  parser.add_option('--script', default=None)
  parser.add_option('--root', default=None)
  parser.add_option('--has_changed', default=None)
  for (name, default, action) in _MKMK_FLAGS:
    parser.add_option('--%s' % name, default=default, action=action)
  (flags, args) = parser.parse_args(argv)
  return flags


# Returns a string in base64 containing the md5 hash of this script.
def get_self_md5():
  import hashlib
  import base64
  md5 = hashlib.md5()
  with open(__file__, "rt") as script_file:
    for line in script_file:
      md5.update(line.encode("utf8"))
  result = base64.b64encode(md5.digest())
  return result.decode("ascii")


# Checks whether the contents of this script still MD5-hashes to the given
# value.
def run_change_check(old_md5):
  new_md5 = get_self_md5()
  if new_md5 == old_md5:
    print("Same")
  else:
    print("Changed")


# Checks that the flags are sane, otherwise bails.
def validate_flags(flags):
  if flags.shell is None:
    raise Exception("Couldn't determine which shell to use; use --shell.")
  if not flags.shell in _SHELLS:
    options = sorted(_SHELLS.keys())
    raise Exception("Unknown shell %s; possible values are %s." % (flags.shell, options))


# Returns the neutrino root folder path.
def get_neutrino_root(flags):
  if flags.root:
    # An explicit root is given so we just use that.
    return flags.root
  else:
    # Try to infer the root from the script path.
    import os
    import os.path
    script_path = __file__
    script_neutrino_root = os.path.dirname(os.path.dirname(script_path))
    if not (":" in script_neutrino_root):
      # We've got a path we can use; return it.
      return script_neutrino_root
    else:
      # We have a candidate but it contains a colon which will make 'make'
      # unhappy. This will most likely be from windows (as in "C:\...") so try
      # to remove it by using a relative path instead.
      cwd = os.getcwd()
      prefix = os.path.commonprefix([cwd, script_neutrino_root])
      return script_neutrino_root[len(prefix)+1:]


# Returns the name of the build script to generate.
def get_script_name(flags):
  if flags.script:
    return flags.script
  else:
    # No script is explicitly passed so default to "build" with the correct
    # extension in the current directory.
    import os
    import os.path
    workspace_root = os.getcwd()
    shell = _SHELLS[flags.shell]
    return os.path.join(workspace_root, shell[1] % "build")


# Returns the name of the generated makefile.
def get_makefile_name(flags):
  import os.path
  return os.path.join(flags.bindir, "Makefile.mkmk")


# Generates a build script of the appropriate type.
def generate_build_script(flags):
  validate_flags(flags)
  import os
  import os.path
  neutrino_root = get_neutrino_root(flags)
  root_mkmk = os.path.join(neutrino_root, "neutrino.mkmk")
  if not os.path.exists(root_mkmk):
    raise Exception("Couldn't find the root build script as %s" % root_mkmk)
  filename = get_script_name(flags)
  shell = _SHELLS[flags.shell]
  template = shell[0]
  variant_flags = []
  # Copy the mkmk flags over so they can be passed from the build script.
  for (name, default, action) in _MKMK_FLAGS:
    if (default == False) and (action == 'store_true'):
      if getattr(flags, name):
        variant_flags.append('--%s' % name)
    else:
      raise Exception("Don't know how to handle mkmk flag --%s" % name)
  init_md5 = get_self_md5()
  makefile_src = template % {
    "init_md5": init_md5,
    "init_tool": __file__,
    "init_args": " ".join(sys.argv[1:]),
    "mkmk_tool": os.path.join(neutrino_root, "tools", "mkmk"),
    "root_mkmk": root_mkmk,
    "bindir": flags.bindir,
    "Makefile.mkmk": get_makefile_name(flags),
    "variant_flags": " ".join(variant_flags),
  }
  with open(filename, "wt") as out:
    out.write(makefile_src)
  # Make the build script executable
  import stat
  st = os.stat(filename)
  os.chmod(filename, st.st_mode | stat.S_IEXEC)


def main():
  flags = parse_options(sys.argv[1:])
  if not flags.has_changed is None:
    run_change_check(flags.has_changed)
  else:
    generate_build_script(flags)


if __name__ == "__main__":
  main()
