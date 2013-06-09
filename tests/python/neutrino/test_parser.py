#!/usr/bin/python

from neutrino import parser, token, ast
import unittest


lt = ast.Literal


class ParserTest(unittest.TestCase):
  
  def check_expression(self, input, expected):
    print parser.Parser(token.tokenize(input)).parse_expression()

  def test_expression(self):
    test = self.check_expression
    test('1', lt(1))


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
