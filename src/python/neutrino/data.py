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


@plankton.serializable(plankton.EnvironmentReference.path("core", "Operation"))
class Operation(object):

  _CALL = 2
  _INFIX = 4
  _PREFIX = 5

  @plankton.field("type")
  @plankton.field("value")
  def __init__(self, type, value):
    self.type = type
    self.value = value

  @staticmethod
  def call():
    return Operation(Operation._CALL, None)

  @staticmethod
  def infix(value):
    return Operation(Operation._INFIX, value)

  @staticmethod
  def prefix(value):
    return Operation(Operation._PREFIX, value)

  def __hash__(self):
    return hash(self.type) ^ hash(self.value)

  def __eq__(self, that):
    return isinstance(that, Operation) and (self.value == that.value) and (self.type == that.type)

  def get_name(self):
    if self.type == Operation._CALL:
      assert self.value is None
      return "()"
    elif self.type == Operation._INFIX:
      return ".%s" % self.value
    else:
      return repr(self)

  def __str__(self):
    return "(operation %s)" % self.get_name()


# A unique key, matching a neutrino runtime key.
class Key(plankton.EnvironmentReference):

  def __init__(self, display_name, key):
    self.display_name = display_name
    self.key = key

  def __str__(self):
    return "(key %s)" % self.display_name


@plankton.serializable(plankton.EnvironmentReference.path("core", "Protocol"))
class Protocol(object):

  @plankton.field("name")
  def __init__(self, display_name):
    self.name = display_name


# A user-defined object instance.
@plankton.serializable()
class Instance(object):

  def __init__(self, protocol):
    self.protocol = protocol

  @plankton.header
  def get_header(self):
    return self.protocol

  @plankton.payload
  def get_payload(self):
    return {}


@plankton.serializable(plankton.EnvironmentReference.path("core", "Path"))
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


@plankton.serializable(plankton.EnvironmentReference.path("core", "Identifier"))
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

  def __str__(self):
    if self.stage < 0:
      prefix = "@" * -self.stage
    else:
      prefix = "$" * (self.stage + 1)
    return "(name %s %s)" % (prefix, self.path)


@plankton.serializable(plankton.EnvironmentReference.path("core", "Function"))
class Function(object):

  @plankton.field("display_name")
  def __init__(self, display_name):
    self.display_name = display_name


@plankton.serializable(plankton.EnvironmentReference.path("core", "Library"))
class Library(object):

  @plankton.field("modules")
  def __init__(self, modules={}):
    self.modules = modules

  def add_module(self, path, module):
    assert not path in self.modules
    self.modules[path] = module


_SUBJECT = Key("subject", ("core", "subject"))
_SELECTOR = Key("selector", ("core", "selector"))
