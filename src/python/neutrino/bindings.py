# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Namespace bindings.

import ast
import interp


# Visitor that traverses the syntax tree and resolves any past-references.
class PastVariableResolver(ast.Visitor):

  def __init__(self, unit):
    self.unit = unit

  def visit_variable(self, that):
    name = that.name
    if name.stage >= 0:
      # If the variable is not in the past there's nothing to do.
      return
    stage = self.unit.get_or_create_stage(name.stage)
    value = stage.namespace.lookup(name.path)
    that.resolved_past_value = ast.Literal(value)


# Utility that can be used by elements to help apply themselves.
class BindingHelper(object):

  def evaluate(self, expr):
    return interp.evaluate(expr)


def resolve_past_variables(index, elements, unit):
  resolver = PastVariableResolver(unit)
  for element in elements:
    element.accept(resolver)


# Apply the various declarations to create the program bindings. This is only
# an approximation of the proper program element semantics but it'll do for
# now, possibly until the source compiler is rewritten in neutrino.
def bind(unit):
  helper = BindingHelper()
  for (index, stage) in unit.get_stages():
    resolve_past_variables(index, stage.elements, unit)
    for element in stage.elements:
      element.apply(stage, helper)
  resolve_past_variables(0, [unit.entry_point], unit)
