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

  # Returns true if the next token is the specified word token.
  def at_word(self, value):
    return self.at_token(Token.WORD, value)

  # Skips over the current punctuation which must have the specified value,
  # if it's not it raises a syntax error.
  def expect_punctuation(self, value):
    if not self.at_punctuation(value):
      raise self.new_syntax_error()
    self.advance()

  # Skips of the current word which must have the specified value, if it's not
  # a syntax error is raised.
  def expect_word(self, value):
    if not self.at_word(value):
      raise self.new_syntax_error()
    self.advance()

  # <program>
  #   -> <toplevel statement>*
  def parse_program(self):
    elements = []
    while self.has_more():
      (name, value) = self.parse_local_declaration()
      decl = ast.NamespaceDeclaration(name, value)
      elements.append(decl)
    return ast.Program(elements)

  # <expression>
  #   -> <operator expression>
  def parse_expression(self):
    return self.parse_word_expression()

  # <word expression>
  #   -> <lambda>
  def parse_word_expression(self):
    if self.at_word('fn'):
      return self.parse_lambda_expression()
    else:
      return self.parse_operator_expression()

  # <lambda expression>
  #   -> "fn" <arguments> "=>" <word expression>
  def parse_lambda_expression(self):
    self.expect_word('fn')
    params = self.parse_parameters()
    self.expect_punctuation('=>')
    value = self.parse_word_expression()
    return ast.Lambda(params, value)

  # <parameters>
  #   -> "(" <parameter> *: "," ")"
  #   -> .
  def parse_parameters(self):
    if not self.at_punctuation('('):
      return []
    self.expect_punctuation('(')
    result = []
    if not self.at_punctuation(')'):
      index = 0
      first = self.parse_parameter(index)
      index += 1
      result.append(first)
      while self.at_punctuation(','):
        self.expect_punctuation(',')
        next = self.parse_parameter(index)
        index += 1
        result.append(next)
    self.expect_punctuation(')')
    return result

  def parse_parameter(self, default_tag):
    name = self.expect_type(Token.IDENTIFIER)
    return ast.Parameter(name, [default_tag])

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
    recv = self.parse_atomic_expression()
    while self.at_punctuation('('):
      args = [
        ast.Argument('this', recv),
        ast.Argument('name', ast.Literal('()'))
      ]
      self.expect_punctuation('(')
      if not self.at_punctuation(')'):
        arg = self.parse_expression()
        args.append(ast.Argument(0, arg))
        index = 1
        while self.at_punctuation(','):
          self.expect_punctuation(',')
          arg = self.parse_expression()
          args.append(ast.Argument(index, arg))
          index += 1
      self.expect_punctuation(')')
      recv = ast.Invocation(args)
    return recv

  # <atomic expression>
  #   -> <literal>
  #   -> "(" <expression> ")"
  #   -> <array expression>
  #   -> <sequence expression>
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
    elif self.at_punctuation('['):
      return self.parse_array_expression()
    elif self.at_punctuation('{'):
      return self.parse_sequence_expression()
    elif self.at_word('null'):
      return ast.Literal(None)
    elif self.at_word('true'):
      return ast.Literal(True)
    elif self.at_word('false'):
      return ast.Literal(False)
    else:
      raise self.new_syntax_error()

  # <array expression>
  #   -> "[" <expression> *: "," "]"
  def parse_array_expression(self):
    self.expect_punctuation('[')
    elements = []
    if self.at_punctuation(']'):
      elements = []
    else:
      first = self.parse_expression()
      elements.append(first)
      while self.at_punctuation(','):
        self.expect_punctuation(',')
        next = self.parse_expression()
        elements.append(next)
    self.expect_punctuation(']')
    return ast.Array(elements)

  # Raises an error if the current token is not a valid statement delimiter. If
  # the delimiter is explicit consumes it.
  def expect_statement_delimiter(self):
    if not self.has_more():
      raise self.new_syntax_error()
    current = self.current()
    if not current.is_delimiter():
      raise self.new_syntax_error()
    if current.is_explicit_delimiter():
      self.advance()

  # <sequence expression>
  #   -> "{" (<statement> ";")* "}"
  def parse_sequence_expression(self):
    self.expect_punctuation('{')
    statements = self.parse_statement_list()
    self.expect_punctuation('}')
    return ast.Sequence.make(statements)

  # Parses a sequence of statements, returning them as a list.
  def parse_statement_list(self):
    # If we're at the end
    if self.at_punctuation('}'):
      return []
    elif self.at_word('def'):
      (name, value) = self.parse_local_declaration()
      body = ast.Sequence.make(self.parse_statement_list())
      return [ast.LocalDeclaration(name, value, body)]
    else:
      next = self.parse_expression()
      self.expect_statement_delimiter()
      tail = self.parse_statement_list()
      return [next] + tail

  def parse_local_declaration(self):
    self.expect_word('def')
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_punctuation(':=')
    value = self.parse_expression()
    self.expect_statement_delimiter()
    return (name, value)

  # Creates a new syntax error at the current token.
  def new_syntax_error(self):
    return SyntaxError(self.current())


# Exception signalling a syntax error.
class SyntaxError(Exception):

  def __init__(self, token):
    self.token = token

  def __str__(self):
    return "at %s" % self.token.value
