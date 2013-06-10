#!/usr/bin/python

from neutrino import parser, token, ast
import unittest


lt = ast.Literal


class ParserTest(unittest.TestCase):
  
  def check_expression(self, input, expected):
    found = parser.Parser(token.tokenize(input)).parse_expression()
    # Convert the asts to strings because that's just infinitely easier to
    # debug when assertions fail. Of course that requires that ast string
    # conversion is sane, which it is.
    self.assertEquals(str(expected), str(found))

  def test_expression(self):
    test = self.check_expression
    test('1', lt(1))


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
