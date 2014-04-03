# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Neutrino parser


from abc import abstractmethod, ABCMeta
import ast
import data
import plankton
from token import Token


# Dispenses default tags for argument parsing.
class TagDispenser(object):

  def __init__(self):
    self.current = 0

  def get_next(self):
    result = self.current
    self.current += 1
    return result


# Neutrino parser.
class Parser(object):

  _SAUSAGES = data.Operation.call()
  _SQUARE_SAUSAGES = data.Operation.index()
  _FOR = data.Operation.infix("for")
  _NEW = data.Operation.infix("new")

  def __init__(self, tokens, module):
    self.tokens = tokens
    self.cursor = 0
    self.module = module
    self.default_subject_guard = ast.Guard.any()

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
        self.module.add_element(entry)
    return self.module

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
      return self.parse_parameter(data._SUBJECT)

  # Returns the stage of the given subject parameter. For instance, the stage
  # of ($this is @String) is -1 because the stage of @String is -1.
  def get_subject_stage(self, subject):
    guard = subject.guard
    if guard.type != data.Guard._IS:
      return -1
    value = guard.value
    if not isinstance(value, ast.Variable):
      return -1
    return value.ident.stage

  # <toplevel-declaration>
  #   -> "def" <ident> ":=" <value> ";"
  #   -> "def" <subject> <operation> <parameters> "=>" <value> ";"
  #   -> "def" "type" <atomic> "is" <atomic> ";"
  def parse_toplevel_declaration(self, annots):
    self.expect_word('def')
    subject = None
    is_prefix = False
    name = None
    allow_extra = False
    if self.at_type(Token.IDENTIFIER):
      # def <ident>
      ident = self.current().value
      name = self.expect_type(Token.IDENTIFIER)
      if self.at_punctuation(':='):
        # def <ident> :=
        self.expect_punctuation(':=')
        value = self.parse_expression(True)
        return ast.NamespaceDeclaration(annots, name, value)
      else:
        if self.at_punctuation('('):
          # def <ident> (
          subject = ast.Parameter(name, [data._SUBJECT], ast.Guard.eq(ast.Variable(name.shift_back())))
          (params, operation, allow_extra) = self.parse_parameters(Parser._SAUSAGES)
          body = self.parse_method_tail(True)
          selector = self.name_as_selector(operation)
          signature = ast.Signature([subject, selector] + params, allow_extra)
          return ast.FunctionDeclaration(name, ast.Method(signature, body))
        else:
          # def <ident> ...
          subject = ast.Parameter(name, [data._SUBJECT], ast.Guard.any())
    elif self.at_word('type'):
      # def type <atomic> is <atomic>
      self.expect_word('type')
      subtype = self.parse_atomic_expression()
      self.expect_word('is')
      supertype = self.parse_atomic_expression()
      self.expect_statement_delimiter(True)
      return ast.IsDeclaration(subtype, supertype)
    elif self.at_type(Token.OPERATION):
      # def <operation> ...
      name = self.expect_type(Token.OPERATION)
      subject = self.parse_subject()
      is_prefix = True
    else:
      # def (<parameter>)
      subject = self.parse_subject()
    signature = self.parse_functino_signature([subject], is_prefix, name)
    body = self.parse_method_tail(True)
    stage = self.get_subject_stage(subject)
    return ast.MethodDeclaration(stage + 1, annots, ast.Method(signature, body))

  # Parse the tail of a method, the part after the parameters.
  def parse_method_tail(self, expect_delim):
    if self.at_punctuation('=>'):
      self.expect_punctuation('=>')
      body = self.parse_expression(False)
    elif self.at_punctuation('{'):
      body = self.parse_sequence_expression()
    elif self.at_punctuation(';'):
      body = ast.Literal(None)
    else:
      raise self.new_syntax_error()
    self.expect_statement_delimiter(expect_delim)
    return body


  # <toplevel-import>
  #   -> "import" <name> ";"
  def parse_toplevel_import(self):
    self.expect_word('import')
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_statement_delimiter(True)
    return ast.Import(name)

  # <toplevel-statement>
  #   -> <annotations> <toplevel-declaration>
  #   -> <annotations> <toplevel-import>
  #   -> <annotations> <toplevel-type-declaration>
  def parse_toplevel_statement(self):
    annots = self.parse_annotations()
    if self.at_word('def'):
      return self.parse_toplevel_declaration(annots)
    elif self.at_word('field'):
      return self.parse_field_declaration()
    elif self.at_word('import'):
      return self.parse_toplevel_import()
    elif self.at_word('do'):
      self.expect_word('do')
      self.module.set_entry_point(self.parse_expression(True))
      return None
    elif self.at_word('type'):
      return self.parse_type_declaration()
    else:
      raise self.new_syntax_error()

  # <annotations>
  #   -> <atomic expression>*
  def parse_annotations(self):
    annots = []
    while self.at_atomic_start():
      annot = self.parse_operator_expression()
      annots.append(annot)
    return annots

  # <expression>
  #   -> <word expression>
  def parse_expression(self, expect_delim):
    return self.parse_word_expression(expect_delim)

  # Parses an expression and wraps it in a program appropriately to make it
  # executable.
  def parse_expression_program(self):
    self.module.set_entry_point(self.parse_word_expression(False))
    return self.module

  # <word expression>
  #   -> <lambda>
  #   -> <assignment expression>
  def parse_word_expression(self, expect_delim):
    if self.at_word('fn'):
      return self.parse_lambda_expression()
    elif self.at_word('bk'):
      return self.parse_block_expression(expect_delim)
    elif self.at_word('new'):
      return self.parse_new_expression(expect_delim)
    elif self.at_word('if'):
      return self.parse_if_expression(expect_delim)
    elif self.at_word('while'):
      return self.parse_while_expression(expect_delim)
    elif self.at_word('for'):
      return self.parse_for_expression(expect_delim)
    elif self.at_word('with_escape'):
      return self.parse_with_escape_expression(expect_delim)
    elif self.at_word('leave') or self.at_word('signal'):
      return self.parse_signal_expression(expect_delim)
    elif self.at_word('try'):
      return self.parse_try_expression(expect_delim)
    elif self.at_word('op'):
      return self.parse_naked_selector(expect_delim)
    elif self.at_word('call'):
      return self.parse_call_literal(expect_delim)
    else:
      return self.parse_assignment_expression(expect_delim)

  # <naked selector>
  #   -> "op" <selector> ("(" ")")?
  def parse_naked_selector(self, expect_delim):
    self.expect_word('op')
    if self.at_type(Token.OPERATION):
      value = self.expect_type(Token.OPERATION)
      if self.at_punctuation('('):
        self.expect_punctuation('(')
        self.expect_punctuation(')')
        op = data.Operation.infix(value)
      else:
        op = data.Operation.property(value)
    elif self.at_punctuation('('):
      self.expect_punctuation('(')
      self.expect_punctuation(')')
      op = data.Operation.call()
    elif self.at_punctuation('['):
      self.expect_punctuation('[')
      self.expect_punctuation(']')
      op = data.Operation.index()
    else:
      raise self.new_syntax_error()
    if self.at_punctuation(':='):
      self.expect_punctuation(':=')
      self.expect_punctuation('(')
      self.expect_punctuation(')')
      op = data.Operation.assign(op)
    self.expect_statement_delimiter(expect_delim)
    return ast.Literal(op)

  def parse_call_literal_argument(self, tag_dispenser):
    if self.at_type(Token.TAG):
      tag_value = self.expect_type(Token.TAG)
      tag = ast.Literal(tag_value)
      value = self.parse_expression(False)
    else:
      value_or_tag = self.parse_expression(False)
      if self.at_punctuation(':'):
        self.expect_punctuation(':')
        tag = value_or_tag
        value = self.parse_expression(False)
      else:
        tag = ast.Literal(tag_dispenser.get_next())
        value = value_or_tag
    return ast.CallLiteralArgument(tag, value)

  def parse_call_literal(self, expect_delim):
    self.expect_word('call')
    args = []
    self.expect_punctuation('(')
    tag_dispenser = TagDispenser()
    if not self.at_punctuation(')'):
      first = self.parse_call_literal_argument(tag_dispenser)
      args.append(first)
      while self.at_punctuation(','):
        self.expect_punctuation(',')
        next = self.parse_call_literal_argument(tag_dispenser)
        args.append(next)
    self.expect_punctuation(')')
    self.expect_statement_delimiter(expect_delim)
    return ast.CallLiteral(args)

  # <field declaration>
  #   -> "field" <subject> <operator> ";"
  def parse_field_declaration(self):
    self.expect_word('field')
    subject = self.parse_subject()
    op = self.expect_type(Token.OPERATION)
    prop = data.Operation.property(op)
    getter = self.name_as_selector(prop)
    setter = self.name_as_selector(data.Operation.assign(prop))
    self.expect_statement_delimiter(True)
    key_name = data.Path([op])
    return ast.FieldDeclaration(subject, key_name, getter, setter)

  # <type declaration>
  #   -> "type" <ident> ("is" <atomic>)? <members>
  def parse_type_declaration(self):
    self.expect_word('type')
    name = self.expect_type(Token.IDENTIFIER)
    if self.at_word('is'):
      self.expect_word('is')
      supertype = [self.parse_atomic_expression()]
    else:
      supertype = []
    members = self.parse_type_members(name)
    self.expect_statement_delimiter(True)
    return ast.TypeDeclaration(name, supertype, members)

  # <type members>
  #   -> "{" <type member>* "}"
  #   -> .
  def parse_type_members(self, holder):
    result = []
    if not self.at_punctuation('{'):
      return result
    self.expect_punctuation('{')
    try:
      # Change the default subject guard for the case where none is explicitly
      # given to the enclosing type.
      old_default = self.default_subject_guard
      self.default_subject_guard = ast.Guard.is_(ast.Variable(holder))
      while not self.at_punctuation('}'):
        member = self.parse_type_member()
        result.append(member)
    finally:
      # Remember to pop the default guard off again.
      self.default_subject_guard = old_default
    self.expect_punctuation('}')
    return result

  # <type member>
  #   -> <toplevel declaration>
  def parse_type_member(self):
    if self.at_word('def'):
      return self.parse_toplevel_declaration([])
    elif self.at_word('field'):
      return self.parse_field_declaration()
    else:
      raise self.new_syntax_error()

  # <new expression>
  #   -> "new" <atomic expression> <arguments>
  def parse_new_expression(self, expect_delim):
    self.expect_word('new')
    cons = self.parse_atomic_expression()
    args = self.parse_arguments('(', ')')
    self.expect_statement_delimiter(expect_delim)
    prefix = [
      ast.Argument(data._SUBJECT, cons),
      ast.Argument(data._SELECTOR, ast.Literal(Parser._NEW)),
    ]
    return ast.Invocation(prefix + args)

  # <if expression>
  #   -> "if" <expression> "then" <expression> ("else" <expression>)?
  def parse_if_expression(self, expect_delim):
    self.expect_word('if')
    cond = self.parse_expression(False)
    self.expect_word('then')
    then_part = self.parse_expression(expect_delim)
    if self.at_word('else'):
      self.expect_word('else')
      else_part = self.parse_expression(expect_delim)
    else:
      else_part = ast.Literal(None)
    methods = [
      ast.Lambda.method(then_part, data.Operation.infix('then')),
      ast.Lambda.method(else_part, data.Operation.infix('else'))
    ]
    args = [
      ast.Argument(data._SUBJECT, ast.Variable(data.Identifier(-1, data.Path(["core", "if"])))),
      ast.Argument(data._SELECTOR, ast.Literal(Parser._SAUSAGES)),
      ast.Argument(0, cond),
      ast.Argument(1, ast.Lambda(methods))
    ]
    result = ast.Invocation(args)
    return result

  # <while expression>
  #   -> "while" <expression> "do" <expression>
  def parse_while_expression(self, expect_delim):
    self.expect_word('while')
    cond = self.parse_expression(False)
    self.expect_word('do')
    body = self.parse_expression(expect_delim)
    methods = [
      ast.Lambda.method(cond, data.Operation.infix('keep_running')),
      ast.Lambda.method(body, data.Operation.infix('run'))
    ]
    return ast.Invocation([
      ast.Argument(data._SUBJECT, ast.Variable(data.Identifier(-1, data.Path(["core", "while"])))),
      ast.Argument(data._SELECTOR, ast.Literal(Parser._SAUSAGES)),
      ast.Argument(0, ast.Lambda(methods)),
    ])

  # <signal expression>
  #   -> ("signal" | "abort") <operator tail> ("default" <expression>)?
  def parse_signal_expression(self, expect_delim):
    if self.at_word('leave'):
      is_abort = True
      self.expect_word('leave')
    else:
      is_abort = False
      self.expect_word('signal')
    (selector, rest) = self.parse_operator_tail()
    if self.at_word('else'):
      self.expect_word('else')
      default = self.parse_expression(expect_delim)
    else:
      default = None
      self.expect_statement_delimiter(expect_delim)
    return ast.Signal(is_abort, [
      ast.Argument(data._SUBJECT, ast.Literal(None)),
      ast.Argument(data._SELECTOR, ast.Literal(selector))
    ] + rest, default)

  # <for expression>
  #   -> "for" <signature> "in" <expression> "do" <expression>
  def parse_for_expression(self, expect_delim):
    self.expect_word('for')
    sig = self.parse_signature()
    self.expect_word('in')
    elms = self.parse_expression(False)
    self.expect_word('do')
    body = self.parse_expression(expect_delim)
    thunk = ast.Lambda([ast.Method(sig, body)])
    return ast.Invocation([
      ast.Argument(data._SUBJECT, ast.Variable(data.Identifier(-1, data.Path(["core", "for"])))),
      ast.Argument(data._SELECTOR, ast.Literal(Parser._SAUSAGES)),
      ast.Argument(0, elms),
      ast.Argument(1, thunk),
    ])

  # <try expression>
  #   -> "try" <expression> "ensure" <expression>
  def parse_try_expression(self, expect_delim):
    self.expect_word('try')
    body = self.parse_expression(False)
    if self.at_word('on'):
      self.expect_word('on')
      subject = [self.get_functino_subject()]
      signature = self.parse_functino_signature(subject, False)
      methods = self.parse_functino_tail(signature, subject)
      body = ast.SignalHandler(body, methods)
    if self.at_word('ensure'):
      self.expect_word('ensure')
      on_exit = self.parse_expression(expect_delim)
      body = ast.Ensure(body, on_exit)
    return body

  # <with escape expression>
  #   -> "with_escape" <ident> "do" <expression>
  def parse_with_escape_expression(self, expect_delim):
    self.expect_word('with_escape')
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_word('do')
    body = self.parse_expression(expect_delim)
    return ast.WithEscape(name, body)

  # <assignment expression>
  #   -> <operator expression> :* ":="
  def parse_assignment_expression(self, expect_delim):
    lvalue = self.parse_operator_expression()
    if self.at_punctuation(':='):
      self.expect_punctuation(':=')
      rvalue = self.parse_assignment_expression(False)
      result = lvalue.to_assignment(rvalue)
    else:
      result = lvalue
    self.expect_statement_delimiter(expect_delim)
    return result

  # Returns a parameter that can be used as the subject of a lambda or block.
  def get_functino_subject(self):
    return self.name_as_subject(data.Identifier(0, data.Path(['self'])))

  # <lambda expression>
  #   -> "fn" <parameters> <functino tail>
  def parse_lambda_expression(self):
    self.expect_word('fn')
    if self.at_word('on'):
      self.expect_word('on')
    subject = [self.get_functino_subject()]
    signature = self.parse_functino_signature(subject, False)
    methods = self.parse_functino_tail(signature, subject)
    return ast.Lambda(methods)

  # <block expression>
  #   -> "bk" <ident> <functino tail> "in" <expression>
  def parse_block_expression(self, expect_delim):
    self.expect_word('bk')
    name = self.expect_type(Token.IDENTIFIER)
    subject = [self.get_functino_subject()]
    signature = self.parse_functino_signature(subject, False)
    methods = self.parse_functino_tail(signature, subject)
    self.expect_word('in')
    body = self.parse_word_expression(expect_delim)
    return ast.Block(name, methods, body)

  # <functino tail>
  #   -> <method tail>
  def parse_functino_tail(self, first_signature, subject):
    first_body = self.parse_method_tail(False)
    first_method = ast.Method(first_signature, first_body)
    methods = [first_method]
    while self.at_word('on'):
      self.expect_word('on')
      next_signature = self.parse_functino_signature(subject, False)
      next_body = self.parse_method_tail(False)
      next_method = ast.Method(next_signature, next_body)
      methods.append(next_method)
    return methods

  # Parses the part that comes after the "fn" or name of the block or method.
  # This is a mess, it should be cleaned up when the parser is rewritten.
  def parse_functino_signature(self, subject, is_prefix, default_name=None):
    name = default_name
    if (not is_prefix) and self.at_type(Token.OPERATION):
      name = self.expect_type(Token.OPERATION)
    if name:
      default_operation = data.Operation.infix(name)
    else:
      default_operation = Parser._SAUSAGES
    (params, param_operation, allow_extra) = self.parse_parameters(default_operation)
    if is_prefix:
      op = data.Operation.prefix(name)
      params = []
    elif params is None:
      params = []
      if name is None:
        op = Parser._SAUSAGES
      else:
        op = data.Operation.property(name)
    else:
      op = param_operation
    if self.at_punctuation(':='):
      self.expect_punctuation(':=')
      (assign_params, assign_op, allow_extra) = self.parse_parameters(None, start_index=len(params))
      params += assign_params
      op = data.Operation.assign(op)
    if allow_extra:
      selector = self.any_selector()
    else:
      selector = self.name_as_selector(op)
    return ast.Signature(subject + [selector] + params, allow_extra)

  # Are we currently at a token that is allowed as the first token of a
  # parameter?
  def at_parameter_start(self):
    return self.at_type(Token.IDENTIFIER) or self.at_type(Token.TAG) or self.at_operation('*')

  # Given a string operation name returns the corresponding selector parameter.
  def name_as_selector(self, name):
    assert isinstance(name, data.Operation)
    return ast.Parameter(data.Identifier(0, data.Path(['name'])), [data._SELECTOR],
      ast.Guard.eq(ast.Literal(name)))

  # Returns a parameter that matches any selector.
  def any_selector(self):
    return ast.Parameter(data.Identifier(0, data.Path(['name'])), [data._SELECTOR],
      ast.Guard.any())

  def name_as_subject(self, name):
    return ast.Parameter(name, [data._SUBJECT], ast.Guard.any())

  # Same as parse_parameters but returns a full signature.
  def parse_signature(self):
    prefix = [
      self.name_as_subject(data.Identifier(0, data.Path(['self']))),
      self.name_as_selector(Parser._SAUSAGES)
    ]
    (params, operation, allow_extra) = self.parse_parameters(None)
    if params is None:
      params = []
    return ast.Signature(prefix + params, allow_extra)

  # <parameters>
  #   -> "(" <parameter> *: "," ")"
  #   -> <parameter>
  #   -> .
  def parse_parameters(self, default_operation, start_index=0):
    operation = default_operation
    if self.at_parameter_start():
      param = self.parse_parameter(0)
      return ([param], operation, False)
    elif self.at_punctuation('('):
      (start, end) = ("(", ")")
    elif self.at_punctuation('['):
      (start, end) = ("[", "]")
      operation = Parser._SQUARE_SAUSAGES
    else:
      return (None, operation, False)
    self.expect_punctuation(start)
    result = []
    allow_extra = False
    if not self.at_punctuation(end):
      index = start_index
      first = self.parse_parameter(index)
      if first is None:
        allow_extra = True
      else:
        index += 1
        result.append(first)
      while self.at_punctuation(','):
        self.expect_punctuation(',')
        next = self.parse_parameter(index)
        if next is None:
          allow_extra = True
        else:
          index += 1
          result.append(next)
    self.expect_punctuation(end)
    return (result, operation, allow_extra)

  def parse_parameter(self, default_tag):
    tags = [default_tag]
    if self.at_type(Token.TAG):
      tag = self.expect_type(Token.TAG)
      tags = [tag] + tags
    elif self.at_operation('*'):
      self.expect_operation('*')
      return None
    name = self.expect_type(Token.IDENTIFIER)
    if self.at_operation('=='):
      self.expect_operation('==')
      guard_value = self.parse_atomic_expression()
      guard = ast.Guard.eq(guard_value)
    elif self.at_word('is'):
      self.expect_word('is')
      guard_value = self.parse_atomic_expression()
      guard = ast.Guard.is_(guard_value)
    else:
      if default_tag == data._SUBJECT:
        guard = self.default_subject_guard
      else:
        guard = ast.Guard.any()
    result = ast.Parameter(name, tags, guard)
    return result

  # <operator expression>
  #   -> <call expression> +: <operator tail>
  def parse_operator_expression(self):
    left = self.parse_unary_expression()
    while self.at_type(Token.OPERATION):
      (selector, rest) = self.parse_operator_tail()
      prefix = [
        ast.Argument(data._SUBJECT, left),
        ast.Argument(data._SELECTOR, ast.Literal(selector))
      ]
      left = ast.Invocation(prefix + rest)
    return left

  # <operator tail>
  #   <operation> <arguments>
  def parse_operator_tail(self):
    name = self.expect_type(Token.OPERATION)
    if self.at_atomic_start():
      selector = data.Operation.infix(name)
      rest = self.parse_arguments('(', ')')
    else:
      selector = data.Operation.property(name)
      rest = []
    return (selector, rest)

  # <arguments>
  #   -> <call expression>
  #   -> <start> <expression> *: "," <end>
  def parse_arguments(self, start, end):
    args = []
    if self.at_punctuation(start):
      self.expect_punctuation(start)
      if not self.at_punctuation(end):
        arg = self.parse_argument(0)
        args.append(arg)
        index = 1
        while self.at_punctuation(','):
          self.expect_punctuation(',')
          arg = self.parse_argument(index)
          args.append(arg)
          index += 1
      self.expect_punctuation(end)
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
    value = self.parse_expression(False)
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
      return ast.Invocation(args)
    else:
      return self.parse_call_expression()

  # <call expression>
  #   -> <atomic expression>
  def parse_call_expression(self):
    recv = self.parse_atomic_expression()
    while True:
      if self.at_punctuation('('):
        selector = Parser._SAUSAGES
        start = '('
        end = ')'
      elif self.at_punctuation('['):
        selector = Parser._SQUARE_SAUSAGES
        start = '['
        end = ']'
      else:
        break
      prefix = [
        ast.Argument(data._SUBJECT, recv),
        ast.Argument(data._SELECTOR, ast.Literal(selector))
      ]
      rest = self.parse_arguments(start, end)
      recv = ast.Invocation(prefix + rest)
    return recv

  # <variable>
  #   -> <identifier>
  def parse_variable(self):
    ident = self.expect_type(Token.IDENTIFIER)
    stage_index = ident.stage
    stage = self.module.get_or_create_fragment(stage_index)
    return ast.Variable(ident)

  # Are we at the beginning of an atomic expression? This is a massive hack and
  # must be replaced when there's a proper precedence parser.
  def at_atomic_start(self):
    return (self.at_type(Token.LITERAL)
        or self.at_type(Token.IDENTIFIER)
        or self.at_punctuation('(')
        or self.at_punctuation('[')
        or self.at_punctuation('{')
        or self.at_word('null')
        or self.at_word('true')
        or self.at_word('false')
        or self.at_word('module')
        or self.at_word('subject')
        or self.at_word('selector')
        or self.at_type(Token.QUOTE))

  # <atomic expression>
  #   -> <literal>
  #   -> "(" <expression> ")"
  #   -> <array expression>
  #   -> <sequence expression>
  def parse_atomic_expression(self):
    if self.at_type(Token.LITERAL):
      value = self.expect_type(Token.LITERAL)
      result = ast.Literal(value)
      if isinstance(value, data.DecimalFraction):
        # While the infrastructure is not in place to create floats without
        # magic we insert a new_float invocation in the parser.
        result = ast.Invocation([
          ast.Argument(data._SUBJECT, ast.Variable(data.Identifier(-1, data.Path(["ctrino"])))),
          ast.Argument(data._SELECTOR, ast.Literal(data.Operation.infix("new_float_32"))),
          ast.Argument(0, result)
        ])
      return result
    elif self.at_type(Token.IDENTIFIER):
      return self.parse_variable()
    elif self.at_punctuation('('):
      self.expect_punctuation('(')
      result = self.parse_expression(False)
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
    elif self.at_word('subject'):
      self.expect_word('subject')
      return ast.Literal(data._SUBJECT)
    elif self.at_word('selector'):
      self.expect_word('selector')
      return ast.Literal(data._SELECTOR)
    elif self.at_word('module'):
      self.expect_word('module')
      return ast.CurrentModule()
    elif self.at_type(Token.QUOTE):
      return self.parse_quote()
    else:
      raise self.new_syntax_error()

  def parse_quote(self):
    stage = self.expect_type(Token.QUOTE)
    self.expect_punctuation('(')
    value = self.parse_expression(False)
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
      first = self.parse_expression(False)
      elements.append(first)
      while self.at_punctuation(','):
        self.expect_punctuation(',')
        next = self.parse_expression(False)
        elements.append(next)
    self.expect_punctuation(']')
    return ast.Array(elements)

  # Raises an error if the current token is not a valid statement delimiter. If
  # the delimiter is explicit consumes it.
  def expect_statement_delimiter(self, expect_delim):
    if not expect_delim:
      return
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
    elif self.at_local_declaration():
      (name, value, is_mutable) = self.parse_local_declaration()
      body = ast.Sequence.make(self.parse_statement_list())
      return [ast.LocalDeclaration(name, is_mutable, value, body)]
    else:
      next = self.parse_expression(True)
      tail = self.parse_statement_list()
      return [next] + tail

  def at_local_declaration(self):
    return self.at_word('def') or self.at_word('var')

  def parse_local_declaration(self):
    if self.at_word('var'):
      self.expect_word('var')
      is_mutable = True
    else:
      self.expect_word('def')
      is_mutable = False
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_punctuation(':=')
    value = self.parse_expression(True)
    return (name, value, is_mutable)

  # Creates a new syntax error at the current token.
  def new_syntax_error(self):
    return SyntaxError(self.current())


# Module manifest parser.
class ModuleParser(Parser):

  def __init__(self, tokens):
    super(ModuleParser, self).__init__(tokens, None)

  def parse_module_manifest(self):
    self.expect_word('module')
    name = self.expect_type(Token.IDENTIFIER)
    self.expect_punctuation('{')
    self.expect_word('source')
    values = self.parse_array_expression()
    self.expect_punctuation('}')
    return ast.ModuleManifest(name, values)


# Exception signalling a syntax error.
class SyntaxError(Exception):

  def __init__(self, token):
    self.token = token

  def __str__(self):
    return "at %s" % self.token.value
