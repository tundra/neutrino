# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Simple neutrino interpreter used to evaluate past code. This is not
# intended to be complete, just good enough to evaluate the stuff we need for
# testing and bootstrapping.

import ast
import data
import operator
import types


class EvaluateError(Exception):

  def __init__(self, expr):
    self.expr = expr


# Simple visitor that can fake-evaluate simple expressions.
class EvaluateVisitor(ast.Visitor):

  def visit_ast(self, that):
    raise EvaluateError(that)

  def visit_literal(self, that):
    return that.value

  def visit_variable(self, that):
    current = that.namespace
    for part in that.ident.get_path_parts():
      current = current.lookup(part)
    return current

  # Map from neutrino operator names to the python implementations
  OPERATOR_MAP = {
    data.Operation.infix("+"): operator.add,
    data.Operation.infix("-"): operator.sub,
    data.Operation.infix("*"): operator.mul,
    data.Operation.infix("/"): operator.div
  }

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
        value = arg.value.accept(self)
        args.append(value)
    if isinstance(subject, types.FunctionType):
      assert selector == data.Operation.call()
      return subject(*args)
    elif selector in EvaluateVisitor.OPERATOR_MAP:
      # This bleeds python semantics into how the operators work. Don't (don't!)
      # depend on anything that's not certain to also work in neutrino.
      op = EvaluateVisitor.OPERATOR_MAP[selector]
      return op(subject, *args)
    else:
      assert False

  def visit_quote(self, that):
    return that.value

# Creates a new empty neutrino object.
def new_neutrino_instance(protocol):
  return data.Instance(protocol)

# Creates a new neutrino protocol.
def new_neutrino_protocol(display_name):
  return data.Protocol(display_name)

# Returns a reference to the given built-in protocol.
def get_neutrino_builtin_protocol(name):
  return data.Key(name, ("protocol", name))

class PytrinoProxy(object):

  def lookup(self, name):
    if name == 'new_instance':
      return new_neutrino_instance
    elif name == 'new_protocol':
      return new_neutrino_protocol
    elif name == 'get_builtin_protocol':
      return get_neutrino_builtin_protocol

_PYTRINO = PytrinoProxy()
_CTRINO = data.Key("ctrino", ("core", "ctrino"))

# Looks up a name that is magically available when doing static evaluation. It's
# a hack until we get imports working properly.
def lookup_special_binding(name):
  if name == 'pytrino':
    return _PYTRINO
  elif name == 'ctrino':
    return _CTRINO

# Evaluate the given expression. The evaluation will only be approximate so
# it may fail if there are constructs or operations it can't handle.
def evaluate(expr):
  visitor = EvaluateVisitor()
  return expr.accept(visitor)
