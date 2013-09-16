# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Syntax tree analysis: scopes, etc.

import ast

# A bottom scope that handles variables that don't have a local binding.
class BottomScope(object):

  def lookup(self, name):
    return None


# A simple scope that contains a map from names to symbols, when you look up a
# name that's in the map you'll get back they associated symbol.
class MappedScope(object):

  def __init__(self, bindings, parent):
    self.parent = parent
    self.bindings = bindings

  def lookup(self, name):
    value = self.bindings.get(name, None)
    if value is None:
      return self.parent.lookup(name)
    else:
      return value


class ScopeVisitor(ast.Visitor):

  def __init__(self):
    super(ScopeVisitor, self).__init__()
    self.scope = BottomScope()

  def lookup_name(self, name):
    return self.scope.lookup(name)

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
    bindings = {}
    bindings[that.name] = that.symbol
    outer_scope = self.scope
    self.scope = MappedScope(bindings, outer_scope)
    try:
      that.body.accept(self)
    finally:
      self.scope = outer_scope

  def visit_invocation(self, that):
    for arg in that.arguments:
      arg.accept(self)

  def visit_variable(self, that):
    assert that.symbol is None
    that.symbol = self.lookup_name(that.name)

  def visit_argument(self, that):
    that.value.accept(self)

  def visit_lambda(self, that):
    bindings = {}
    for param in that.parameters:
      param.symbol = ast.Symbol(param.name)
      bindings[param.name] = param.symbol
    outer_scope = self.scope
    self.scope = MappedScope(bindings, outer_scope)
    try:
      that.body.accept(self)
    finally:
      self.scope = outer_scope

  def visit_program(self, that):
    that.entry_point.accept(self)

  def visit_namespace_declaration(self, that):
    that.value.accept(self)


def analyze(ast):
  visitor = ScopeVisitor()
  ast.accept(visitor)
