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


@plankton.serializable(("core", "Methodspace"))
class Methodspace(object):

  @plankton.field("inheritance")
  @plankton.field("methods")
  @plankton.field("imports")
  def __init__(self, inheritance=None, methods=None, imports=None):
    self.inheritance = inheritance
    self.methods = methods
    self.imports = imports


@plankton.serializable(("core", "Module"))
class Module(object):

  @plankton.field("namespace")
  @plankton.field("methodspace")
  def __init__(self, namespace=None, methodspace=None):
    self.namespace = namespace
    self.methodspace = methodspace


@plankton.serializable(("core", "Method"))
class Method(object):

  @plankton.field("signature")
  @plankton.field("implementation")
  def __init__(self, signature=None, implementation=None):
    self.signature = signature
    self.implementation = implementation


@plankton.serializable(("core", "Guard"))
class Guard(object):

  _EQ = "="
  _IS = "i"
  _ANY = "*"

  @plankton.field("type")
  @plankton.field("value")
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


# A unique key, matching a neutrino runtime key.
class Key(plankton.EnvironmentPlaceholder):

  def __init__(self, display_name, key):
    self.display_name = display_name
    self.key = key

  def __str__(self):
    return "(key %s)" % self.display_name


_SUBJECT = Key("subject", ("core", "subject"))
_SELECTOR = Key("selector", ("core", "selector"))
