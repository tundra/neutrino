# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Namespace bindings.

import ast
import interp


# Visitor that traverses the syntax tree and resolves any past-references.
class PastVariableResolver(ast.Visitor):

  def __init__(self, unit):
    self.unit = unit

  def visit_quote(self, that):
    that.ast.accept(self)
    if that.stage >= 0:
      # If the unquote is not in the past there's nothing to do.
      return
    value = interp.evaluate(that.ast)
    that.value = value


# Utility that can be used by elements to help apply themselves.
class BindingHelper(object):

  def evaluate(self, expr):
    return interp.evaluate(expr)


# Apply the various declarations to create the program bindings. This is only
# an approximation of the proper program element semantics but it'll do for
# now, possibly until the source compiler is rewritten in neutrino.
def bind(unit):
  helper = BindingHelper()
  resolver = PastVariableResolver(unit)
  for (index, stage) in unit.get_stages():
    for element in stage.elements:
      # Resolve expressions as we go so that earlier bindings (lexically) are
      # visible to later bindings.
      element.accept(resolver)
      element.apply(stage, helper)
  unit.entry_point.accept(resolver)
