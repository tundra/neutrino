# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Syntax tree analysis: scopes, etc.

import ast

class ScopeVisitor(ast.Visitor):

  def __init__(self):
    super(ScopeVisitor, self).__init__()
    self.scope_stack = []

  def lookup_name(self, name):
    for (sym_name, sym) in reversed(self.scope_stack):
      if name == sym_name:
        return sym

  def visit_array(self, that):
    for element in that.elements:
      element.accept(self)

  def visit_sequence(self, that):
    for value in that.values:
      value.accept(self)

  def visit_local_declaration(self, that):
    assert that.symbol is None
    that.value.accept(self)
    that.symbol = ast.Symbol(that.name)
    self.scope_stack.append((that.name, that.symbol))
    try:
      that.body.accept(self)
    finally:
      self.scope_stack.pop()

  def visit_invocation(self, that):
    for arg in that.arguments:
      arg.accept(self)

  def visit_variable(self, that):
    assert that.symbol is None
    symbol = self.lookup_name(that.name)
    assert not symbol is None
    that.symbol = symbol

  def visit_argument(self, that):
    that.value.accept(self)


def analyze(ast):
  visitor = ScopeVisitor()
  ast.accept(visitor)
