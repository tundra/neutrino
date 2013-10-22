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


# A constant literal value.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Literal"))
class Literal(object):

  @plankton.new_field("value")
  def __init__(self, value=None):
    self.value = value

  def accept(self, visitor):
    return visitor.visit_literal(self);

  def traverse(self, visitor):
    pass

  def __str__(self):
    return "(literal %s)" % self.value


# An array expression.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Array"))
class Array(object):

  @plankton.new_field("elements")
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
@plankton.new_serializable()
class Variable(object):

  _LOCAL_HEADER = plankton.EnvironmentReference("ast", "LocalVariable")
  _NAMESPACE_HEADER = plankton.EnvironmentReference("ast", "NamespaceVariable")

  def __init__(self, ident=None, namespace=None, symbol=None):
    self.ident = ident
    self.namespace = namespace
    self.symbol = symbol

  def accept(self, visitor):
    return visitor.visit_variable(self)

  def traverse(self, visitor):
    pass

  def get_name(self):
    return self.ident.get_name()

  @plankton.header
  def get_header(self):
    if self.symbol is None:
      return Variable._NAMESPACE_HEADER
    else:
      return Variable._LOCAL_HEADER

  @plankton.payload
  def get_payload(self):
    if self.symbol is None:
      return {
        'name': self.ident.path,
        'namespace': self.namespace
      }
    else:
      return {
        'symbol': self.symbol
      }

  def __str__(self):
    return "(var %s)" % str(self.ident)


# A multi-method invocation.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Invocation"))
class Invocation(object):

  @plankton.new_field("arguments")
  @plankton.new_field("methodspace")
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
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Argument"))
class Argument(object):

  @plankton.new_field("tag")
  @plankton.new_field("value")
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
@plankton.new_serializable()
class Binding(object):

  @plankton.new_field("symbol")
  @plankton.new_field("value")
  def __init__(self, symbol=None, value=None):
    self.symbol = symbol
    self.value = value


# A sequence of expressions to execute in order, yielding the value of the last
# expression.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Sequence"))
class Sequence(object):

  @plankton.new_field("values")
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
@plankton.new_serializable(plankton.EnvironmentReference("ast", "LocalDeclaration"))
class LocalDeclaration(object):

  @plankton.new_field("symbol")
  @plankton.new_field("value")
  @plankton.new_field("body")
  def __init__(self, ident=None, value=None, body=None, symbol=None):
    self.ident = ident
    self.symbol = symbol
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
    return "(def %s := %s in %s)" % (self.ident, self.value, self.body)


# A symbol that identifies a scoped binding.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Symbol"))
class Symbol(object):

  @plankton.new_field("name")
  def __init__(self, name=None):
    self.name = name


# An individual method parameter.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Parameter"))
class Parameter(object):

  @plankton.new_field("symbol")
  @plankton.new_field("tags")
  @plankton.new_field("guard")
  def __init__(self, ident=None, tags=None, guard=None):
    self.ident = ident
    self.symbol = None
    self.tags = tags
    self.guard = guard

  def accept(self, visitor):
    visitor.visit_parameter(self)

  def traverse(self, visitor):
    self.guard.accept(visitor)

  def get_name(self):
    return self.ident.get_name()

  def __str__(self):
    return "(param (tags %s) (name %s) (guard %s))" % (
        ", ".join(map(str, self.tags)),
        self.ident,
        self.guard)


@plankton.new_serializable(plankton.EnvironmentReference("ast", "Signature"))
class Signature(object):

  @plankton.new_field("parameters")
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


@plankton.new_serializable(plankton.EnvironmentReference("ast", "Guard"))
class Guard(object):

  @plankton.new_field("type")
  @plankton.new_field("value")
  def __init__(self, type=None, value=None):
    self.type = type
    if value is None:
      self.value = None
    else:
      self.value = Quote(-1, value)

  def accept(self, visitor):
    visitor.visit_guard(self)

  def traverse(self, visitor):
    if not self.value is None:
      self.value.accept(visitor)

  def __str__(self):
    if self.value is None:
      return "%s()" % self.type
    else:
      return "%s(%s)" % (self.type, self.value.ast)

  @staticmethod
  def any():
    return Guard(data.Guard._ANY, None)

  @staticmethod
  def eq(value):
    return Guard(data.Guard._EQ, value)

  @staticmethod
  def bound_eq(value):
    result = Guard(data.Guard._EQ, value)
    result.value.value = value
    return result

  # 'is' is a reserved word in python. T t t.
  @staticmethod
  def is_(value):
    return Guard(data.Guard._IS, value)


# An anonymous function. These can be broken down into equivalent new-object
# and set-property calls but that's for later.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Lambda"))
class Lambda(object):

  @plankton.new_field("method")
  def __init__(self, method=None):
    self.method = method

  def accept(self, visitor):
    return visitor.visit_lambda(self)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def __str__(self):
    return "(fn (%s) => %s)" % (self.method.signature, self.method.body)


@plankton.new_serializable(plankton.EnvironmentReference("ast", "Program"))
class Program(object):

  @plankton.new_field("entry_point")
  def __init__(self, elements=None, entry_point=None):
    self.elements = elements
    self.entry_point = entry_point

  def accept(self, visitor):
    return visitor.visit_program(self)

  def __str__(self):
    return "(program %s)" % " ".join(map(str, self.elements))


# A toplevel namespace declaration.
class NamespaceDeclaration(object):

  def __init__(self, ident=None, value=None):
    self.ident = ident
    self.value = value

  # Returns the stage this declaration belongs to.
  def get_stage(self):
    return self.ident.stage

  def accept(self, visitor):
    return visitor.visit_namespace_declaration(self)

  def apply(self, program, helper):
    name = self.ident.get_name()
    value = helper.evaluate(self.value)
    program.module.namespace.add_binding(name, value)

  def traverse(self, visitor):
    self.value.accept(visitor)

  def __str__(self):
    return "(namespace-declaration %s %s)" % (self.ident, self.value)


# Syntax of a method.
@plankton.new_serializable(plankton.EnvironmentReference("ast", "Method"))
class Method(object):

  @plankton.new_field("signature")
  @plankton.new_field("body")
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


# A toplevel function declaration.
class FunctionDeclaration(object):

  def __init__(self, method):
    self.method = method

  def get_stage(self):
    return 0

  def accept(self, visitor):
    return visitor.visit_function_declaration(self)

  def apply(self, program, helper):
    subject = self.method.signature.parameters[0]
    name = subject.ident.get_name()
    namespace = program.module.namespace
    if namespace.has_binding(name):
      value = namespace.lookup(name)
    else:
      value = data.Function(name)
      namespace.add_binding(name, value)
    subject.guard = Guard.bound_eq(value)
    program.module.methodspace.add_method(self.method)

  def traverse(self, visitor):
    self.method.accept(visitor)

  def __str__(self):
    return "(function-declaration %s %s)" % (self.method.signature, self.method.body)


# An individual execution stage.
class Stage(object):

  _BUILTIN_METHODSPACE = data.Key("subject", ("core", "builtin_methodspace"))

  def __init__(self, index, module_name):
    self.index = index
    self.imports = []
    self.namespace = data.Namespace({})
    self.methodspace = data.Methodspace({}, [], self.imports)
    self.module = data.Module(self.namespace, self.methodspace, module_name)
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

  def __init__(self, module_name):
    self.module_name = module_name
    self.entry_point = None
    self.stages = {}
    self.min_stage = 0
    self.max_stage = 0
    self.has_flushed = False
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
      assert not self.has_flushed
      self.stages[index] = self.create_stage(index, self.module_name)
      # If there are stages before or after this make sure this one is properly
      # chained into them, that is, methods from this stage are visible to
      # future stages.
      if self.min_stage < index:
        self.chain_imports(index - 1, index)
      if self.max_stage > index:
        self.chain_imports(index, index + 1)
      self.min_stage = min(self.min_stage, index)
      self.max_stage = max(self.max_stage, index)
    return self.stages[index]

  def create_stage(self, index, module_name):
    return Stage(index, module_name)

  # Ensures that the stage with source_index is imported into the one with
  # the given target index.
  def chain_imports(self, source_index, target_index):
    source = self.get_stage(source_index)
    target = self.get_or_create_stage(target_index)
    target.get_methodspace().add_import(source.get_methodspace())

  def get_present(self):
    return self.get_or_create_stage(0)

  def get_present_program(self):
    self.flush()
    last_stage = self.get_present()
    return Program(last_stage.elements, self.entry_point)

  # Ensure that the oldest stage imports the builtins. This is kind of a hack,
  # there needs to be a better way to ensure that some methodspaces are visible
  # everywhere without creating ambiguities by importing them more than once.
  def flush(self):
    if not self.has_flushed:
      self.has_flushed = True
      self.get_stage(self.min_stage).get_methodspace().add_import(Stage._BUILTIN_METHODSPACE)

  def get_present_module(self):
    self.flush()
    last_stage = self.get_present()
    return last_stage.get_module()

  def accept(self, visitor):
    return visitor.visit_unit(self)

  def __str__(self):
    stage_list = list(self.get_stages())
    stage_strs = ["(%s %s)" % (i, " ".join(map(str, s.elements))) for (i, s) in stage_list]
    return "(unit %s)" % " ".join(stage_strs)


# A quote/unquote. The stage indicates which direction to quote in -- less than
# 0 means unquote, greater than means quote.
@plankton.new_serializable()
class Quote(object):

  def __init__(self, stage=None, ast=None):
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


class Import(object):

  def __init__(self, ident=None):
    self.ident = ident

  def get_stage(self):
    return self.ident.stage

  def accept(self, visitor):
    visitor.visit_import(self)

  def traverse(self, visitor):
    pass

  def apply(self, program, helper):
    name = self.ident.get_name()
    value = helper.lookup_import(name)
    program.module.namespace.add_binding(name, value)
    program.module.methodspace.add_import(value.methodspace)

  def __str__(self):
    return "(import %s)" % self.ident
