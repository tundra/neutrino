# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import plankton

@plankton.serializable(("core", "Namespace"))
class Namespace(object):

  @plankton.field("bindings")
  def __init__(self, bindings=None):
    self.bindings = bindings

  def add_binding(self, name, value):
    self.bindings[name] = value

  def lookup(self, name):
    import interp
    special_binding = interp.lookup_special_binding(name)
    if special_binding is None:
      return self.bindings.get(name, None)
    else:
      return special_binding


@plankton.serializable(("core", "Methodspace"))
class Methodspace(object):

  @plankton.field("inheritance")
  @plankton.field("methods")
  @plankton.field("imports")
  def __init__(self, inheritance=None, methods=None, imports=None):
    self.inheritance = inheritance
    self.methods = methods
    self.imports = imports

  def add_method(self, method):
    self.methods.append(method)


@plankton.serializable(("core", "Module"))
class Module(object):

  @plankton.field("namespace")
  @plankton.field("methodspace")
  @plankton.field("display_name")
  def __init__(self, namespace=None, methodspace=None, display_name=None):
    self.namespace = namespace
    self.methodspace = methodspace
    self.display_name = display_name


class Method(object):

  def __init__(self, signature=None, syntax=None):
    self.signature = signature
    self.syntax = syntax


class Signature(object):

  def __init__(self, params):
    self.parameters = params


class Guard(object):

  _EQ = "="
  _IS = "i"
  _ANY = "*"

  def __init__(self, type=None, value=None):
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

  def __init__(self, tags=None, guard=None):
    self.tags = tags
    self.guard = guard


# A unique key, matching a neutrino runtime key.
class Key(plankton.EnvironmentPlaceholder):

  def __init__(self, display_name, key):
    self.display_name = display_name
    self.key = key

  def __str__(self):
    return "(key %s)" % self.display_name


@plankton.serializable(("core", "Protocol"))
class Protocol(object):

  @plankton.field("name")
  def __init__(self, display_name=None):
    self.name = display_name


# A user-defined object instance.
@plankton.virtual
@plankton.serializable()
class Instance(object):

  def __init__(self, protocol=None):
    self.protocol = protocol

  def get_header(self):
    return self.protocol

  def get_payload(self):
    return {}


@plankton.serializable(("core", "Path"))
class Path(object):

  @plankton.field("names")
  def __init__(self, names=None):
    self.names = names

  # Is this path a single name?
  def is_singular(self):
    return len(self.names) == 1

  # Assumes this path is a single name and returns that name.
  def as_singular(self):
    assert self.is_singular()
    return self.names[0]

  def __hash__(self):
    return hash(tuple(self.names))

  def __eq__(self, that):
    return (isinstance(that, Path) and
            self.names == that.names)

  def __str__(self):
    return "".join([":%s" % name for name in self.names])

@plankton.serializable(("core", "Identifier"))
class Identifier(object):

  @plankton.field("path")
  @plankton.field("stage")
  def __init__(self, stage=None, path=None):
    self.stage = stage
    self.path = path

  def __hash__(self):
    return hash(self.stage) ^ hash(self.path)

  def __eq__(self, that):
    return (isinstance(that, Identifier) and
            self.stage == that.stage and
            self.path == that.path)

  def __str__(self):
    if self.stage < 0:
      prefix = "@" * -self.stage
    else:
      prefix = "$" * (self.stage + 1)
    return "(name %s %s)" % (prefix, self.path)


_SUBJECT = Key("subject", ("core", "subject"))
_SELECTOR = Key("selector", ("core", "selector"))
