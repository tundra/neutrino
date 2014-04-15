#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Installs any dependencies used when building on travis-ci.

set -v -e

sudo apt-get install python-pip

sudo pip install virtualenv

virtualenv neudev
