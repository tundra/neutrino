# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Simple neutrino interpreter used to evaluate past code. This is not
# intended to be complete, just good enough to evaluate the stuff we need for
# testing and bootstrapping.

import ast

# Simple visitor that can fake-evaluate simple expressions.
class EvaluateVisitor(ast.Visitor):

  def visit_literal(self, that):
    return that.value

  def visit_variable(self, that):
    assert that.resolved_past_value
    return that.resolved_past_value.accept(self)


# Evaluate the given expression. The evaluation will only be approximate so
# it may fail if there are constructs or operations it can't handle.
def evaluate(expr):
  visitor = EvaluateVisitor()
  return expr.accept(visitor)
