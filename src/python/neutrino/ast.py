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

  def visit_invocation(self, that):
    self.visit_ast(that)

  def visit_argument(self, that):
    self.visit_ast(that)

  def visit_sequence(self, that):
    self.visit_ast(that)

  def visit_local_declaration(self, that):
    self.visit_ast(that)

  def visit_namespace_declaration(self, that):
    self.visit_ast(that)

  def visit_method_declaration(self, that):
    self.visit_ast(that)

  def visit_signature(self, that):
    self.visit_ast(that)

  def visit_parameter(self, that):
    self.visit_ast(that)

  def visit_method(self, that):
    self.visit_ast(that)

  def visit_guard(self, that):
    self.visit_ast(that)

  def visit_past_unquote(self, that):
    self.visit_ast(that)


# A constant literal value.
@plankton.serializable(("ast", "Literal"))
class Literal(object):

  @plankton.field("value")
  def __init__(self, value=None):
    self.value = value

  def accept(self, visitor):
    return visitor.visit_literal(self);

  def traverse(self, visitor):
    pass

  def __str__(self):
    return "(literal %s)" % self.value


# An array expression.
@plankton.serializable(("ast", "Array"))
class Array(object):

  @plankton.field("elements")
  def __init__(self, elements=None):
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
@plankton.virtual
@plankton.serializable(("ast", "LocalVariable"), ("ast", "NamespaceVariable"))
class Variable(object):

  _LOCAL_HEADER = plankton.EnvironmentPlaceholder(("ast", "LocalVariable"))
  _NAMESPACE_HEADER = plankton.EnvironmentPlaceholder(("ast", "NamespaceVariable"))

  # We don't need to serialize the name since the symbol holds the name.
  @plankton.field("symbol")
  def __init__(self, name=None, namespace=None, symbol=None):
    self.name = name
    self.namespace = namespace
    self.symbol = symbol

  def accept(self, visitor):
    return visitor.visit_variable(self)

  def traverse(self, visitor):
    pass

  def get_header(self):
    if self.symbol is None:
      return Variable._NAMESPACE_HEADER
    else:
      return Variable._LOCAL_HEADER

  def get_payload(self):
    if self.symbol is None:
      return {
        'name': self.name.path,
        'namespace': self.namespace
      }
    else:
      return {'symbol': self.symbol}

  def __str__(self):
    return "(var %s)" % str(self.name)


# A multi-method invocation.
@plankton.serializable(("ast", "Invocation"))
class Invocation(object):

  @plankton.field("arguments")
  @plankton.field("methodspace")
  def __init__(self, arguments=None, methodspace=None):
    self.arguments = arguments
    self.methodspace = methodspace

  def accept(self, visitor):
    return visitor.visit_invocation(self)

  def traverse(self, visitor):
    for argument in self.arguments:
      argument.accept(visitor)

  def __str__(self):
    return "(call %s)" % " ".join(map(str, self.arguments))


# An individual argument to an invocation.
@plankton.serializable(("ast", "Argument"))
class Argument(object):

  @plankton.field("tag")
  @plankton.field("value")
  def __init__(self, tag=None, value=None):
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
  def __init__(self, symbol=None, value=None):
    self.symbol = symbol
    self.value = value


# A sequence of expressions to execute in order, yielding the value of the last
# expression.
@plankton.serializable(("ast", "Sequence"))
class Sequence(object):

  @plankton.field("values")
  def __init__(self, values=None):
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
@plankton.serializable(("ast", "LocalDeclaration"))
class LocalDeclaration(object):

  @plankton.field("symbol")
  @plankton.field("value")
  @plankton.field("body")
  def __init__(self, name=None, value=None, body=None, symbol=None):
    self.name = name
    self.symbol = symbol
    self.value = value
    self.body = body

  def accept(self, visitor):
    return visitor.visit_local_declaration(self)

  def traverse(self, visitor):
    self.value.accept(visitor)
    self.body.accept(visitor)

  def __str__(self):
    return "(def %s := %s in %s)" % (self.name, self.value, self.body)


# A symbol that identifies a scoped binding.
@plankton.serializable(("ast", "Symbol"))
class Symbol(object):

  @plankton.field("name")
  def __init__(self, name=None):
    self.name = str(name)


# An individual method parameter.
@plankton.serializable(("ast", "Parameter"))
class Parameter(object):

  @plankton.field("symbol")
  @plankton.field("tags")
  @plankton.field("guard")
  def __init__(self, name=None, tags=None, guard=None):
    self.name = name
    self.symbol = None
    self.tags = tags
    self.guard = guard

  def accept(self, visitor):
    visitor.visit_parameter(self)

  def traverse(self, visitor):
    self.guard.accept(visitor)

  def __str__(self):
    return "(param (tags %s) (name %s) (guard %s))" % (
        ", ".join(map(str, self.tags)),
        self.name,
        self.guard)


@plankton.serializable(("ast", "Signature"))
class Signature(object):

  @plankton.field("parameters")
  def __init__(self, parameters=None):
    self.parameters = parameters

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


@plankton.serializable(("ast", "Guard"))
class Guard(object):

  @plankton.field("type")
  @plankton.field("value")
  def __init__(self, type=None, value=None):
    self.type = type
    # TODO: this is just me not being clear on how quoting should work with
    #   guard values. I should really sort that out.
    if value is None:
      self.value = None
    elif isinstance(value, PastUnquote):
      self.value = value
    else:
      self.value = PastUnquote(-1, value)

  def accept(self, visitor):
    visitor.visit_guard(self)

  def traverse(self, visitor):
    if not self.value is None:
      self.value.accept(visitor)

  def __str__(self):
    return "%s(%s)" % (self.type, self.value)

  @staticmethod
  def any():
    return Guard(data.Guard._ANY, None)

  @staticmethod
  def eq(value):
    return Guard(data.Guard._EQ, value)

  # 'is' is a reserved word in python. T t t.
  @staticmethod
  def is_(value):
    return Guard(data.Guard._IS, value)


# An anonymous function. These can be broken down into equivalent new-object
# and set-property calls but that's for later.
@plankton.serializable(("ast", "Lambda"))
class Lambda(object):

  @plankton.field("method")
  def __init__(self, method=None):
    self.method = method

  def accept(self, visitor):
    return visitor.visit_lambda(self)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def __str__(self):
    return "(fn (%s) => %s)" % (self.method.signature, self.method.body)


@plankton.serializable(("ast", "Path"))
class Path(object):

  @plankton.field("parts")
  def __init__(self, parts=None):
    self.parts = parts

  def __eq__(self, that):
    return (isinstance(that, Path) and
            self.parts == that.parts)


@plankton.serializable(("ast", "Name"))
class Name(object):

  @plankton.field("path")
  @plankton.field("stage")
  def __init__(self, stage=None, path=None):
    self.stage = stage
    self.path = tuple(path)

  def __hash__(self):
    return hash(self.stage) ^ hash(self.path)

  def __eq__(self, that):
    return (isinstance(that, Name) and
            self.stage == that.stage and
            self.path == that.path)

  def __str__(self):
    if self.stage < 0:
      prefix = "@" * -self.stage
    else:
      prefix = "$" * (self.stage + 1)
    return "(name %s %s)" % (prefix, ":".join(self.path))


@plankton.serializable(("ast", "Program"))
class Program(object):

  @plankton.field("entry_point")
  @plankton.field("module")
  def __init__(self, elements=None, entry_point=None, module=None):
    self.elements = elements
    self.entry_point = entry_point
    self.module = module

  def accept(self, visitor):
    return visitor.visit_program(self)

  def __str__(self):
    return "(program %s)" % " ".join(map(str, self.elements))


# A toplevel namespace declaration.
class NamespaceDeclaration(object):

  def __init__(self, name=None, value=None):
    self.name = name
    self.value = value

  # Returns the stage this declaration belongs to.
  def get_stage(self):
    return self.name.stage

  def accept(self, visitor):
    return visitor.visit_namespace_declaration(self)

  def apply(self, program, helper):
    name = tuple(self.name.path)
    value = helper.evaluate(self.value)
    program.module.namespace.add_binding(name, value)

  def traverse(self, visitor):
    self.value.accept(visitor)

  def __str__(self):
    return "(namespace-declaration %s %s)" % (self.name, self.value)


# Syntax of a method.
@plankton.serializable(("ast", "Method"))
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
class MethodDeclaration(object):

  def __init__(self, method):
    self.method = method

  def get_stage(self):
    return 0

  def accept(self, visitor):
    return visitor.visit_method_declaration(self)

  def apply(self, program, helper):
    program.module.methodspace.add_method(self.method)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def __str__(self):
    return "(method-declaration %s %s)" % (self.method.signature, self.method.body)


# An individual execution stage.
class Stage(object):

  _BUILTIN_METHODSPACE = data.Key("subject", ("core", "builtin_methodspace"))

  def __init__(self, index):
    self.index = index
    self.imports = [Stage._BUILTIN_METHODSPACE]
    self.namespace = data.Namespace({})
    self.methodspace = data.Methodspace({}, [], self.imports)
    self.module = data.Module(self.namespace, self.methodspace)
    self.elements = []

  def __str__(self):
    return "#<stage %i>" % self.index

  # Returns all the imports for this stage.
  def get_imports(self):
    return self.imports

  # Returns this stage's namespace.
  def get_namespace(self):
    return self.namespace

  # Returns this stage's methodspace.
  def get_methodspace(self):
    return self.methodspace

  # Returns the module that encapsulates this stage's data.
  def get_module(self):
    return self.module


# A full compilation unit.
class Unit(object):

  def __init__(self):
    self.entry_point = None
    self.stages = {}
    self.get_or_create_stage(0)

  def add_element(self, stage, *elements):
    target = self.get_or_create_stage(stage)
    for element in elements:
      target.elements.append(element)
    return self

  def get_stages(self):
    for index in sorted(self.stages.keys()):
      yield (index, self.stages[index])

  def set_entry_point(self, value):
    self.entry_point = value
    return self

  def get_stage(self, index):
    return self.stages[index]

  def get_or_create_stage(self, index):
    if not index in self.stages:
      self.stages[index] = Stage(index)
    return self.stages[index]

  def get_present(self):
    return self.get_or_create_stage(0)

  def get_present_program(self):
    last_stage = self.get_present()
    return Program(last_stage.elements, self.entry_point, last_stage.get_module())

  def accept(self, visitor):
    return visitor.visit_unit(self)

  def __str__(self):
    stage_list = list(self.get_stages())
    stage_strs = ["(%s %s)" % (i, " ".join(map(str, s.elements))) for (i, s) in stage_list]
    return "(unit %s)" % " ".join(stage_strs)


@plankton.substitute
@plankton.serializable()
class PastUnquote(object):

  def __init__(self, stage=None, ast=None):
    self.stage = stage
    self.ast = ast
    self.value = None

  def accept(self, visitor):
    return visitor.visit_past_unquote(self)

  def traverse(self, visitor):
    self.ast.accept(visitor)

  def get_substitute(self):
    return Literal(self.value)

  def __str__(self):
    return "(@ %s)" % self.ast
