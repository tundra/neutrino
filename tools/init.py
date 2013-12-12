#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import sys


# Parse command-line options, returning just the flags and discarding the args.
def parse_options(argv):
  import optparse
  parser = optparse.OptionParser()
  parser.add_option('--bindir')
  parser.add_option('--toolchain', default='gcc')
  parser.add_option('--language', default='sh')
  parser.add_option('--has_changed', default=None, )
  (flags, args) = parser.parse_args(argv)
  return flags


SH_TEMPLATE = """\
#!/bin/sh

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
  exit 0
fi


# Rebuild makefile every time.
%(mkmk_tool)s --root "%(root_mkmk)s" --bindir "%(bindir)s" --makefile "%(Makefile.mkmk)s"

# Delegate to the resulting makefile.
make -f Makefile.mkmk $*
"""


BAT_TEMPLATE = """\
@echo off

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
  exit %%errorlevel%%
)

python %(mkmk_tool)s --root "%(root_mkmk)s" --bindir "%(bindir)s" --makefile "%(Makefile.mkmk)s" --toolchain msvc
if %%errorlevel%% neq 0 exit /b %%errorlevel%%

nmake /nologo -f Makefile.mkmk %%*
if %%errorlevel%% neq 0 exit /b %%errorlevel%%
"""


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


# Generates a build script of the appropriate type.
def generate_build_script(flags):
  import os
  import os.path
  workspace_root = os.getcwd()
  script_path = __file__
  neutrino_root = os.path.dirname(os.path.dirname(script_path))
  root_mkmk = os.path.join(neutrino_root, "neutrino.mkmk")
  if flags.language == 'sh':
    filename = os.path.join(workspace_root, "build.sh")
    template = SH_TEMPLATE
  else:
    filename = os.path.join(workspace_root, "build.bat")
    template = BAT_TEMPLATE
  init_md5 = get_self_md5()
  makefile_src = template % {
    "init_md5": init_md5,
    "init_tool": __file__,
    "init_args": " ".join(sys.argv[1:]),
    "mkmk_tool": os.path.join(neutrino_root, "tools", "mkmk"),
    "root_mkmk": root_mkmk,
    "bindir": flags.bindir,
    "Makefile.mkmk": "Makefile.mkmk"
  }
  with open(filename, "wt") as out:
    out.write(makefile_src)


def main():
  flags = parse_options(sys.argv[1:])
  if not flags.has_changed is None:
    run_change_check(flags.has_changed)
  else:
    generate_build_script(flags)


if __name__ == "__main__":
  main()
