# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import plankton

# A constant literal value.
@plankton.serializable(("core", "Namespace"))
class Namespace(object):

  @plankton.field("bindings")
  def __init__(self, bindings=None):
    self.bindings = bindings

  def add_binding(self, name, value):
    self.bindings[name] = value


# A unique key, matching a neutrino runtime key.
class Key(plankton.EnvironmentPlaceholder):

  def __init__(self, display_name, key):
    self.display_name = display_name
    self.key = key

  def __str__(self):
    return "(key %s)" % self.display_name


_SUBJECT = Key("subject", ("core", "subject"))
_SELECTOR = Key("selector", ("core", "selector"))
