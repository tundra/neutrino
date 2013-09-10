# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

import plankton

# A constant literal value.
@plankton.serializable(("core", "Namespace"))
class Namespace(object):

  @plankton.field("bindings")
  def __init__(self, bindings=None):
    self.bindings = bindings
