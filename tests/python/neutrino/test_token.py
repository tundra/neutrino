#!/usr/bin/python

from neutrino import ast
from neutrino.token import Token, tokenize
import unittest


# Convenience shorthands
wd = Token.word
pt = Token.punctuation
op = Token.operation
tg = Token.tag
lt = Token.literal

def id(phase, *names):
  return Token.identifier(ast.Name(phase, list(names)))


class TokenTest(unittest.TestCase):
  
  def run_test(self, input, *expected):
    found = tokenize(input)
    self.assertEquals(list(expected), found)

  def test_simple_tokens(self):
    test = self.run_test
    test("foo", wd("foo"))
    test("foo bar baz", wd("foo"), wd("bar"), wd("baz"))
    test("$foo", id(0, "foo"))
    test("$foo$bar$baz", id(0, "foo"), id(0, "bar"), id(0, "baz"))
    test("foo$bar baz", wd("foo"), id(0, "bar"), wd("baz"))
    test(".foo", op("foo"))
    test(".foo.bar.baz", op("foo"), op("bar"), op("baz"))
    test("$foo.bar", id(0, "foo"), op("bar"))
    test("+ ++ +++", op("+"), op("++"), op("+++"))
    test("+ - * / % < > =", op("+"), op("-"), op("*"), op("/"), op("%"),
        op("<"), op(">"), op("="))
    test("$foo + $bar", id(0, "foo"), op("+"), id(0, "bar"))
    test("(){};", pt('('), pt(')'), pt('{'), pt('}'), pt(';', Token.EXPLICIT_DELIMITER))
    test("foo:", tg("foo"))
    test("10", lt(10))
    test("10 000", lt(10), lt(000))
    test("10:", tg(10))
    test("\"boo\"", lt("boo"))
    test("def $x := 4 ;", wd("def"), id(0, "x"), op(":="), lt(4),
        pt(';', Token.EXPLICIT_DELIMITER))
    test("for $i in 0 -> 10", wd("for"), id(0, "i"), wd("in"), lt(0),
        op("->"), lt(10))
    test("+ - * /", op("+"), op("-"), op("*"), op("/"))

  def test_implicit_semis(self):
    test = self.run_test
    test("} foo bar", pt('}'), wd("foo", Token.IMPLICIT_DELIMITER), wd("bar"));
    test("} ; bar", pt('}'), pt(';', Token.EXPLICIT_DELIMITER),
        wd("bar", Token.IMPLICIT_DELIMITER));

  def check_name(self, input, expected):
    tokens = tokenize(input)
    self.assertEquals(1, len(tokens))
    self.assertEquals(expected, tokens[0].value)

  def test_name(self):
    test = self.check_name
    def nm(phase, *parts):
      return ast.Name(phase, list(parts))
    test("$foo:bar:baz", nm(0, "foo", "bar", "baz"))
    test("$foo:bar", nm(0, "foo", "bar"));
    test("$foo", nm(0, "foo"));
    test("@foo:bar:baz", nm(-1, "foo", "bar", "baz"))
    test("@foo:bar", nm(-1, "foo", "bar"));
    test("@foo", nm(-1, "foo"));
    test("$$foo:bar:baz", nm(1, "foo", "bar", "baz"))
    test("$$foo:bar", nm(1, "foo", "bar"));
    test("$$foo", nm(1, "foo"));
    test("@@foo:bar:baz", nm(-2, "foo", "bar", "baz"))
    test("@@foo:bar", nm(-2, "foo", "bar"));
    test("@@foo", nm(-2, "foo"));


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)