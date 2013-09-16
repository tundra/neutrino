# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Namespace bindings.

import ast

def bind(program):
  program.namespace.add_binding(("x",), 4)
