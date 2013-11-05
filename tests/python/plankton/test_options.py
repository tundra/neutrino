#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from plankton import options
import unittest


a = options.ArgumentElement
f = options.FlagElement
N = None


def o(*elms):
  return options.Options(elements=elms)


class OptionsTest(unittest.TestCase):

  def run_element_test(self, expected, *args):
    result = options.parse(args)
    self.assertEquals(expected, result.elements)

  def test_expressions(self):
    test = self.run_element_test
    # Numbers
    test([], '')
    test([a(100)], '100')
    test([a(0)], '0')
    test([a(0), a(1), a(2)], '0', '1', '2')
    test([a(0), a(1), a(2)], '0 1 2')
    # Singletons
    test([a(None)], 'null')
    test([a(True)], 'true')
    test([a(False)], 'false')
    # Parens
    test([a(0), a(1), a(2)], '0', '(1)', '((2))')
    test([a(0), a(1), a(2)], '((0)) (1) 2')
    # Quoted strings
    test([a('')], '""')
    test([a('foo')], '"foo"')
    test([a('foo'), a('bar'), a('baz')], '"foo""bar""baz"')
    test([a('foo'), a('bar'), a('baz')], '"foo"', '"bar"', '"baz"')
    test([a('foo bar baz')], '"foo bar baz"')
    # Symbols
    test([a('foo')], 'foo')
    test([a('foo'), a('bar'), a('baz')], 'foo bar baz')
    test([a('foo'), a('bar'), a('baz')], 'foo', 'bar', 'baz')
    test([a('foo-bar-baz')], 'foo-bar-baz')
    test([a('foo_bar_baz')], 'foo_bar_baz')
    test([a('foo_123_baz')], 'foo_123_baz')
    # Arrays
    test([a([])], '[]')
    test([a([1])], '[1]')
    test([a([1, 2])], '[1 2]')
    test([a([1, 2, 3])], '[1 2 3]')
    test([a([1, [2, 3], 4])], '[1 [2 3] 4]')
    test([a([])], '[', ']')
    test([a([1])], '[', '1', ']')
    test([a([1, 2])], '[', '1', '2', ']')
    test([a([1, 2, 3])], '[', '1 2 3', ']')
    test([a([1, [2, 3], 4])], '[', '1', '[', '2', '3', ']', '4', ']')
    # Maps
    test([a({})], '{}')
    test([a({"a": 4})], '{--a 4}')
    test([a({"a": N})], '{--a}')
    test([a({"a": N, "b": N})], '{--a --b}')
    test([a({"a": "--b"})], '{--a "--b"}')
    test([a({"a": 4})], '{--(a) 4}')
    test([a({"a b c": 4})], '{--"a b c" 4}')
    test([a({"--abc": 4})], '{--"--abc" 4}')
    test([a({"a b c": N})], '{--"a b c"}')
    test([a({"a-b-c": 4})], '{--a-b-c 4}')
    test([a({"a": 4, "b": 5, "c": 6})], '{--a 4 --b 5 --c 6}')
    test([a({"a": 4, "b": 5, "c": 6})], '{', '--a 4', '--b 5', '--c 6', '}')
    test([a({"a": 4, "b": 5, "c": 6})], '{', '--a', '4', '--b', '5', '--c', '6', '}')
    test([a({"a": 4, "b": 5, "c": 6})], '{', '--', 'a', '4', '--', 'b', '5', '--' 'c', '6', '}')
    test([a({"a": [4, 5, 6]})], '{--a [4 5 6]}')
    test([a({"a": [4, 5, 6]})], '{--a[ 4 5 6 ]}')
    test([a({"a": [4, 5, 6]})], '{', '--a[', '4', '5', '6', ']', '}')

  def test_nested(self):
    test = self.run_element_test
    test([a(o())], '{{}}')
    test([a(o(f('foo', 'bar')))], '{{--foo bar}}')
    test([a(o(f('foo', o(a(1), a(2), a(3)))))], '{{--foo {{1 2 3}}}}')
    test([a(o(f('foo', N), f('bar', N), f('baz', N)))], '{{ --foo --bar --baz }}')

  def test_options(self):
    test = self.run_element_test
    test(
      [f('foo', 'bar')],
      '--foo bar')
    test(
      [f('foo', 'bar'), f('baz', 'quux')],
      '--foo bar --baz quux')
    test(
      [f('foo', 'bar'), f('baz', 'quux')],
      '--foo', 'bar', '--baz', 'quux')
    test(
      [f('foo', N), f('bar', N), f('baz', 'quux')],
      '--foo', '--bar', '--baz', 'quux')
    test(
      [f('foo', N), f('bar', N), f('baz', 'quux')],
      '--foo --bar --baz quux')
    test(
      [a('a'), a('b'), f('foo', 'c'), a('d'), f('bar', 'e'), a('f')],
      'a b --foo c d --bar e f')


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
