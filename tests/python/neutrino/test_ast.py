#!/usr/bin/python

from neutrino import ast
import plankton
import unittest


_ENCODER = plankton.Encoder()
_DECODER = plankton.Decoder()


class AstTest(unittest.TestCase):

  def test_simple_encoding(self):
    lb = ast.Literal(None)
    la = _DECODER.decode(_ENCODER.encode(lb))
    self.assertEquals(None, la.value)

  def test_symbol_encoding(self):
    sb = ast.Symbol("x")
    eb = ast.Binding(sb, ast.Sequence([ast.Variable(symbol=sb), ast.Variable(symbol=sb)]))
    ea = _DECODER.decode(_ENCODER.encode(eb))
    self.assertTrue(ea.symbol is ea.value.values[0].symbol)
    self.assertTrue(ea.symbol is ea.value.values[1].symbol)


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
