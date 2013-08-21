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
@plankton.serializable(("ast", "Variable"))
class Variable(object):

  # We don't need to serialize the name since the symbol holds the name.
  @plankton.field("symbol")
  def __init__(self, name=None, symbol=None):
    self.name = name
    self.symbol = symbol

  def accept(self, visitor):
    return visitor.visit_variable(self);

  def __str__(self):
    return "#<var %s>" % str(self.name)


# A multi-method invocation.
@plankton.serializable(("ast", "Invocation"))
class Invocation(object):

  @plankton.field("arguments")
  def __init__(self, arguments=None):
    self.arguments = arguments

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
    self.value = value
    self.body = body
    self.symbol = symbol

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


@plankton.serializable()
class Path(object):

  @plankton.field("parts")
  def __init__(self, parts=None):
    self.parts = parts

  def __eq__(self, that):
    return (isinstance(that, Path) and
            self.parts == that.parts)


@plankton.serializable()
class Name(object):

  @plankton.field("path")
  @plankton.field("phase")
  def __init__(self, phase=None, path=None):
    self.phase = phase
    self.path = path

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
