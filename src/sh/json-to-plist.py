#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Does what it says on the package really.

import json
import plistlib
import sys

in_file = sys.argv[1]
out_file = sys.argv[2]

data = json.load(open(in_file, "rt"))
plistlib.writePlist(data, out_file)
