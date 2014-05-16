#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Sets up a build environment.

set -e

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

. devenv/bin/activate

$(dirname $0)/ensure-mkmk.sh
