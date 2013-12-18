#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Utilities for parsing command-line options into plankton.


from . import codec
import collections
import re
import sys


# Parsing failed.
class OptionSyntaxError(Exception):

  def __init__(self, token):
    super(OptionSyntaxError, self).__init__(token)
    self.token = token


# An individual command-line argument token.
class Token(object):

  _VALUE = 'value'
  _PUNCTUATION = 'punctuation'
  _END = 'end'

  def __init__(self, type, value):
    self.type = type
    self.value = value

  # Is this an atomic value?
  def is_value(self):
    return self.type == Token._VALUE

  # Is this the specified punctuation value?
  def is_punctuation(self, value):
    return self.is_any_punctuation() and (self.value == value)

  # Is this some punctuation?
  def is_any_punctuation(self):
    return self.type == Token._PUNCTUATION

  # Is this the end token?
  def is_end(self):
    return self.type == Token._END


# Basic support for scanning over a stream of elements, in this case characters
# in a string or tokens in a list of tokens.
class AbstractScanner(object):

  def __init__(self, elements):
    self.elements = elements
    self.cursor = 0

  # Is there more input?
  def has_more(self):
    return self.cursor < len(self.elements)

  # Returns the current character.
  def current(self):
    return self.elements[self.cursor]

  # Advances one character ahead.
  def advance(self):
    assert self.has_more()
    self.cursor += 1

  # Returns a subset of the elements from 'start' to 'end'.
  def slice(self, start, end):
    return self.elements[start:end]


# Tokenizer that chops raw command-line source code (individual options and
# arguments) into tokens.
class Tokenizer(AbstractScanner):

  _PUNCTUATORS = '[],():'
  _DOUBLE_PUNCTUATORS = '-{}'

  def __init__(self, source):
    super(Tokenizer, self).__init__(source)
    self.skip_whitespace()

  # Skips over any whitespace characters from the current position.
  def skip_whitespace(self):
    while self.has_more() and self.is_whitespace(self.current()):
      self.advance()

  _WHITESPACE = re.compile(r'\s')
  # Is the given character whitespace?
  def is_whitespace(self, c):
    return Tokenizer._WHITESPACE.match(c)

  _SYMBOL_EXTRA_CHARS = '_-/.'

  def is_symbol(self, c):
    return c.isalpha() or c.isdigit() or (c in Tokenizer._SYMBOL_EXTRA_CHARS)

  # Returns the next token from the input, advancing past it.
  def scan_next_token(self):
    assert self.has_more()
    current = self.current()
    if current.isdigit():
      result = self.scan_next_number()
    elif current == '"':
      result = self.scan_next_string()
    elif current in Tokenizer._PUNCTUATORS:
      result = Token(Token._PUNCTUATION, current)
      self.advance()
    elif current in Tokenizer._DOUBLE_PUNCTUATORS:
      result = self.scan_next_double_punctuator()
    elif self.is_symbol(current):
      result = self.scan_next_symbol()
    else:
      raise self.new_syntax_error()
    self.skip_whitespace()
    return result

  # Returns the next token which must be a number.
  def scan_next_number(self):
    start = self.cursor
    while self.has_more() and self.current().isdigit():
      self.advance()
    value = self.slice(start, self.cursor)
    return Token(Token._VALUE, int(value))

  _RESERVED_SYMBOLS = {
    'null': None,
    'true': True,
    'false': False
  }

  # Returns the next token which must be a symbol.
  def scan_next_symbol(self):
    start = self.cursor
    while self.has_more() and self.is_symbol(self.current()):
      self.advance()
    value = self.slice(start, self.cursor)
    if value in Tokenizer._RESERVED_SYMBOLS:
      value = Tokenizer._RESERVED_SYMBOLS[value]
    return Token(Token._VALUE, value)

  # Returns the next token which must be a string.
  def scan_next_string(self):
    assert self.current() == '"'
    self.advance()
    start = self.cursor
    while self.has_more() and self.current() != '"':
      self.advance()
    if not self.has_more():
      raise self.new_syntax_error()
    end = self.cursor
    assert self.current() == '"'
    self.advance()
    value = self.slice(start, end)
    return Token(Token._VALUE, value)

  def scan_next_double_punctuator(self):
    c = self.current()
    self.advance()
    if self.has_more() and self.current() == c:
      self.advance()
      return Token(Token._PUNCTUATION, c + c)
    else:
      return Token(Token._PUNCTUATION, c)

  def new_syntax_error(self):
    current = self.current()
    return OptionSyntaxError(current)

  # Returns a list of the tokens from the given source string.
  @staticmethod
  def tokenize(source):
    tokenizer = Tokenizer(source)
    result = []
    while tokenizer.has_more():
      result.append(tokenizer.scan_next_token())
    return result


def value_to_string(value):
  if isinstance(value, collections.OrderedDict):
    mappings = ['--%s %s' % (value_to_string(k), value_to_string(v))
      for (k, v) in value.items()]
    return '{%s}' % ' '.join(mappings)
  elif isinstance(value, list):
    return '[%s]' % ' '.join(map(value_to_string, value))
  else:
    return str(value)


# An individual option element representing an argument (as opposed to a flag).
@codec.serializable(codec.EnvironmentReference.path('options', 'ArgumentElement'))
class ArgumentElement(object):

  @codec.field('value')
  def __init__(self, value):
    self.value = value

  def apply(self, options):
    options.arguments.append(self.value)

  def __eq__(self, that):
    if not isinstance(that, ArgumentElement):
      return False
    return self.value == that.value

  def __hash__(self):
    return hash(self.value)

  def __str__(self):
    return '%s' % value_to_string(self.value)


@codec.serializable(codec.EnvironmentReference.path('options', 'FlagElement'))
class FlagElement(object):

  @codec.field('key')
  @codec.field('value')
  def __init__(self, key, value):
    self.key = key
    self.value = value

  def apply(self, options):
    options.flags[self.key] = self.value

  def __eq__(self, that):
    if not isinstance(that, FlagElement):
      return False
    return (self.key == that.key) and (self.value == that.value)

  def __hash__(self):
    return hash(self.key) ^ id(self.value)

  def __str__(self):
    if self.value is None:
      return '--%s' % value_to_string(self.key)
    else:
      return '--%s %s' % (value_to_string(self.key), value_to_string(self.value))


# A collection of command-line options. An options object provides access to
# the arguments as a list, the flags by name, and a more naked view of the
# options as a list of elements, each either an argument or a flag.
@codec.serializable(codec.EnvironmentReference.path('options', 'Options'))
class Options(object):

  @codec.field('elements')
  def __init__(self, elements=None):
    if elements is None:
      self.elements = []
    else:
      self.elements = elements

  def add_argument(self, value):
    self.elements.append(ArgumentElement(value))

  def add_flag(self, key, value):
    self.elements.append(FlagElement(key, value))

  def get_flags(self):
    self.ensure_processed()
    return self.flags_proxy

  def get_flag_map(self):
    self.ensure_processed()
    return self.flags

  def get_arguments(self):
    self.ensure_processed()
    return self.arguments

  def ensure_processed(self):
    if hasattr(self, 'has_been_processed') and self.has_been_processed:
      return
    self.has_been_processed = True
    outer = self
    class FlagsProxy(object):
      def __getattr__(self, name):
        return self.get(name, None)
      def get(self, name, default=None):
        return outer.get_flag_value(name, default)
      def __contains__(self, name):
        return name in outer.flags
    self.flags = collections.OrderedDict()
    self.flags_proxy = FlagsProxy()
    self.arguments = []
    for element in self.elements:
      element.apply(self)

  def get_flag_value(self, name, default):
    if name in self.flags:
      return self.flags[name]
    elif ('_' in name):
      return self.get_flag_value(name.replace('_', '-'), default)
    else:
      return default


  # Encodes this option set as base64 plankton.
  def base64_encode(self):
    encoder = codec.Encoder()
    return "p64/%s" % encoder.base64encode(self)

  # Parses a base64 string into an options object.
  @staticmethod
  def base64_decode(string):
    if string.startswith('p64/'):
      decoder = codec.Decoder()
      return decoder.base64decode(string[4:])
    else:
      return string

  def __str__(self):
    return '{{%s}}' % " ".join(map(str, self.elements))

  def __eq__(self, that):
    if not isinstance(that, Options):
      return False
    if len(self.elements) != len(that.elements):
      return False
    for i in range(0, len(self.elements)):
      if not (self.elements[i] == that.elements[i]):
        return False
    return True

  def __hash__(self):
    return hash(tuple(self.elements))


# Command-line option parser.
class Parser(AbstractScanner):

  def __init__(self, tokens):
    super(Parser, self).__init__(tokens)

    # Is there more input?
  def has_more(self):
    return not self.current().is_end()

  def expect_punctuation(self, value):
    if not self.current().is_punctuation(value):
      raise self.new_syntax_error()
    self.advance()

  # <options>
  #   -> <option>*
  def parse_options(self):
    options = Options()
    while self.has_more() and not self.current().is_punctuation('}}'):
      self.parse_option(options)
    return options

  # <nested options>
  #   -> "{{" <options> "}}"
  def parse_nested_options(self):
    self.expect_punctuation('{{')
    result = self.parse_options()
    self.expect_punctuation('}}')
    return result


  # <option>
  #   -> <expression>
  #   -> <map entry>
  def parse_option(self, options):
    current = self.current()
    if current.is_punctuation('--'):
      (key, value) = self.parse_map_entry()
      options.add_flag(key, value)
    else:
      options.add_argument(self.parse_expression())

  # <expression>
  #   -> <atomic expression>
  def parse_expression(self):
    return self.parse_atomic_expression()

  # Are we at the beginning of an atomic expression?
  def at_atomic_expression(self):
    current = self.current()
    if current.is_value():
      return True
    if not current.is_any_punctuation():
      return False
    return current.value in ['[', '{', '(', '{{']

  # <atomic expression>
  #   -> <value>
  #   -> <list>
  #   -> <nested options>
  #   -> "(" <expression> ")"
  def parse_atomic_expression(self):
    current = self.current()
    if current.is_value():
      return self.parse_value()
    elif current.is_punctuation('['):
      return self.parse_list()
    elif current.is_punctuation('{'):
      return self.parse_map()
    elif current.is_punctuation('{{'):
      return self.parse_nested_options()
    elif current.is_punctuation('('):
      self.expect_punctuation('(')
      result = self.parse_expression()
      self.expect_punctuation(')')
      return result
    else:
      raise self.new_syntax_error()

  # <value>
  def parse_value(self):
    assert self.current().is_value()
    value = self.current().value
    self.advance()
    return value

  # <list>
  #   -> "[" <atomic expression>* "]"
  def parse_list(self):
    self.expect_punctuation('[')
    result = []
    while not self.current().is_punctuation(']'):
      next = self.parse_atomic_expression()
      result.append(next)
    self.expect_punctuation(']')
    return result

  # <map>
  #   -> "{" <map entry>* "}"
  def parse_map(self):
    self.expect_punctuation('{')
    result = collections.OrderedDict()
    while not self.current().is_punctuation('}'):
      (key, value) = self.parse_map_entry()
      result[key] = value
    self.expect_punctuation('}')
    return result

  # <map entry>
  #   -> <key> <atomic expression>?
  def parse_map_entry(self):
    key = self.parse_key()
    if self.at_atomic_expression():
      value = self.parse_atomic_expression()
    else:
      value = None
    return (key, value)

  # <key>
  #   -> "--" <atomic expression>
  def parse_key(self):
    self.expect_punctuation('--')
    return self.parse_atomic_expression()

  def new_syntax_error(self):
    return OptionSyntaxError(self.current().value)


# Tokenizes the given list of arguments.
def tokenize_arguments(args):
  tokens = []
  for arg in args:
    tokens += Tokenizer.tokenize(arg)
  tokens.append(Token(Token._END, None))
  return tokens


# Parses the given arguments, returning an Options object describing the
# contents.
def parse(args):
  tokens = tokenize_arguments(args)
  return Parser(tokens).parse_options()
