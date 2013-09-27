# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Tokens and tokenization.


import ast
import re


# A source token. A token has a type, a value (which may be a string or some
# other type of value), and a delimiter status which specifies whether this
# token acts as a statement delimiter. The delimiter stuff is used to allow
# expressions that end with '}' to not be followed by a ';'; instead they
# (and semis too) cause the next token to be marked as a legal place to end a
# statement.
class Token(object):

  # Naked word: 'foo', 'if', ...
  WORD = 'word'

  # Punctuation: '(', '}', ...
  PUNCTUATION = 'punctuation'

  # Operations: '+', '.foo', '*', ...
  OPERATION = 'operation'

  # Keyword tags: 'foo:', 'count:', '0:', ...
  TAG = 'tag'

  # Literal values: '"foo"', '10', ...
  LITERAL = 'literal'

  # Structured identifier: '$foo', '@bar:bar', ...
  IDENTIFIER = 'identifier'

  # Unexpected input.
  ERROR = 'error'

  # This token does not act as a delimiter in any way.
  NO_DELIMITER = {'is_delimiter': False, 'is_explicit': False}

  # This token acts as a delimiter but is not a delimiter in itself.
  IMPLICIT_DELIMITER = {'is_delimiter': True, 'is_explicit': False}

  # This token has no purpose but to act as a delimiter.
  EXPLICIT_DELIMITER = {'is_delimiter': True, 'is_explicit': True}

  def __init__(self, type, value, delimiter):
    self.type = type
    self.value = value
    self.delimiter = delimiter

  # Is this token of the given type?
  def has_type(self, type):
    return self.type == type

  # Returns this token's value.
  def get_value(self):
    return self.value

  # Returns true iff this token is valid as a statement delimiter.
  def is_delimiter(self):
    return self.delimiter['is_delimiter']

  # Returns true iff this statement delimiter token is explicit and should be
  # consumed when expecting a delimiter.
  def is_explicit_delimiter(self):
    return self.delimiter['is_explicit']

  # Just for debugging.
  def __str__(self):
    return "%s(%s)" % (self.type, self.value)

  def __eq__(self, that):
    return self.type == that.type and self.value == that.value and self.delimiter == that.delimiter

  @staticmethod
  def word(value, delimiter=NO_DELIMITER):
    return Token(Token.WORD, value, delimiter)

  @staticmethod
  def punctuation(value, delimiter=NO_DELIMITER):
    return Token(Token.PUNCTUATION, value, delimiter)

  @staticmethod
  def operation(value, delimiter=NO_DELIMITER):
    return Token(Token.OPERATION, value, delimiter)

  @staticmethod
  def tag(value, delimiter=NO_DELIMITER):
    return Token(Token.TAG, value, delimiter)

  @staticmethod
  def literal(value, delimiter=NO_DELIMITER):
    return Token(Token.LITERAL, value, delimiter)

  @staticmethod
  def identifier(value, delimiter=NO_DELIMITER):
    return Token(Token.IDENTIFIER, value, delimiter)

  @staticmethod
  def error(value, delimiter=NO_DELIMITER):
    return Token(Token.ERROR, value, delimiter)


# Keeps track of state while parsing input.
class Tokenizer(object):

  # Punctuation characters.
  _PUNCTUATION = "(){}[];,"

  # Operator characters.
  _OPERATORS = "+-<>%*/=:"

  def __init__(self, source):
    self.source = source
    self.cursor = 0
    self.skip_whitespace()
    self.next_delimiter = Token.NO_DELIMITER

  # Are there more tokens to be read from this scanner?
  def has_more(self):
    return self.cursor < len(self.source)

  # Yields the current character.
  def current(self):
    return self.source[self.cursor]

  # Skips ahead to the next character
  def advance(self):
    assert self.has_more()
    self.cursor += 1

  # Returns a substring of the source from start to but not including end.
  def slice(self, start, end):
    return self.source[start:end]

  # Skips over any whitespace characters from the current position.
  def skip_whitespace(self):
    while self.has_more() and self.is_whitespace(self.current()):
      self.advance()

  _WHITESPACE = re.compile(r'\s')
  # Is the given character whitespace?
  def is_whitespace(self, c):
    return Tokenizer._WHITESPACE.match(c)

  # Is the given character legal as part of an identifier part
  def is_alpha(self, c):
    return c.isalpha() or (c == '_')

  # Is the given character an operator character.
  def is_operator(self, c):
    return c in Tokenizer._OPERATORS

  # Is the given character punctuation?
  def is_punctuation(self, c):
    return c in Tokenizer._PUNCTUATION

  # Is the given character legal as part of a number?
  def is_number_part(self, c):
    return c.isdigit()

  # Reads and returns the next token.
  def scan_token(self):
    c = self.current()
    delim = self.next_delimiter
    self.next_delimiter = Token.NO_DELIMITER
    if self.is_alpha(c):
      result = self.scan_word_or_tag(delim)
    elif (c == '$') or (c == '@'):
      result = self.scan_identifier(delim)
    elif c == '.':
      result = self.scan_named_operation(delim)
    elif self.is_operator(c):
      result = self.scan_operator(delim)
    elif self.is_punctuation(c):
      result = self.yield_punctuation(c, delim)
      self.advance()
    elif c.isdigit():
      result = self.scan_number_or_tag(delim)
    elif c == '"':
      result = self.scan_string(delim)
    else:
      result = Token.error(c, delim)
      self.advance()
    self.skip_whitespace()
    return result

  # Scans over the next word or string-valued tag.
  def scan_word_or_tag(self, delim):
    start = self.cursor
    while self.has_more() and self.is_alpha(self.current()):
      self.advance()
    end = self.cursor
    if self.has_more() and self.current() == ':':
      self.advance()
      cons = Token.tag
    else:
      cons = Token.word
    value = self.slice(start, end)
    return cons(value, delim)

  # Scans over the next structured identifier, semis and all.
  def scan_identifier(self, delim):
    if self.current() == '$':
      phase = 0
      direction = 1
      char = '$'
    else:
      assert self.current() == '@'
      phase = -1
      direction = -1
      char = '@'
    self.advance()
    while self.current() == char:
      phase += direction
      self.advance()
    start = self.cursor
    while self.has_more() and (self.is_alpha(self.current()) or self.current() == ':'):
      self.advance()
    end = self.cursor
    value = self.slice(start, end).split(':')
    return Token.identifier(ast.Name(phase, value), delim)

  # Is this a character that is allowed to follow a .?
  def is_named_operator_char(self, value):
    return self.is_alpha(value) or self.is_operator(value)

  # Scans over the next named operation.
  def scan_named_operation(self, delim):
    assert self.current() == '.'
    self.advance()
    start = self.cursor
    while self.has_more() and self.is_named_operator_char(self.current()):
      self.advance()
    end = self.cursor
    value = self.slice(start, end)
    return Token.operation(value, delim)

  # Scans over the next special operator.
  def scan_operator(self, delim):
    start = self.cursor
    while self.has_more() and self.is_operator(self.current()):
      self.advance()
    end = self.cursor
    value = self.slice(start, end)
    if value == ':=' or value == '=>':
      return Token.punctuation(value, delim)
    else:
      return Token.operation(value, delim)

  # Scans over a literal number or a number-valued tag.
  def scan_number_or_tag(self, delim):
    start = self.cursor
    while self.has_more() and self.is_number_part(self.current()):
      self.advance()
    end = self.cursor
    value = int(self.slice(start, end))
    if self.has_more() and self.current() == ':':
      self.advance()
      cons = Token.tag
    else:
      cons = Token.literal
    return cons(value, delim)

  # Scans over the next string.
  def scan_string(self, delim):
    start = self.cursor
    self.advance()
    while self.has_more() and self.current() != '"':
      self.advance()
    if self.has_more():
      self.advance()
    end = self.cursor
    value = self.slice(start + 1, end - 1)
    return Token.literal(value, delim)

  # Returns a punctuation token for the specified value and update the internal
  # state of the tokenizer to reflect that we've just seen that token.
  def yield_punctuation(self, value, delim):
    if value == '}' or value == ';':
      # Mark the next token as one where we'll allow a new statement.
      self.next_delimiter = Token.IMPLICIT_DELIMITER
    if value == ';':
      # Override the default next delimiter.
      delim = Token.EXPLICIT_DELIMITER
    return Token.punctuation(value, delim)


# Tokenizes the given input, returning a list of tokens.
def tokenize(input):
  tokenizer = Tokenizer(input)
  result = []
  while tokenizer.has_more():
    result.append(tokenizer.scan_token())
  return result
