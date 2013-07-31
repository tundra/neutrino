# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Neutrino parser


import ast
from token import Token


# Neutrino parser.
class Parser(object):

  def __init__(self, tokens):
    self.tokens = tokens
    self.cursor = 0

  # Does this parser have more tokens to process?
  def has_more(self):
    return self.cursor < len(self.tokens)

  # Advances to the next token.
  def advance(self):
    self.cursor += 1

  # Returns the current token.
  def current(self):
    return self.tokens[self.cursor]

  # Advances the parser over the next token which must be of the given type and
  # returns the value of that token. If the token is not of the required type
  # a syntax error is thrown.
  def expect_type(self, type):
    if not self.at_type(type):
      raise self.new_syntax_error()
    value = self.current().get_value()
    self.advance()
    return value

  # Is the current token of the given type?
  def at_type(self, type):
    return self.has_more() and self.current().has_type(type)

  # Returns true if we're currently at the given token type with the given
  # value.
  def at_token(self, type, value):
    if not self.has_more():
      return False
    current = self.current()
    return current.has_type(type) and current.get_value() == value

  # Returns true if the next token is the specified punctuation.
  def at_punctuation(self, value):
    return self.at_token(Token.PUNCTUATION, value)

  # Skips over the current punctuation which must have the specified value,
  # if it's not it raises a syntax error.
  def expect_punctuation(self, value):
    if not self.at_punctuation(value):
      raise self.new_syntax_error()
    self.advance()

  # <expression>
  #   -> <operator expression>
  def parse_expression(self):
    return self.parse_operator_expression()

  # <operator expression>
  #   -> <call expression> +: <operation>
  def parse_operator_expression(self):
    left = self.parse_call_expression()
    while self.at_type(Token.OPERATION):
      name = self.expect_type(Token.OPERATION)
      right = self.parse_call_expression()
      left = ast.Invocation([
        ast.Argument('this', left),
        ast.Argument('name', ast.Literal(name)),
        ast.Argument(0, right)
      ])
    return left

  # <call expression>
  #   -> <atomic expression>
  def parse_call_expression(self):
    return self.parse_atomic_expression()

  # <atomic expression>
  #   -> <literal>
  def parse_atomic_expression(self):
    if self.at_type(Token.LITERAL):
      value = self.expect_type(Token.LITERAL)
      return ast.Literal(value)
    elif self.at_type(Token.IDENTIFIER):
      name = self.expect_type(Token.IDENTIFIER)
      return ast.Variable(name=name)
    elif self.at_punctuation('('):
      self.expect_punctuation('(')
      result = self.parse_expression()
      self.expect_punctuation(')')
      return result
    else:
      raise self.new_syntax_error()

  # Creates a new syntax error at the current token.
  def new_syntax_error(self):
    return SyntaxError(self.current())


# Exception signalling a syntax error.
class SyntaxError(Exception):

  def __init__(self, token):
    self.token = token

  def __str__(self):
    return "at %s" % self.token.value
