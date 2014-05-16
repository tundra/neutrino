#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Ensures that there's an mkmk command available, installing it from git if
## necessary.

set -e

if ! which mkmk > /dev/null; then
  if [ ! -d mkmk ]; then
    echo "Trying to check out mkmk from github."
    git clone https://github.com/goto-10/mkmk.git mkmk
  fi
  echo "Trying to install python package."
  cd mkmk
  python setup.py develop
fi
