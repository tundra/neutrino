# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import plankton
from plankton import codec
import unittest


@plankton.serializable
class Object(object):

  def __init__(self, header=None):
    self.header = header
    self.payload = None

  def set_header(self, value):
    self.header = value
    return self

  @plankton.header
  def get_header(self):
    return self.header

  def set_payload(self, value):
    self.payload = value
    return self

  def set_contents(self, value):
    return self.set_payload(value)

  @plankton.payload
  def get_payload(self):
    return self.payload


class Context(object):

  def __init__(self):
    self.refs = {}

  def new_object(self, id=None):
    result = Object()
    if not id is None:
      self.refs[id] = result
    return result

  def get_ref(self, id):
    return self.refs[id]


class RecordingAssembler(object):

  def __init__(self):
    self.entries = []

  def tag(self, value):
    self.entries.append(('tag', value))
    return self

  def int32(self, value):
    self.entries.append(('int32', value))
    return self

  def uint32(self, value):
    self.entries.append(('uint32', value))
    return self

  def blob(self, bytes):
    self.entries.append(('blob', str(bytes)))
    return self


class TestCase(unittest.TestCase):

  def new_context(self):
    return Context()

  def run_test(self, input, assemble):
    self.run_encode_test(input, assemble)
    self.run_decode_test(input, assemble)

  def run_decode_test(self, input, assemble):
    # Write the assembly using the assembler. We're assuming the assembler works
    # (which gets tested by other tests).
    assm = codec.EncodingAssembler()
    assemble(assm)
    bytes = assm.bytes
    # Then try decoding the bytes.
    decoder = plankton.Decoder(default_object=Object)
    decoded = decoder.decode(bytes)
    self.check_equals(input, decoded)

  def run_encode_test(self, input, assemble):
    expected_assm = RecordingAssembler()
    assemble(expected_assm)
    expected = expected_assm.entries
    found_assm = RecordingAssembler()
    plankton.Encoder().write(input, assembler=found_assm)
    found = found_assm.entries
    self.assertEquals(expected, found)

  def are_equal(self, a, b, seen):
    if isinstance(a, Object) and isinstance(b, Object):
      for (sa, sb) in seen:
        if sa is a:
          return sb is b
      seen.append((a, b))
      ah = a.header
      bh = b.header
      ap = a.payload
      bp = b.payload
      return self.are_equal(ah, bh, seen) and self.are_equal(ap, bp, seen)
    elif isinstance(a, list) and isinstance(b, list):
      if len(a) != len(b):
        return false
      for i in xrange(0, len(a)):
        if not self.are_equal(a[i], b[i], seen):
          return False
      return True
    elif isinstance(a, dict) and isinstance(b, dict):
      if len(a) != len(b):
        return False
      for k in a.keys():
        if not k in b:
          return False
        if not self.are_equal(a[k], b[k], seen):
          return False
      return True
    elif a == b:
      return True
    else:
      return False


  def check_equals(self, expected, decoded):
    if not self.are_equal(expected, decoded, []):
      self.assertEquals(expected, decoded)
