# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


# Framework for generating plankton tests.


from plankton import codec


_next_serial = 0
# Returns the next in a sequence of serial numbers.
def get_next_serial():
  global _next_serial
  result = _next_serial
  _next_serial += 1
  return result


# A generic object.
class Object(object):

  def __init__(self):
    self.header = None
    self.payload = None
    self.serial = get_next_serial()

  def set_header(self, value):
    self.header = value
    return self

  def set_payload(self, value):
    self.payload = value
    return self

  def __str__(self):
    return "Object(#%i)" % self.serial


# Creates a new, initially empty, object and if an id is specified registers it
# under that id.
def new_object(id=None):
  result = Object()
  if not id is None:
    get_active().add_ref(id, result)
  return result


# Returns the object registered under the given id.
def ref(id):
  return get_active().get_ref(id)


ACTIVE = None


# Returns the currently active assembler.
def get_active():
  global ACTIVE
  return ACTIVE


# Sets the currently active assembler
def set_active(value):
  global ACTIVE
  ACTIVE = value


# The machinery used to generate a test case.
class TestAssembler(object):

  def __init__(self, generator):
    self.name = None
    self.refs = {}
    self.input = None
    self.assembly = None
    self.generator = generator

  # Sets the descriptive name of this test
  def set_name(self, value):
    self.value = value
    return self

  # Add a reference to the given value.
  def add_ref(self, id, value):
    self.refs[id] = value

  # Returns the value with the given id.
  def get_ref(self, id):
    return self.refs[id]

  # Sets the input to check for.
  def set_input(self, value):
    self.input = value
    return self

  # Sets the high-level opcode corresponding to the input.
  def get_assembly(self):
    return self.assembly

  # Registers this as the active assembler.
  def begin(self):
    assert get_active() == None
    set_active(self)

  # Unregisters this as the active assembler.
  def end(self):
    assert get_active() == self
    set_active(None)


# Wrapper that escapes a value given to the output stream's append and ensures
# that it gets emitted using the generator rather than just added as a string.
class E(object):

  def __init__(self, value):
    self.value = value


class OutputStream(object):

  def __init__(self, generator):
    self.generator = generator
    self.level = 0
    self.pending_newline = False
    self.refs = {}
    self.chars = []

  def append(self, *parts):
    for part in parts:
      if isinstance(part, E):
        self.generator.emit_value(part.value, self)
      else:
        self.append_chunk(part)
    return self

  def append_chunk(self, part):
    for c in part:
      if c == '\n':
        self.flush_pending_newline()
        self.pending_newline = True
      else:
        self.append_char(c)

  def append_char(self, c):
    self.flush_pending_newline()
    self.chars.append(c)

  def flush_pending_newline(self):
    if self.pending_newline:
      self.pending_newline = False
      self.append_char('\n')
      for i in xrange(0, self.level):
        self.append_char('  ')

  def indent(self, delta):
    self.level += delta
    return self

  def flush(self):
    self.flush_pending_newline()
    return "".join(self.chars)


class AbstractAssembly(object):

  def string(self, str):
    return self.tag(tSTRING).uint32(len(str)).blob(bytearray(str))

  def int(self, value):
    return self.tag(tINT32).int32(value)

  def array(self, n):
    return self.tag(tARRAY).uint32(n)

  def map(self, n):
    return self.tag(tMAP).uint32(n)

  def object(self):
    return self.tag(tOBJECT)

  def ref(self, n):
    return self.tag(tREF).uint32(n)

  def null(self):
    return self.tag(tNULL)

  def true(self):
    return self.tag(tTRUE)

  def false(self):
    return self.tag(tFALSE)


# Tag values.
tINT32 = codec._INT32_TAG
tSTRING = codec._STRING_TAG
tNULL = codec._NULL_TAG
tTRUE = codec._TRUE_TAG
tFALSE = codec._FALSE_TAG
tARRAY = codec._ARRAY_TAG
tMAP = codec._MAP_TAG
tOBJECT = codec._OBJECT_TAG
tREF = codec._REFERENCE_TAG
