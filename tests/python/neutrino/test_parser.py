#!/usr/bin/python

from neutrino import parser, token, ast
import unittest


lt = ast.Literal
ar = lambda *e: ast.Array(e)

def id(phase, *names):
  name = ast.Name(phase, list(names))
  return ast.Variable(name=name)

def bn(left, op, right):
  return ast.Invocation([
    ast.Argument('this', left),
    ast.Argument('name', ast.Literal(op)),
    ast.Argument(0, right)
  ])


class ParserTest(unittest.TestCase):
  
  def check_expression(self, input, expected):
    found = parser.Parser(token.tokenize(input)).parse_expression()
    # Convert the asts to strings because that's just infinitely easier to
    # debug when assertions fail. Of course that requires that ast string
    # conversion is sane, which it is.
    self.assertEquals(str(expected), str(found))

  def test_atomic_expressions(self):
    test = self.check_expression
    test('1', lt(1))
    test('"foo"', lt('foo'))
    test('$foo', id(0, 'foo'))
    test('@foo', id(-1, 'foo'))
    test('@foo:bar', id(-1, 'foo', 'bar'))
    test('(1)', lt(1))
    test('((($foo)))', id(0, 'foo'))
    test('[]', ar())
    test('[1]', ar(lt(1)))
    test('[2, 3]', ar(lt(2), lt(3)))
    test('[4, 5, 6]', ar(lt(4), lt(5), lt(6)))
    test('[7, [8, [9]]]', ar(lt(7), ar(lt(8), ar(lt(9)))))

  def test_calls(self):
    test = self.check_expression
    test('1 + 2', bn(lt(1), '+', lt(2)))
    test('1 + 2 + 3', bn(bn(lt(1), '+', lt(2)), '+', lt(3)))


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
