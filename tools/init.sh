#!/bin/sh
# Copyright 2014 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

## Neutrino workspace initializer script

set -e

ROOT_REL=$(dirname $(dirname $0))
mkmk init --config $ROOT_REL/neutrino.mkmk $*
