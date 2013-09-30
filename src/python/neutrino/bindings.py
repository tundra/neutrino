# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Namespace bindings.

import ast
import interp


# Utility that can be used by elements to help apply themselves.
class BindingHelper(object):

  def evaluate(self, expr):
  	return interp.evaluate(expr)


# Apply the various declarations to create the program bindings. This is only
# an approximation of the proper program element semantics but it'll do for
# now, possibly until the source compiler is rewritten in neutrino.
def bind(program):
  helper = BindingHelper()
  for element in program.elements:
    element.apply(program, helper)
