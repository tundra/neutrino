#!/usr/bin/python

from neutrino import parser, token, ast
import unittest


ar = lambda *e: ast.Array(e)
df = ast.LocalDeclaration
lm = ast.Lambda
lt = ast.Literal
nd = ast.NamespaceDeclaration
pg = lambda *e: ast.Program(e)
pm = lambda n, *t: ast.Parameter(n, t)
sq = lambda *e: ast.Sequence(e)

def nm(names, phase=0):
  if isinstance(names, list):
    return ast.Name(phase, names)
  else:
    return ast.Name(phase, [names])

def id(names, phase=0):
  name = nm(names, phase)
  return ast.Variable(name=name)

def bn(left, op, right):
  return ast.Invocation([
    ast.Argument('this', left),
    ast.Argument('name', ast.Literal(op)),
    ast.Argument(0, right)
  ])

def cl(fun, *poss):
  args = [
    ast.Argument('this', fun),
    ast.Argument('name', ast.Literal("()"))
  ]
  for i in xrange(len(poss)):
    args.append(ast.Argument(i, poss[i]))
  return ast.Invocation(args)

class ParserTest(unittest.TestCase):

  def check_expression(self, input, expected):
    found = parser.Parser(token.tokenize(input)).parse_expression()
    # Convert the asts to strings because that's just infinitely easier to
    # debug when assertions fail. Of course that requires that ast string
    # conversion is sane, which it is.
    self.assertEquals(str(expected), str(found))

  def check_program(self, input, expected):
    found = parser.Parser(token.tokenize(input)).parse_program()
    self.assertEquals(str(expected), str(found))

  def test_atomic_expressions(self):
    test = self.check_expression
    test('1', lt(1))
    test('"foo"', lt('foo'))
    test('$foo', id('foo'))
    test('@foo', id('foo', -1))
    test('@foo:bar', id(['foo', 'bar'], -1))
    test('(1)', lt(1))
    test('((($foo)))', id('foo'))
    test('[]', ar())
    test('[1]', ar(lt(1)))
    test('[2, 3]', ar(lt(2), lt(3)))
    test('[4, 5, 6]', ar(lt(4), lt(5), lt(6)))
    test('[7, [8, [9]]]', ar(lt(7), ar(lt(8), ar(lt(9)))))
    test('null', lt(None))
    test('true', lt(True))
    test('false', lt(False))

  def test_calls(self):
    test = self.check_expression
    test('1 + 2', bn(lt(1), '+', lt(2)))
    test('1 + 2 + 3', bn(bn(lt(1), '+', lt(2)), '+', lt(3)))
    test('$a()', cl(id("a")))
    test('$a()()', cl(cl(id("a"))))
    test('$a(1)', cl(id("a"), lt(1)))
    test('$a(1, 2)', cl(id("a"), lt(1), lt(2)))
    test('$a(1, 2, 3)', cl(id("a"), lt(1), lt(2), lt(3)))
    test('$a(1)(2)(3)', cl(cl(cl(id("a"), lt(1)), lt(2)), lt(3)))

  def test_sequence(self):
    test = self.check_expression
    test('{}', lt(None))
    test('{1;}', lt(1))
    test('{1; 2;}', sq(lt(1), lt(2)))
    test('{1; 2; 3;}', sq(lt(1), lt(2), lt(3)))
    test('{1; 2; 3; 4;}', sq(lt(1), lt(2), lt(3), lt(4)))
    test('{1; {2;} 3; 4;}', sq(lt(1), lt(2), lt(3), lt(4)))

  def test_local_definitions(self):
    test = self.check_expression
    test('{ def $x := 4; $x; }', df(nm("x"), lt(4), id("x")))
    test('{ def $x := 4; $x; $y; }', df(nm("x"), lt(4), sq(id("x"), id("y"))))
    test('{ def $x := 4; }', df(nm("x"), lt(4), lt(None)))
    test('{ $x; $y; def $x := 4; }', sq(id("x"), id("y"), df(nm("x"), lt(4), lt(None))))

  def test_lambda(self):
    test = self.check_expression
    test('fn () => $x', lm([], id("x")))
    test('fn ($x) => $x', lm([pm(nm("x"), 0)], id("x")))
    test('fn ($x, $y, $z) => $x', lm([pm(nm("x"), 0), pm(nm("y"), 1), pm(nm("z"), 2)], id("x")))
    test('fn ($x, $y) => $x', lm([pm(nm("x"), 0), pm(nm("y"), 1)], id("x")))
    test('fn ($x, $y, $z) => $x', lm([pm(nm("x"), 0), pm(nm("y"), 1), pm(nm("z"), 2)], id("x")))
    test('fn => $x', lm([], id("x")))

  def test_program_toplevel(self):
    test = self.check_program
    test('def $x := 5;', pg(nd(nm("x"), lt(5))))
    test('', pg())
    test('def $x := 5; def $y := 6;', pg(nd(nm("x"), lt(5)), nd(nm("y"), lt(6))))

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
