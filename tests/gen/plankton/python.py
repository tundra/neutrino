# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Test case generator for python


from framework import AbstractAssembly, E, Object


class PythonAssembly(AbstractAssembly):

  def __init__(self):
    self.ops = []

  def tag(self, code):
    self.ops.append('tag(%i)' % code)
    return self

  def int32(self, value):
    self.ops.append('int32(%i)' % value)
    return self

  def uint32(self, value):
    self.ops.append('uint32(%i)' % value)
    return self

  def blob(self, bytes):
    self.ops.append('blob(bytearray(%s))' % str(list(bytes)))
    return self

  def to_string(self):
    return "lambda assm: assm.%s" % (".".join(self.ops))


class PythonGenerator(object):

  def new_assembly(self):
    return PythonAssembly()

  def emit_value(self, value, out):
    if isinstance(value, int) or (value == None):
      out.append(str(value))
    elif isinstance(value, str):
      out.append("\"%s\"" % value)
    elif isinstance(value, tuple) or isinstance(value, list):
      out.append('[')
      first = True
      for element in value:
        if first:
          first = False
        else:
          out.append(', ')
        out.append(E(element))
      out.append(']')
    elif isinstance(value, dict):
      out.append('{\n').indent(+1)
      first = True
      for key in sorted(value.keys()):
        if first:
          first = False
        else:
          out.append(',\n')
        out.append(E(key), ': ', E(value[key]))
      out.indent(-1).append('}')
    else:
      assert isinstance(value, Object)
      serial = value.serial
      if serial in out.refs:
        out.append('ctx.get_ref(%i)' % serial)
      else:
        out.refs[serial] = value
        (out
          .append('(ctx.new_object(id=%i)\n' % serial)
          .indent(+1)
            .append('.set_header(', E(value.header), ')\n')
            .append('.set_payload(', E(value.payload), '))')
          .indent(-1))

  def emit_test(self, test, out):
    (out
      .append('\ndef test_%s(self):\n' % test.name)
      .indent(+1)
        .append('ctx = self.new_context()\n')
        .append('input = ', E(test.input), '\n')
        .append('assemble = ', test.assembly.to_string(), '\n')
        .append('self.run_test(input, assemble)\n')
      .indent(-1))

  def emit_preamble(self, out):
    (out
      .append('# This test was generated from tests/gen/plankton/testdata.py.\n')
      .append('# Don\'t modify it by hand.\n\n\n')
      .append('import unittest\n')
      .append('import planktontest\n\n\n')
      .append('class PlanktonTests(planktontest.TestCase):\n')
      .indent(+1))

  def emit_epilogue(self, out):
    (out
        .indent(-1)
      .append('\n\nif __name__ == \'__main__\':\n')
      .indent(+1)
        .append('runner = unittest.TextTestRunner(verbosity=0)\n')
        .append('unittest.main(testRunner=runner)\n')
      .indent(-1))
