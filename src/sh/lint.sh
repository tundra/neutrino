#!/bin/sh
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Important: die as soon as any of the commands die.
set -e

# Choke on any tabs except in pyc files. I wish python wouldn't dump its
# garbage everywhere.
! (find src/ tests/ -type f | grep -v ".*\.pyc" | xargs grep -P '\t')

# Choke on HEST except in this file itself and log.h where it's defined.
! (find src/ tests/ -type f | grep -v -e lint.sh -e log.h | xargs grep HEST)
