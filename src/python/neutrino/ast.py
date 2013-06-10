# Neutrino syntax tree definitions

import plankton

# A constant literal value.
@plankton.serializable
class Literal(object):

  @plankton.field("value")
  def __init__(self, value=None):
    self.value = value

  def __str__(self):
    return str(self.value)


# A reference to an enclosing binding.
@plankton.serializable
class Variable(object):

  @plankton.field("symbol")
  def __init__(self, symbol=None):
    self.symbol = symbol


# A binding from a symbol to a value.
@plankton.serializable
class Binding(object):

  @plankton.field("symbol")
  @plankton.field("value")
  def __init__(self, symbol=None, value=None):
    self.symbol = symbol
    self.value = value


# A sequence of expressions to execute in order, yielding the value of the last
# expression.
@plankton.serializable
class Sequence(object):

  @plankton.field("values")
  def __init__(self, values=None):
    self.values = values


# A symbol that identifies a scoped binding.
@plankton.serializable
class Symbol(object):

  @plankton.field("display_name")
  def __init__(self, display_name=None):
    self.display_name = display_name


@plankton.serializable
class Path(object):

  @plankton.field("parts")
  def __init__(self, parts=None):
    self.parts = parts


@plankton.serializable
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
    return "%s%s" % (prefix, ":".join(self.path))
