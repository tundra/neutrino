# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Neutrino syntax tree definitions

from abc import abstractmethod
import data
import plankton

# Syntax tree visitor. The default behavior of each visit method is to apply
# the visitor recursively.
class Visitor(object):

  def __init__(self):
    pass

  def visit_ast(self, that):
    that.traverse(self)

  def visit_literal(self, that):
    self.visit_ast(that)

  def visit_array(self, that):
    self.visit_ast(that)

  def visit_lambda(self, that):
    self.visit_ast(that)

  def visit_variable(self, that):
    self.visit_ast(that)

  def visit_variable_assignment(self, that):
    self.visit_ast(that)

  def visit_invocation(self, that):
    self.visit_ast(that)

  def visit_signal(self, that):
    self.visit_ast(that)

  def visit_argument(self, that):
    self.visit_ast(that)

  def visit_sequence(self, that):
    self.visit_ast(that)

  def visit_local_declaration(self, that):
    self.visit_ast(that)
  
  def visit_block(self, that):
    self.visit_ast(that)

  def visit_with_escape(self, that):
    self.visit_ast(that)

  def visit_namespace_declaration(self, that):
    self.visit_ast(that)

  def visit_method_declaration(self, that):
    self.visit_ast(that)

  def visit_function_declaration(self, that):
    self.visit_ast(that)

  def visit_signature(self, that):
    self.visit_ast(that)

  def visit_parameter(self, that):
    self.visit_ast(that)

  def visit_method(self, that):
    self.visit_ast(that)

  def visit_guard(self, that):
    self.visit_ast(that)

  def visit_quote(self, that):
    self.visit_ast(that)

  def visit_import(self, that):
    self.visit_ast(that)

  def visit_is_declaration(self, that):
    self.visit_ast(that)

  def visit_current_module(self, that):
    self.visit_ast(that)


# A constant literal value.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Literal"))
class Literal(object):

  @plankton.field("value")
  def __init__(self, value):
    self.value = value

  def accept(self, visitor):
    return visitor.visit_literal(self);

  def traverse(self, visitor):
    pass

  def __str__(self):
    return "(literal %s)" % self.value


# An array expression.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Array"))
class Array(object):

  @plankton.field("elements")
  def __init__(self, elements):
    self.elements = elements

  def accept(self, visitor):
    return visitor.visit_array(self)

  def traverse(self, visitor):
    for element in self.elements:
      element.accept(visitor)

  def __str__(self):
    return "(array %s)" % map(str, self.elements)


# A reference to an enclosing binding. The name is used before the variable
# has been resolved, the symbol after.
@plankton.serializable()
class Variable(object):

  _LOCAL_HEADER = plankton.EnvironmentReference.path("ast", "LocalVariable")
  _NAMESPACE_HEADER = plankton.EnvironmentReference.path("ast", "NamespaceVariable")

  def __init__(self, ident, symbol=None):
    self.ident = ident
    self.symbol = symbol

  def accept(self, visitor):
    return visitor.visit_variable(self)

  def traverse(self, visitor):
    pass

  def get_name(self):
    return self.ident.get_name()

  def to_assignment(self, rvalue):
    return VariableAssignment(self, rvalue)

  @plankton.header
  def get_header(self):
    if self.symbol is None:
      return Variable._NAMESPACE_HEADER
    else:
      return Variable._LOCAL_HEADER

  @plankton.payload
  def get_payload(self):
    if self.symbol is None:
      return {'name': self.ident}
    else:
      return {'symbol': self.symbol}

  def __str__(self):
    return "(var %s)" % str(self.ident)


@plankton.serializable(plankton.EnvironmentReference.path("ast", "VariableAssignment"))
class VariableAssignment(object):

  @plankton.field("target")
  @plankton.field("value")
  def __init__(self, target, value):
    self.target = target
    self.value = value

  def accept(self, visitor):
    visitor.visit_variable_assignment(self)

  def traverse(self, visitor):
    self.target.accept(visitor)
    self.value.accept(visitor)


# A multi-method invocation.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Invocation"))
class Invocation(object):

  @plankton.field("arguments")
  def __init__(self, arguments):
    self.arguments = arguments

  def accept(self, visitor):
    return visitor.visit_invocation(self)

  def traverse(self, visitor):
    for argument in self.arguments:
      argument.accept(visitor)

  def to_assignment(self, rvalue):
    new_args = []
    max_arg_index = -1
    for arg in self.arguments:
      new_arg = arg
      if arg.tag is data._SELECTOR:
        inner_op = arg.value.value
        outer_op = data.Operation.assign(inner_op)
        new_arg = Argument(arg.tag, Literal(outer_op))
      elif type(arg.tag) == int:
        max_arg_index = max(max_arg_index, arg.tag)
      new_args.append(new_arg)
    new_args.append(Argument(max_arg_index + 1, rvalue))
    return Invocation(new_args)

  def __str__(self):
    return "(call %s)" % " ".join(map(str, self.arguments))


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Signal"))
class Signal(object):

  @plankton.field("arguments")
  def __init__(self, arguments):
    self.arguments = arguments

  def accept(self, visitor):
    return visitor.visit_signal(self)

  def traverse(self, visitor):
    for argument in self.arguments:
      argument.accept(visitor)


# An individual argument to an invocation.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Argument"))
class Argument(object):

  @plankton.field("tag")
  @plankton.field("value")
  def __init__(self, tag, value):
    self.tag = tag
    self.value = value

  def accept(self, visitor):
    return visitor.visit_argument(self)

  def traverse(self, visitor):
    self.value.accept(visitor)

  def __str__(self):
    return "(argument %s %s)" % (self.tag, self.value)


# A binding from a symbol to a value.
@plankton.serializable()
class Binding(object):

  @plankton.field("symbol")
  @plankton.field("value")
  def __init__(self, symbol, value):
    self.symbol = symbol
    self.value = value


# A sequence of expressions to execute in order, yielding the value of the last
# expression.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Sequence"))
class Sequence(object):

  @plankton.field("values")
  def __init__(self, values):
    self.values = values

  def accept(self, visitor):
    return visitor.visit_sequence(self)

  def traverse(self, visitor):
    for value in self.values:
      value.accept(visitor)

  @staticmethod
  def make(values):
    if len(values) == 0:
      return Literal(None)
    elif len(values) == 1:
      return values[0]
    else:
      return Sequence(values)

  def __str__(self):
    return "(sequence %s)" % map(str, self.values)


# A local variable declaration.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "LocalDeclaration"))
class LocalDeclaration(object):

  @plankton.field("symbol")
  @plankton.field("is_mutable")
  @plankton.field("value")
  @plankton.field("body")
  def __init__(self, ident, is_mutable, value, body):
    self.ident = ident
    self.symbol = None
    self.is_mutable = is_mutable
    self.value = value
    self.body = body

  def accept(self, visitor):
    return visitor.visit_local_declaration(self)

  def traverse(self, visitor):
    self.value.accept(visitor)
    self.body.accept(visitor)

  def get_name(self):
    return self.ident.get_name()

  def __str__(self):
    if self.is_mutable:
      type = "def"
    else:
      type = "var"
    return "(%s %s := %s in %s)" % (type, self.ident, self.value, self.body)


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Block"))
class Block(object):

  @plankton.field("symbol")
  @plankton.field("method")
  @plankton.field("body")
  def __init__(self, ident, method, body):
    self.ident = ident
    self.symbol = None
    self.method = method
    self.body = body

  def accept(self, visitor):
    return visitor.visit_block(self)

  def traverse(self, visitor):
    self.method.accept(visitor)
    self.body.accept(visitor)

  def get_name(self):
    return self.ident.get_name()

  def __str__(self):
    return "(lfn %s %s in %s)" % (type, self.ident, self.method, self.body)


# A local escape capture.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "WithEscape"))
class WithEscape(object):

  @plankton.field("symbol")
  @plankton.field("body")
  def __init__(self, ident, body):
    self.ident = ident
    self.symbol = None
    self.body = body

  def accept(self, visitor):
    return visitor.visit_with_escape(self)

  def traverse(self, visitor):
    self.body.accept(visitor)

  def get_name(self):
    return self.ident.get_name()


# A symbol that identifies a scoped binding.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Symbol"))
class Symbol(object):

  @plankton.field("name")
  @plankton.field("origin")
  def __init__(self, name, origin=None):
    self.name = name
    self.origin = origin

  def set_origin(self, value):
    assert self.origin is None
    self.origin = value
    return self


# An individual method parameter.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Parameter"))
class Parameter(object):

  @plankton.field("symbol")
  @plankton.field("tags")
  @plankton.field("guard")
  def __init__(self, ident, tags, guard):
    self.ident = ident
    self.symbol = Symbol(self.get_name())
    self.tags = tags
    self.guard = guard

  def accept(self, visitor):
    visitor.visit_parameter(self)

  def traverse(self, visitor):
    self.guard.accept(visitor)

  def get_name(self):
    return self.ident.get_name()

  def get_symbol(self):
    return self.symbol


  def __str__(self):
    return "(param (tags %s) (name %s) (guard %s))" % (
        ", ".join(map(str, self.tags)),
        self.ident,
        self.guard)


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Signature"))
class Signature(object):

  @plankton.field("parameters")
  @plankton.field("allow_extra")
  def __init__(self, parameters, allow_extra):
    self.parameters = parameters
    self.allow_extra = allow_extra

  def accept(self, visitor):
    visitor.visit_signature(self)

  def traverse(self, visitor):
    for param in self.parameters:
      param.accept(visitor)

  def to_data(self):
    data_params = [param.to_data() for param in self.parameters]
    return data.Signature(data_params)

  def __str__(self):
    return "(signature %s)" % " ".join(map(str, self.parameters))


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Guard"))
class Guard(object):

  @plankton.field("type")
  @plankton.field("value")
  def __init__(self, type, value):
    self.type = type
    self.value = value

  def accept(self, visitor):
    visitor.visit_guard(self)

  def traverse(self, visitor):
    if not self.value is None:
      self.value.accept(visitor)

  # Is this an any-guard?
  def is_any(self):
    return self.type == data.Guard._ANY

  def __str__(self):
    if self.value is None:
      return "%s()" % self.type
    else:
      return "%s(%s)" % (self.type, self.value)

  @staticmethod
  def any():
    return Guard(data.Guard._ANY, None)

  @staticmethod
  def eq(value):
    assert not value is None
    return Guard(data.Guard._EQ, value)

  # 'is' is a reserved word in python. T t t.
  @staticmethod
  def is_(value):
    assert not value is None
    return Guard(data.Guard._IS, value)


# An anonymous function. These can be broken down into equivalent new-object
# and set-property calls but that's for later.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Lambda"))
class Lambda(object):

  @plankton.field("method")
  def __init__(self, method):
    self.method = method

  def accept(self, visitor):
    return visitor.visit_lambda(self)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def __str__(self):
    return "(fn (%s) => %s)" % (self.method.signature, self.method.body)

  # Creates a no-argument lambda with the given expression as the body.
  @staticmethod
  def thunk(body):
    signature = Signature([
      Parameter(data.Identifier(0, data.Path(['self'])), [data._SUBJECT],
        Guard.any()),
      Parameter(data.Identifier(0, data.Path(['name'])), [data._SELECTOR],
        Guard.eq(Literal(data.Operation.call())))
    ], False)
    return Lambda(Method(signature, body))


# Yields the current bound module fragment.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "CurrentModule"))
class CurrentModule(object):

  def accept(self, visitor):
    return visitor.visit_current_module(self)

  def traverse(self, visitor):
    pass

  def __str__(self):
    return "(current-module)"


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Program"))
class Program(object):

  @plankton.field("entry_point")
  @plankton.field("module")
  def __init__(self, entry_point, module):
    self.entry_point = entry_point
    self.module = module

  def accept(self, visitor):
    return visitor.visit_program(self)

  def __str__(self):
    return "(program %s)" % str(self.module)


# A toplevel namespace declaration.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "NamespaceDeclaration"))
class NamespaceDeclaration(object):

  @plankton.field("annotations")
  @plankton.field("path")
  @plankton.field("value")
  def __init__(self, annotations, ident, value):
    self.annotations = annotations
    self.ident = ident
    self.path = ident.path
    self.value = value

  # Returns the stage this declaration belongs to.
  def get_stage(self):
    return self.ident.stage

  def accept(self, visitor):
    return visitor.visit_namespace_declaration(self)

  def apply(self, module):
    fragment = module.get_or_create_fragment(self.ident.stage)
    fragment.add_element(self)

  def traverse(self, visitor):
    for annot in self.annotations:
      annot.accept(visitor)
    self.value.accept(visitor)

  def __str__(self):
    return "(namespace-declaration %s %s)" % (self.ident, self.value)


# Syntax of a method.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "Method"))
class Method(object):

  @plankton.field("signature")
  @plankton.field("body")
  def __init__(self, signature, body):
    self.signature = signature
    self.body = body

  def accept(self, visitor):
    visitor.visit_method(self)

  def traverse(self, visitor):
    self.signature.accept(visitor)
    self.body.accept(visitor)


# A toplevel method declaration.
@plankton.serializable(plankton.EnvironmentReference.path("ast", "MethodDeclaration"))
class MethodDeclaration(object):

  @plankton.field("annotations")
  @plankton.field("method")
  def __init__(self, stage, annotations, method):
    self.stage = stage
    self.annotations = annotations
    self.method = method

  def accept(self, visitor):
    return visitor.visit_method_declaration(self)

  def traverse(self, visitor):
    for annot in self.annotations:
      annot.accept(visitor)
    self.method.accept(visitor)

  def apply(self, module):
    fragment = module.get_or_create_fragment(self.stage)
    fragment.add_element(self)          

  def __str__(self):
    return "(method-declaration %s %s)" % (self.method.signature, self.method.body)


# A toplevel function declaration.
class FunctionDeclaration(object):

  def __init__(self, ident, method):
    self.ident = ident
    self.method = method

  def get_stage(self):
    return self.ident.stage

  def accept(self, visitor):
    return visitor.visit_function_declaration(self)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def apply(self, module):
    stage = self.ident.stage
    value_fragment = module.get_or_create_fragment(stage - 1);
    value_fragment.ensure_function_declared(self.ident)
    method_fragment = module.get_or_create_fragment(stage)
    method_fragment.add_element(MethodDeclaration(0, [], self.method))

  def __str__(self):
    return "(function-declaration %s %s)" % (self.method.signature, self.method.body)


# A toplevel type declaration
class TypeDeclaration(object):

  _NEW_TYPE = data.Operation.infix("new_type")

  def __init__(self, ident, supers, members):
    self.ident = ident
    self.supers = supers
    self.members = members

  def apply(self, module):
    name_decl = NamespaceDeclaration([], self.ident, Invocation([
      Argument(data._SUBJECT, CurrentModule()),
      Argument(data._SELECTOR, Literal(TypeDeclaration._NEW_TYPE)),
      Argument(0, Literal(self.ident)),
    ]))
    name_decl.apply(module)
    for parent in self.supers:
      sub = Variable(self.ident)
      is_decl = IsDeclaration(sub, parent)
      is_decl.apply(module)
    for member in self.members:
      member.apply(module)


# A stand-alone field declaration.
class FieldDeclaration(object):

  _NEW_GLOBAL_FIELD = data.Operation.infix("new_global_field")
  _SQUARE_SAUSAGES = data.Operation.index()
  _SQUARE_SAUSAGE_ASSIGN = data.Operation.assign(data.Operation.index())

  def __init__(self, subject, key_name, getter, setter):
    self.subject = subject
    self.key_name = key_name
    self.getter = getter
    self.setter = setter

  def apply(self, module):
    # TODO: this field shouldn't be accessible through the namespace.
    key_ident = data.Identifier(-1, self.key_name)
    key_access = Variable(key_ident)
    key_decl = NamespaceDeclaration([], key_ident, Invocation([
      Argument(data._SUBJECT, CurrentModule()),
      Argument(data._SELECTOR, Literal(FieldDeclaration._NEW_GLOBAL_FIELD)),
      Argument(0, Literal(self.key_name)),
    ]))
    key_decl.apply(module)
    getter = MethodDeclaration(0, [], Method(
      Signature([self.subject, self.getter], False), Invocation([
        Argument(data._SUBJECT, key_access),
        Argument(data._SELECTOR, Literal(FieldDeclaration._SQUARE_SAUSAGES)),
        Argument(0, Variable(self.subject.ident))
    ])))
    value = Parameter(data.Identifier(0, data.Path(['value'])), [0],
      Guard.any())
    setter = MethodDeclaration(0, [], Method(
      Signature([self.subject, self.setter, value], False), Invocation([
        Argument(data._SUBJECT, key_access),
        Argument(data._SELECTOR, Literal(FieldDeclaration._SQUARE_SAUSAGE_ASSIGN)),
        Argument(0, Variable(self.subject.ident)),
        Argument(1, Variable(value.ident))
    ])))
    getter.apply(module)
    setter.apply(module)


@plankton.serializable(plankton.EnvironmentReference.path("core", "UnboundModule"))
class UnboundModule(object):

  @plankton.field('path')
  @plankton.field('fragments')
  def __init__(self, path, fragments):
    self.path = path
    self.fragments = fragments

  def __str__(self):
    return "(module %s %s)" % (self.path, " ".join(map(str, self.fragments)))


@plankton.serializable(plankton.EnvironmentReference.path("core", "UnboundModuleFragment"))
class UnboundModuleFragment(object):

  @plankton.field('stage')
  @plankton.field('imports')
  @plankton.field('elements')
  def __init__(self, stage):
    self.stage = stage
    self.imports = []
    self.elements = []
    self.functions = set()

  # Returns all the imports for this stage.
  def add_import(self, path):
    self.imports.append(path)

  def add_element(self, element):
    self.elements.append(element)

  def ensure_function_declared(self, name):
    if name in self.functions:
      return
    self.functions.add(name)
    value = Invocation([
      Argument(data._SUBJECT, Variable(data.Identifier(-1, data.Path(["ctrino"])))),
      Argument(data._SELECTOR, Literal(data.Operation.infix("new_function"))),
      Argument(0, Literal(name.path))
    ])
    self.add_element(NamespaceDeclaration([], name, value))

  def __str__(self):
    return "(fragment %s %s)" % (self.stage, " ".join(map(str, self.elements)))


# A full compilation unit.
class Module(object):

  def __init__(self, module_name):
    self.module_name = module_name
    self.entry_point = None
    self.stages = {}
    self.get_or_create_fragment(0)

  def add_element(self, *elements):
    for element in elements:
      element.apply(self)
    return self

  def get_stages(self):
    for index in sorted(self.stages.keys()):
      yield (index, self.stages[index])

  def set_entry_point(self, value):
    self.entry_point = value
    return self

  # Returns the stage with the given index. If no such stage exists an exception
  # is raised.
  def get_stage(self, index):
    return self.stages[index]

  # Returns the oldest stage, the one with the lowest stage offset.
  def get_oldest_stage(self):
    result = None
    result_stage = 100
    for (stage, value) in self.stages.items():
      if stage < result_stage:
        result = value
        result_stage = stage
    return result

  def get_or_create_fragment(self, index):
    if not index in self.stages:
      self.stages[index] = self.create_fragment(index)
    return self.stages[index]

  def create_fragment(self, stage):
    return UnboundModuleFragment(stage)

  def get_present(self):
    return self.get_or_create_fragment(0)

  def get_present_program(self):
    module = self.as_unbound_module()
    return Program(self.entry_point, module)

  def get_present_module(self):
    last_stage = self.get_present()
    return last_stage.get_module()

  def accept(self, visitor):
    return visitor.visit_unit(self)

  def as_unbound_module(self):
    fragments = []
    for (index, fragment) in self.stages.iteritems():
      fragments.append(fragment)
    return UnboundModule(data.Path([self.module_name]), fragments)

  def __str__(self):
    stage_list = list(self.get_stages())
    stage_strs = ["(%s %s)" % (i, " ".join(map(str, s.elements))) for (i, s) in stage_list]
    return "(unit %s)" % " ".join(stage_strs)


# A quote/unquote. The stage indicates which direction to quote in -- less than
# 0 means unquote, greater than means quote.
@plankton.serializable()
class Quote(object):

  def __init__(self, stage, ast):
    self.stage = stage
    self.ast = ast
    self.value = None

  def accept(self, visitor):
    return visitor.visit_quote(self)

  def traverse(self, visitor):
    self.ast.accept(visitor)

  @plankton.replacement
  def get_substitute(self):
    return Literal(self.value)

  def __str__(self):
    return "(@ %s)" % self.ast


@plankton.serializable(plankton.EnvironmentReference.path("ast", "Unquote"))
class Unquote(object):

  @plankton.field('stage')
  @plankton.field('')
  def __init__(self, stage, ast):
    self.stage = stage
    self.value = value


class Import(object):

  def __init__(self, ident=None):
    self.ident = ident

  def get_stage(self):
    return self.ident.stage

  def accept(self, visitor):
    visitor.visit_import(self)

  def apply(self, module):
    fragment = module.get_or_create_fragment(self.ident.stage)
    fragment.add_import(self.ident.path)

  def traverse(self, visitor):
    pass

  def __str__(self):
    return "(import %s)" % self.ident


@plankton.serializable(plankton.EnvironmentReference.path("ast", "IsDeclaration"))
class IsDeclaration(object):

  @plankton.field("subtype")
  @plankton.field("supertype")
  def __init__(self, subtype, supertype):
    self.subtype = subtype
    self.supertype = supertype

  def accept(self, visitor):
    visitor.visit_is_declaration(self)

  def traverse(self, visitor):
    self.subtype.accept(visitor)
    self.supertype.accept(visitor)

  def apply(self, module):
    # TODO: allow past is-declarations.
    fragment = module.get_or_create_fragment(0)
    fragment.add_element(self)


class ModuleManifest(object):

  def __init__(self, ident, sources):
    assert isinstance(sources, Array)
    self.ident = ident
    self.sources = sources

  def get_path(self):
    return self.ident.path

  def get_sources(self):
    for element in self.sources.elements:
      assert isinstance(element, Literal)
      yield element.value
