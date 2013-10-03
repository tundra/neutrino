# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Simple neutrino interpreter used to evaluate past code. This is not
# intended to be complete, just good enough to evaluate the stuff we need for
# testing and bootstrapping.

import ast
import data
import types

# Simple visitor that can fake-evaluate simple expressions.
class EvaluateVisitor(ast.Visitor):

  def visit_literal(self, that):
    return that.value

  def visit_variable(self, that):
    assert not that.resolved_past_value is None
    return that.resolved_past_value.accept(self)

  def visit_invocation(self, that):
    subject = None
    selector = None
    args = []
    for arg in that.arguments:
      if arg.tag == data._SUBJECT:
        subject = arg.value.accept(self)
      elif arg.tag == data._SELECTOR:
        selector = arg.value.accept(self)
      else:
        assert False
    assert type(subject) == types.FunctionType
    assert selector == "()"
    return subject(*args)

# Creates a new empty neutrino object.
def new_neutrino_instance():
  return data.Instance()

# Looks up a name that is magically available when doing static evaluation. It's
# a hack until we get imports working properly.
def lookup_special_binding(name):
  if name == ('pytrino', 'new_instance'):
    return new_neutrino_instance
  else:
    return None

# Evaluate the given expression. The evaluation will only be approximate so
# it may fail if there are constructs or operations it can't handle.
def evaluate(expr):
  visitor = EvaluateVisitor()
  return expr.accept(visitor)
