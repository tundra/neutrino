# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import plankton


class Method(object):

  def __init__(self, signature, syntax):
    self.signature = signature
    self.syntax = syntax


class Signature(object):

  def __init__(self, params):
    self.parameters = params


class Guard(object):

  _EQ = "="
  _IS = "i"
  _ANY = "*"

  def __init__(self, type, value):
    self.type = type
    self.value = value

  def __str__(self):
    return "%s(%s)" % (self.type, self.value)

  @staticmethod
  def any():
    return Guard(Guard._ANY, None)

  @staticmethod
  def eq(value):
    return Guard(Guard._EQ, value)


class Parameter(object):

  def __init__(self, tags, guard):
    self.tags = tags
    self.guard = guard


@plankton.serializable("core:Operation")
class Operation(object):

  _ASSIGN = 1
  _CALL = 2
  _INDEX = 3
  _INFIX = 4
  _PREFIX = 5
  _PROPERTY = 6

  @plankton.field("type")
  @plankton.field("value")
  def __init__(self, type, value):
    self.type = type
    self.value = value

  @staticmethod
  def call():
    return Operation(Operation._CALL, None)

  @staticmethod
  def index():
    return Operation(Operation._INDEX, None)

  @staticmethod
  def infix(value):
    return Operation(Operation._INFIX, value)

  @staticmethod
  def prefix(value):
    return Operation(Operation._PREFIX, value)

  @staticmethod
  def assign(value):
    return Operation(Operation._ASSIGN, value)

  @staticmethod
  def property(value):
    return Operation(Operation._PROPERTY, value)

  def __hash__(self):
    return hash(self.type) ^ hash(self.value)

  def __eq__(self, that):
    return isinstance(that, Operation) and (self.value == that.value) and (self.type == that.type)

  def get_name(self):
    if self.type == Operation._CALL:
      assert self.value is None
      return "()"
    if self.type == Operation._INDEX:
      assert self.value is None
      return "[]"
    elif self.type == Operation._INFIX:
      return ".%s" % self.value
    elif self.type == Operation._ASSIGN:
      return "%s:=" % self.value
    else:
      return repr(self)

  def __str__(self):
    return "(operation %s)" % self.get_name()


# A unique key, matching a neutrino runtime key.
@plankton.serializable("core:Key")
class Key(object):

  @plankton.field("id")
  def __init__(self, id):
    self.id = id

  def __str__(self):
    return "(key %s)" % self.id


@plankton.serializable("core:Path")
class Path(object):

  @plankton.field("names")
  def __init__(self, names):
    self.names = names

  # Is this path a single name?
  def is_singular(self):
    return len(self.names) == 1

  # Assumes this path is a single name and returns that name.
  def get_name(self):
    assert self.is_singular()
    return self.names[0]

  def get_head(self):
    return self.names[0]

  def get_path_parts(self):
    return self.names

  def __hash__(self):
    return hash(tuple(self.names))

  def __eq__(self, that):
    return (isinstance(that, Path) and
            self.names == that.names)

  def __str__(self):
    return "".join([":%s" % name for name in self.names])


@plankton.serializable("core:Identifier")
class Identifier(object):

  @plankton.field("path")
  @plankton.field("stage")
  def __init__(self, stage, path):
    self.stage = stage
    self.path = path

  def __hash__(self):
    return hash(self.stage) ^ hash(self.path)

  def __eq__(self, that):
    return (isinstance(that, Identifier) and
            self.stage == that.stage and
            self.path == that.path)

  # Returns the unique name this identifier is made of. If it is not a single
  # name an error is reported.
  def get_name(self):
    return self.path.get_name()

  def get_path_parts(self):
    return self.path.get_path_parts()

  # Returns the same identifier shifted one step into the past.
  def shift_back(self):
    return Identifier(self.stage - 1, self.path)

  def __str__(self):
    if self.stage < 0:
      prefix = "@" * -self.stage
    else:
      prefix = "$" * (self.stage + 1)
    return "(name %s %s)" % (prefix, self.path)


@plankton.serializable("core:Function")
class Function(object):

  @plankton.field("display_name")
  def __init__(self, display_name):
    self.display_name = display_name


@plankton.serializable("core:Library")
class Library(object):

  @plankton.field("modules")
  def __init__(self, modules={}):
    self.modules = modules

  def add_module(self, path, module):
    assert not path in self.modules
    self.modules[path] = module


@plankton.serializable("core:DecimalFraction")
class DecimalFraction(object):

  @plankton.field("numerator")
  @plankton.field("denominator")
  @plankton.field("precision")
  def __init__(self, numerator, denominator, precision):
    self.numerator = numerator
    self.denominator = denominator
    self.precision = precision

  def __eq__(self, that):
    if not isinstance(that, DecimalFraction):
      return False
    return (self.numerator == that.numerator
       and self.denominator == that.denominator
       and self.precision == that.precision)

  def __str__(self):
    return "(decimal %i/%i@%i)" % (self.numerator, self.denominator, self.precision)


_SUBJECT = Key("core:subject")
_SELECTOR = Key("core:selector")
_TRANSPORT = Key("core:transport")
_SYNC = Key("core:sync")
_ASYNC = Key("core:async")
