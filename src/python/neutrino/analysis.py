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

  def __init__(self, unit):
    super(ScopeVisitor, self).__init__()
    self.scope = BottomScope()
    self.unit = unit

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
    self.visit_generic_declaration(that)

  def visit_local_lambda(self, that):
    assert that.symbol is None
    that.method.accept(self)
    self.visit_generic_declaration(that)

  def visit_with_escape(self, that):
    assert that.symbol is None
    self.visit_generic_declaration(that)

  # Analyzes the body of a declaration-like syntax tree.
  def visit_generic_declaration(self, that):
    that.symbol = ast.Symbol(that.get_name(), that)
    bindings = {}
    bindings[that.get_name()] = that.symbol
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
    path = that.ident.path
    if path.is_singular():
      that.symbol = self.lookup_name(path.get_head())

  def visit_static_binding(self, that):
    # Static bindings cannot refer to locally scoped bindings. Maybe they should
    # be able to? Well, not yet.
    pass

  def visit_argument(self, that):
    that.value.accept(self)

  def visit_method(self, that):
    bindings = {}
    for param in that.signature.parameters:
      bindings[param.get_name()] = param.get_symbol()
    outer_scope = self.scope
    self.scope = MappedScope(bindings, outer_scope)
    try:
      that.body.accept(self)
    finally:
      self.scope = outer_scope

  def visit_unit(self, that):
    for (index, stage) in that.get_stages():
      for element in stage.elements:
        element.accept(self)
    if not that.entry_point is None:
      that.entry_point.accept(self)

  def visit_namespace_declaration(self, that):
    that.value.accept(self)

  def visit_import(self, that):
    ident = that.ident
    stage = self.unit.get_or_create_stage(ident.stage)
    stage.add_import(ident.path, ident.path)


# Do scope analysis, bind variables to their enclosing definitions and register
# imports appropriately.
def scope_analyze(unit):
  visitor = ScopeVisitor(unit)
  unit.accept(visitor)
