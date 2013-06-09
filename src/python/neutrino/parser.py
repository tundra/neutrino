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

  # <expression>
  #   -> <atomic expression>
  def parse_expression(self):
    return self.parse_atomic_expression()

  # <atomic expression>
  #   -> <literal>
  def parse_atomic_expression(self):
    if self.at_type(Token.LITERAL):
      return self.expect_type(Token.LITERAL)
    else:
      raise self.new_syntax_error()
