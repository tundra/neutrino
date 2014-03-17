#!/bin/sh

# Installs build dependencies. Generally you'll want to sudo-run this.

set -e

# Determine the absolute path to the neutrino directory.
ROOT_REL=$(dirname $0)/../
ROOT_ABS=$(readlink -e "$ROOT_REL")

# Install plankton
$(cd $ROOT_ABS/deps/plankton/src/python && python setup.py install)
