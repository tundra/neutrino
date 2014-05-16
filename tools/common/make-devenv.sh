#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Sets up a build environment.

set -v -e

sudo apt-get install python-pip

sudo pip install virtualenv

virtualenv devenv

ensure-mkmk.sh
