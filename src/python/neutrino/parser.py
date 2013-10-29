# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Neutrino parser


import ast
import data
import plankton
from token import Token


# Neutrino parser.
class Parser(object):

  _BUILTIN_METHODSPACE = data.Key("subject", ("core", "builtin_methodspace"))
  _SAUSAGES = data.Operation.call()

  def __init__(self, tokens, module_name=None):
    self.tokens = tokens
    self.cursor = 0
    self.unit = ast.Unit(module_name)
    self.present = self.unit.get_present()

  # Does this parser have more tokens to process?
  def has_more(self):
    return not self.current().has_type(Token.END)

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

  # Returns true if the next token is the specified operation token
  def at_operation(self, value):
    return self.at_token(Token.OPERATION, value)

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

  # Skips over the current operation which must have the specified value, if
  # not a syntax error is raised.
  def expect_operation(self, value):
    if not self.at_operation(value):
      raise self.new_syntax_error()
    self.advance()

  # <program>
  #   -> <toplevel statement>*
  def parse_program(self):
    while self.has_more():
      entry = self.parse_toplevel_statement()
      if entry:
        self.unit.add_element(entry.get_stage(), entry)
    return self.unit

  # <subject>
  #   -> <identifier>
  #   -> "(" <parameter> ")"
  def parse_subject(self):
    if self.at_punctuation('('):
      self.expect_punctuation('(')
      result = self.parse_parameter(data._SUBJECT)
      self.expect_punctuation(')')
      return result
    else:
      return self.expect_type(Token.IDENTIFIER)

  # <toplevel-declaration>
  #   -> "def" <ident> ":=" <value> ";"
  #   -> "def" <subject> <operation> <parameters> "=>" <value> ";"
  def parse_toplevel_declaration(self):
    self.expect_word('def')
    subject = None
    if self.at_type(Token.IDENTIFIER):
      ident = self.current().value
      if not ident.path.is_singular():
        raise self.new_syntax_error()
      name = self.expect_type(Token.IDENTIFIER)
      if self.at_punctuation(':='):
        # Plain definition
        self.expect_punctuation(':=')
        value = self.parse_expression()
        self.expect_statement_delimiter()
        return ast.NamespaceDeclaration(name, value)
      else:
        # First argument to a method
        subject = self.name_as_subject(name)
        if self.at_punctuation('('):
          params = self.parse_parameters()
          body = self.parse_method_tail()
          selector = self.name_as_selector(Parser._SAUSAGES)
          signature = ast.Signature([subject, selector] + params)
          return ast.FunctionDeclaration(ast.Method(signature, body))
    else:
      subject = self.parse_subject()
    name = self.expect_type(Token.OPERATION)
    selector = self.name_as_selector(data.Operation.infix(name))
    params = self.parse_parameters()
    body = self.parse_method_tail()
    signature = ast.Signature([subject, selector] + params)
    return ast.MethodDeclaration(ast.Method(signature, body))

  # Parse the tail of a method, the part after the parameters.
  def parse_method_tail(self):
    if self.at_punctuation('=>'):
      self.expect_punctuation('=>')
      body = self.parse_expression()
      self.expect_statement_delimiter()
      return body
    elif self.at_punctuation('{'):
      return self.parse_sequence_expression()
    else:
      raise self.new_syntax_error()


  # <toplevel-import>
  #   -> "import" <name> ";"
  def parse_toplevel_import(self):
    self.expect_word('import')
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_statement_delimiter()
    return ast.Import(name)

  # <toplevel-statement>
  #   -> <toplevel-declaration>
  #   -> <toplevel-import>
  def parse_toplevel_statement(self):
    if self.at_word('def'):
      return self.parse_toplevel_declaration()
    elif self.at_word('import'):
      return self.parse_toplevel_import()
    elif self.at_word('entry_point'):
      self.expect_word('entry_point')
      self.unit.set_entry_point(self.parse_expression())
      self.expect_statement_delimiter()
      return None
    else:
      raise self.new_syntax_error()

  # <expression>
  #   -> <operator expression>
  def parse_expression(self):
    return self.parse_word_expression()

  # Parses an expression and wraps it in a program appropriately to make it
  # executable.
  def parse_expression_program(self):
    self.unit.set_entry_point(self.parse_word_expression())
    return self.unit

  # <word expression>
  #   -> <lambda>
  def parse_word_expression(self):
    if self.at_word('fn'):
      return self.parse_lambda_expression()
    else:
      return self.parse_operator_expression()

  # <lambda expression>
  #   -> "fn" <parameters> "=>" <word expression>
  #   -> "fn" <parameters> <sequence expression>
  def parse_lambda_expression(self):
    self.expect_word('fn')
    signature = self.parse_signature()
    if self.at_punctuation('{'):
      value = self.parse_sequence_expression()
    else:
      self.expect_punctuation('=>')
      value = self.parse_word_expression()
    return ast.Lambda(ast.Method(signature, value))

  # Are we currently at a token that is allowed as the first token of a
  # parameter?
  def at_parameter_start(self):
    return self.at_type(Token.IDENTIFIER) or self.at_type(Token.TAG)

  # Given a string operation name returns the corresponding selector parameter.
  def name_as_selector(self, name):
    assert isinstance(name, data.Operation)
    return ast.Parameter(data.Identifier(0, data.Path(['name'])), [data._SELECTOR],
      ast.Guard.eq(ast.Literal(name)))

  def name_as_subject(self, name):
    return ast.Parameter(name, [data._SUBJECT], ast.Guard.any())

  # Same as parse_parameters but returns a full signature.
  def parse_signature(self):
    prefix = [
      self.name_as_subject(data.Identifier(0, data.Path(['this']))),
      self.name_as_selector(Parser._SAUSAGES)
    ]
    params = self.parse_parameters()
    return ast.Signature(prefix + params)

  # <parameters>
  #   -> "(" <parameter> *: "," ")"
  #   -> <parameter>
  #   -> .
  def parse_parameters(self):
    if self.at_parameter_start():
      param = self.parse_parameter(0)
      return [param]
    elif not self.at_punctuation('('):
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
    tags = [default_tag]
    if self.at_type(Token.TAG):
      tag = self.expect_type(Token.TAG)
      tags = [tag] + tags
    name = self.expect_type(Token.IDENTIFIER)
    guard = ast.Guard.any()
    if self.at_operation('=='):
      self.expect_operation('==')
      guard_value = self.parse_atomic_expression()
      guard = ast.Guard.eq(guard_value)
    elif self.at_word('is'):
      self.expect_word('is')
      guard_value = self.parse_atomic_expression()
      guard = ast.Guard.is_(guard_value)
    result = ast.Parameter(name, tags, guard)
    return result

  # <operator expression>
  #   -> <call expression> +: <operation>
  def parse_operator_expression(self):
    left = self.parse_unary_expression()
    while self.at_type(Token.OPERATION):
      name = self.expect_type(Token.OPERATION)
      selector = data.Operation.infix(name)
      prefix = [
        ast.Argument(data._SUBJECT, left),
        ast.Argument(data._SELECTOR, ast.Literal(selector))
      ]
      rest = self.parse_arguments()
      left = ast.Invocation(prefix + rest, self.present.get_methodspace())
    return left

  # <arguments>
  #   -> <call expression>
  #   -> "(" <expression> *: "," ")"
  def parse_arguments(self):
    args = []
    if self.at_punctuation('('):
      self.expect_punctuation('(')
      if not self.at_punctuation(')'):
        arg = self.parse_argument(0)
        args.append(arg)
        index = 1
        while self.at_punctuation(','):
          self.expect_punctuation(',')
          arg = self.parse_argument(index)
          args.append(arg)
          index += 1
      self.expect_punctuation(')')
    else:
      arg = self.parse_unary_expression()
      args.append(ast.Argument(0, arg))
    return args

  # <argument>
  #   -> <tag>? <expression
  def parse_argument(self, default_tag):
    if self.at_type(Token.TAG):
      tag = self.expect_type(Token.TAG)
    else:
      tag = default_tag
    value = self.parse_expression()
    return ast.Argument(tag, value)

  # <unary expression>
  #   -> <prefix>* <atomic expression>
  def parse_unary_expression(self):
    if self.at_type(Token.OPERATION):
      name = self.expect_type(Token.OPERATION)
      selector = data.Operation.prefix(name)
      subject = self.parse_unary_expression()
      args = [
        ast.Argument(data._SUBJECT, subject),
        ast.Argument(data._SELECTOR, ast.Literal(selector))
      ]
      return ast.Invocation(args, self.present.get_methodspace())
    else:
      return self.parse_call_expression()

  # <call expression>
  #   -> <atomic expression>
  def parse_call_expression(self):
    recv = self.parse_atomic_expression()
    while self.at_punctuation('('):
      prefix = [
        ast.Argument(data._SUBJECT, recv),
        ast.Argument(data._SELECTOR, ast.Literal(Parser._SAUSAGES))
      ]
      rest = self.parse_arguments()
      recv = ast.Invocation(prefix + rest, self.present.get_methodspace())
    return recv

  # <variable>
  #   -> <identifier>
  def parse_variable(self):
    ident = self.expect_type(Token.IDENTIFIER)
    stage_index = ident.stage
    stage = self.unit.get_or_create_stage(stage_index)
    namespace = stage.get_namespace()
    variable = ast.Variable(ident=ident, namespace=namespace)
    if ident.stage < 0:
      return ast.Quote(stage_index, variable)
    else:
      return variable


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
      return self.parse_variable()
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
      self.expect_word('null')
      return ast.Literal(None)
    elif self.at_word('true'):
      self.expect_word('true')
      return ast.Literal(True)
    elif self.at_word('false'):
      self.expect_word('false')
      return ast.Literal(False)
    elif self.at_type(Token.QUOTE):
      return self.parse_quote()
    else:
      raise self.new_syntax_error()

  def parse_quote(self):
    stage = self.expect_type(Token.QUOTE)
    self.expect_punctuation('(')
    value = self.parse_expression()
    self.expect_punctuation(')')
    return ast.Quote(stage, value)

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
    # Don't check explicitly for has-more since we may be at the end where the
    # end token is an implicit delimiter.
    current = self.current()
    if not current.is_delimiter():
      raise self.new_syntax_error()
    # Advance over any explicit delimiters but only if it's not the end marker.
    if self.has_more() and current.is_explicit_delimiter():
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
