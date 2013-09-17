# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Neutrino syntax tree definitions

from abc import abstractmethod
import plankton

# Syntax tree visitor.
class Visitor(object):

  def __init__(self):
    pass

  @abstractmethod
  def visit_literal(self, that):
    pass

  @abstractmethod
  def visit_array(self, that):
    pass

  @abstractmethod
  def visit_variable(self, that):
    pass

  @abstractmethod
  def visit_invocation(self, that):
    pass

  @abstractmethod
  def visit_argument(self, that):
    pass

  @abstractmethod
  def visit_sequence(self, that):
    pass

  @abstractmethod
  def visit_local_declaration(self, that):
    pass


# A constant literal value.
@plankton.serializable(("ast", "Literal"))
class Literal(object):

  @plankton.field("value")
  def __init__(self, value=None):
    self.value = value

  def accept(self, visitor):
    return visitor.visit_literal(self);

  def __str__(self):
    return "#<literal: %s>" % self.value


# An array expression.
@plankton.serializable(("ast", "Array"))
class Array(object):

  @plankton.field("elements")
  def __init__(self, elements=None):
    self.elements = elements

  def accept(self, visitor):
    return visitor.visit_array(self);

  def __str__(self):
    return "#<array: %s>" % map(str, self.elements)


# A reference to an enclosing binding. The name is used before the variable
# has been resolved, the symbol after.
@plankton.virtual
@plankton.serializable(("ast", "LocalVariable"), ("ast", "NamespaceVariable"))
class Variable(object):

  _LOCAL_HEADER = plankton.EnvironmentPlaceholder(("ast", "LocalVariable"))
  _NAMESPACE_HEADER = plankton.EnvironmentPlaceholder(("ast", "NamespaceVariable"))

  # We don't need to serialize the name since the symbol holds the name.
  @plankton.field("symbol")
  def __init__(self, name=None, namespace=None, symbol=None):
    self.name = name
    self.namespace = namespace
    self.symbol = symbol

  def accept(self, visitor):
    return visitor.visit_variable(self)

  def get_header(self):
    if self.symbol is None:
      return Variable._NAMESPACE_HEADER
    else:
      return Variable._LOCAL_HEADER

  def get_payload(self):
    if self.symbol is None:
      return {
        'name': self.name.path,
        'namespace': self.namespace
      }
    else:
      return {'symbol': self.symbol}

  def __str__(self):
    return "#<var %s>" % str(self.name)


# A multi-method invocation.
@plankton.serializable(("ast", "Invocation"))
class Invocation(object):

  @plankton.field("arguments")
  @plankton.field("methodspace")
  def __init__(self, arguments=None, methodspace=None):
    self.arguments = arguments
    self.methodspace = methodspace

  def accept(self, visitor):
    return visitor.visit_invocation(self);

  def __str__(self):
    return "(! %s)" % " ".join(map(str, self.arguments))


# An individual argument to an invocation.
@plankton.serializable(("ast", "Argument"))
class Argument(object):

  @plankton.field("tag")
  @plankton.field("value")
  def __init__(self, tag=None, value=None):
    self.tag = tag
    self.value = value

  def accept(self, visitor):
    return visitor.visit_argument(self)

  def __str__(self):
    return "(: %s %s)" % (self.tag, self.value)


# A binding from a symbol to a value.
@plankton.serializable()
class Binding(object):

  @plankton.field("symbol")
  @plankton.field("value")
  def __init__(self, symbol=None, value=None):
    self.symbol = symbol
    self.value = value


# A sequence of expressions to execute in order, yielding the value of the last
# expression.
@plankton.serializable(("ast", "Sequence"))
class Sequence(object):

  @plankton.field("values")
  def __init__(self, values=None):
    self.values = values

  def accept(self, visitor):
    return visitor.visit_sequence(self)

  @staticmethod
  def make(values):
    if len(values) == 0:
      return Literal(None)
    elif len(values) == 1:
      return values[0]
    else:
      return Sequence(values)

  def __str__(self):
    return "#<sequence: %s>" % map(str, self.values)


# A local variable declaration.
@plankton.serializable(("ast", "LocalDeclaration"))
class LocalDeclaration(object):

  @plankton.field("symbol")
  @plankton.field("value")
  @plankton.field("body")
  def __init__(self, name=None, value=None, body=None, symbol=None):
    self.name = name
    self.symbol = symbol
    self.value = value
    self.body = body

  def accept(self, visitor):
    return visitor.visit_local_declaration(self)

  def __str__(self):
    return "#<def %s := %s in %s>" % (self.name, self.value, self.body)


# A symbol that identifies a scoped binding.
@plankton.serializable(("ast", "Symbol"))
class Symbol(object):

  @plankton.field("name")
  def __init__(self, name=None):
    self.name = str(name)


# An individual method parameter.
@plankton.serializable(("ast", "Parameter"))
class Parameter(object):

  @plankton.field("symbol")
  @plankton.field("tags")
  def __init__(self, name=None, tags=None):
    self.name = name
    self.symbol = None
    self.tags = tags

  def __str__(self):
    return "%s: %s" % (", ".join(map(str, self.tags)), self.name)


# An anonymous function. These can be broken down into equivalent new-object
# and set-property calls but that's for later.
@plankton.serializable(("ast", "Lambda"))
class Lambda(object):

  @plankton.field("parameters")
  @plankton.field("body")
  def __init__(self, parameters=[], body=None):
    self.parameters = parameters
    self.body = body

  def accept(self, visitor):
    return visitor.visit_lambda(self)

  def __str__(self):
    return "(fn (%s) => %s)" % (", ".join(map(str, self.parameters)), self.body)


@plankton.serializable(("ast", "Path"))
class Path(object):

  @plankton.field("parts")
  def __init__(self, parts=None):
    self.parts = parts

  def __eq__(self, that):
    return (isinstance(that, Path) and
            self.parts == that.parts)


@plankton.serializable(("ast", "Name"))
class Name(object):

  @plankton.field("path")
  @plankton.field("phase")
  def __init__(self, phase=None, path=None):
    self.phase = phase
    self.path = path

  def __hash__(self):
    return hash(self.phase) ^ hash(tuple(self.path))

  def __eq__(self, that):
    return (isinstance(that, Name) and
            self.phase == that.phase and
            self.path == that.path)

  def __str__(self):
    if self.phase < 0:
      prefix = "@" * -self.phase
    else:
      prefix = "$" * (self.phase + 1)
    return "#<name: %s%s>" % (prefix, ":".join(self.path))


@plankton.serializable(("ast", "Program"))
class Program(object):

  @plankton.field("elements")
  @plankton.field("entry_point")
  @plankton.field("namespace")
  @plankton.field("methodspace")
  def __init__(self, elements=None, entry_point=None, namespace=None,
      methodspace=None):
    self.elements = elements
    self.entry_point = entry_point
    self.namespace = namespace
    self.methodspace = methodspace

  def accept(self, visitor):
    return visitor.visit_program(self)

  def __str__(self):
    return "(program %s)" % " ".join(map(str, self.elements))


@plankton.serializable(("ast", "NamespaceDeclaration"))
class NamespaceDeclaration(object):

  @plankton.field("name")
  @plankton.field("value")
  def __init__(self, name=None, value=None):
    self.name = name
    self.value = value

  def accept(self, visitor):
    return visitor.visit_namespace_declaration(self)

  def apply(self, program, helper):
    name = tuple(self.name.path)
    value = helper.evaluate(self.value)
    program.namespace.bindings[name] = value

  def __str__(self):
    return "(namespace-declaration %s %s)" % (self.name, self.value)
