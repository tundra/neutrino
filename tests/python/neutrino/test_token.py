#!/usr/bin/python

from neutrino import ast, data
from neutrino.token import Token, tokenize
import unittest


# Convenience shorthands
wd = Token.word
pt = Token.punctuation
op = Token.operation
tg = Token.tag
lt = Token.literal
ed = Token.end
qt = Token.quote
dec = data.DecimalFraction


def id(phase, *names):
  return Token.identifier(data.Identifier(phase, data.Path(list(names))))


class TokenTest(unittest.TestCase):

  def run_test(self, input, *expected):
    found = tokenize(input)
    extended = list(expected)
    if not extended[-1].has_type(Token.END):
      extended.append(Token.end())
    self.assertEquals(extended, found)

  def test_simple_tokens(self):
    test = self.run_test
    test("foo", wd("foo"))
    test("foo bar baz", wd("foo"), wd("bar"), wd("baz"))
    test("$foo", id(0, "foo"))
    test("$foo$bar$baz", id(0, "foo"), id(0, "bar"), id(0, "baz"))
    test("foo$bar baz", wd("foo"), id(0, "bar"), wd("baz"))
    test(".foo", op("foo"))
    test(".+", op("+"))
    test(".foo.bar.baz", op("foo"), op("bar"), op("baz"))
    test("$foo.bar", id(0, "foo"), op("bar"))
    test("+ ++ +++", op("+"), op("++"), op("+++"))
    test("+ - * / % < > =", op("+"), op("-"), op("*"), op("/"), op("%"),
        op("<"), op(">"), op("="))
    test("$foo + $bar", id(0, "foo"), op("+"), id(0, "bar"))
    test("(){};", pt('('), pt(')'), pt('{'), pt('}'), pt(';', Token.EXPLICIT_DELIMITER),
        ed(Token.IMPLICIT_DELIMITER))
    test("foo:", tg("foo"))
    test("0:", tg(0))
    test("10", lt(10))
    test("10 000", lt(10), lt(000))
    test("10:", tg(10))
    test("\"boo\"", lt("boo"))
    test("def $x := 4 ;", wd("def"), id(0, "x"), pt(":="), lt(4),
        pt(';', Token.EXPLICIT_DELIMITER), ed(Token.IMPLICIT_DELIMITER))
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
    self.assertEquals(2, len(tokens))
    self.assertEquals(expected, tokens[0].value)
    self.assertTrue(tokens[1].has_type(Token.END))

  def test_name(self):
    test = self.check_name
    def nm(phase, *parts):
      return data.Identifier(phase, data.Path(list(parts)))
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

  def test_quote(self):
    test = self.run_test
    test("@ foo", qt(-1), wd("foo"))
    test("@", qt(-1))
    test("@@", qt(-2))
    test("@@@", qt(-3))
    test("$", qt(0))
    test("$$", qt(1))
    test("$$$", qt(2))

  def test_comment(self):
    test = self.run_test
    test("# foo\na\n# bar", wd("a"))
    test("# foo\n a \n# bar", wd("a"))
    test("# foo\n# bar\na \n# baz", wd("a"))

  def test_float(self):
    test = self.run_test
    test('1.5', lt(dec(15, 1, 0)))
    test('2.0', lt(dec(2, 0, 1)))
    test('2.00', lt(dec(2, 0, 2)))
    test('2.00001', lt(dec(200001, 5, 0)))
    test('2.0000000001', lt(dec(20000000001, 10, 0)))
    test('3.1415926', lt(dec(31415926, 7, 0)))
    test('3.14159260', lt(dec(31415926, 7, 1)))

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
