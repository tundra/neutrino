#!/usr/bin/python

from neutrino.token import Token, tokenize
import unittest


# Convenience shorthands
wd = Token.word
pt = Token.punctuation
op = Token.operation
tg = Token.tag
lt = Token.literal
id = Token.identifier


class TokenTest(unittest.TestCase):
  
  def run_test(self, input, *expected):
    found = tokenize(input)
    self.assertEquals(list(expected), found)

  def test_simple_tokens(self):
    test = self.run_test
    test("foo", wd("foo"));
    test("foo bar baz", wd("foo"), wd("bar"), wd("baz"));
    test("$foo", id("$foo"));
    test("$foo$bar$baz", id("$foo"), id("$bar"), id("$baz"));
    test("foo$bar baz", wd("foo"), id("$bar"), wd("baz"));
    test(".foo", op("foo"));
    test(".foo.bar.baz", op("foo"), op("bar"), op("baz"));
    test("$foo.bar", id("$foo"), op("bar"));
    test("+ ++ +++", op("+"), op("++"), op("+++"));
    test("+ - * / % < > =", op("+"), op("-"), op("*"), op("/"), op("%"),
        op("<"), op(">"), op("="));
    test("$foo + $bar", id("$foo"), op("+"), id("$bar"));
    test("$foo:bar:baz", id("$foo:bar:baz"));
    test("(){};", pt('('), pt(')'), pt('{'), pt('}'), pt(';', Token.EXPLICIT_DELIMITER));
    test("foo:", tg("foo"));
    test("10", lt(10));
    test("10 000", lt(10), lt(000));
    test("10:", tg(10));
    test("\"boo\"", lt("boo"));
    test("def $x := 4 ;", wd("def"), id("$x"), op(":="), lt(4),
        pt(';', Token.EXPLICIT_DELIMITER));
    test("for $i in 0 -> 10", wd("for"), id("$i"), wd("in"), lt(0),
        op("->"), lt(10));
    test("+ - * /", op("+"), op("-"), op("*"), op("/"));


  def test_implicit_semis(self):
    test = self.run_test
    test("} foo bar", pt('}'), wd("foo", Token.IMPLICIT_DELIMITER), wd("bar"));
    test("} ; bar", pt('}'), pt(';', Token.EXPLICIT_DELIMITER),
        wd("bar", Token.IMPLICIT_DELIMITER));


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
