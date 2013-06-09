#!/usr/bin/python

from neutrino import ast
import plankton
import unittest


class AstTest(unittest.TestCase):

  def test_simple_encoding(self):
    lb = ast.Literal(None)
    la = plankton.deserialize(plankton.serialize(lb))
    self.assertEquals(None, la.value)

  def test_symbol_encoding(self):
    sb = ast.Symbol("x")
    eb = ast.Binding(sb, ast.Sequence([ast.Variable(sb), ast.Variable(sb)]))
    ea = plankton.deserialize(plankton.serialize(eb))
    self.assertTrue(ea.symbol is ea.value.values[0].symbol)
    self.assertTrue(ea.symbol is ea.value.values[1].symbol)


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
