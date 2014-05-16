#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Sets up a build environment. This file is copied among all the repos that use
## mkmk, the authoritative version lives in mkmk/tools. Don't change this
## directly.

set -e

if ! which mkmk > /dev/null; then
  if [ ! -d devenv ]; then
    if ! which virtualenv > /dev/null; then
      if ! which pip > /dev/null; then
        echo "Trying to install pip."
        sudo apt-get install python-pip
      fi
      echo "Trying to install virtualenv."
      sudo pip install virtualenv
    fi
    echo "Trying to create devenv."
    virtualenv devenv
  fi
  if [ ! -d mkmk ]; then
    echo "Trying to check out mkmk from github."
    git clone https://github.com/goto-10/mkmk.git mkmk
  fi
  echo "Trying to install python package."
  . devenv/bin/activate
  cd mkmk
  python setup.py develop
  cd ..
fi
