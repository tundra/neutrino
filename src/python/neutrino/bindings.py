# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Namespace bindings.

import ast
import interp
import schedule


# Visitor that traverses the syntax tree and resolves any past-references.
class PastVariableResolver(ast.Visitor):

  def __init__(self, unit):
    self.unit = unit

  def visit_quote(self, that):
    that.ast.accept(self)
    if that.stage >= 0:
      # If the unquote is not in the past there's nothing to do.
      return
    value = interp.evaluate(that.ast)
    that.value = value


# Utility that can be used by elements to help apply themselves.
class BindingHelper(object):

  def __init__(self, units):
    self.units = units

  def evaluate(self, expr):
    return interp.evaluate(expr)

  def lookup_import(self, name):
    # For now just single-name imports will be good enough.
    return self.units[name].get_present_module()


# Apply the various declarations to create the program bindings. This is only
# an approximation of the proper program element semantics but it'll do for
# now, possibly until the source compiler is rewritten in neutrino.
def bind(unit, units):
  helper = BindingHelper(units)
  resolver = PastVariableResolver(unit)
  for (index, stage) in unit.get_stages():
    for element in stage.elements:
      # Resolve expressions as we go so that earlier bindings (lexically) are
      # visible to later bindings.
      element.accept(resolver)
      element.apply(stage, helper)
  if not unit.entry_point is None:
    unit.entry_point.accept(resolver)


# A task that binds a particular stage.
class BindTask(schedule.Task):

  def __init__(self, stage, prev_stage, preqs, binder):
    super(BindTask, self).__init__(stage.bind_action)
    self.stage = stage
    self.prev_stage = prev_stage
    self.preqs = preqs
    self.binder = binder

  def run(self):
    self.binder.bind_stage(self.stage, self.prev_stage)

  def get_prerequisites(self):
    return self.preqs


# We've got binders full of units.
class Binder(object):

  def __init__(self, units):
    self.units = units
    self.helper = BindingHelper(units)

  # Creates a new task to be added to the scheduler which binds the given stage
  # after all its dependencies have been bound.
  def new_task(self, unit, stage):
    deps = []
    # Make sure any imports have been bound before this one.
    for path in stage.imports:
      name = path.get_name()
      unit = self.units[name]
      import_bind_action = unit.get_present().bind_action
      deps.append(import_bind_action)
    # Make sure the preceding stages has been bound before this one, if there
    # are any.
    prev_stage = unit.get_stage_before(stage.index)
    if not prev_stage is None:
      deps.append(prev_stage.bind_action)
    return BindTask(stage, prev_stage, deps, self)

  # Resolves past bindings in the given program.
  def bind_program(self, program):
    resolver = PastVariableResolver(None)
    program.entry_point.accept(resolver)

  # Creates bindings for the given stage.
  def bind_stage(self, stage, prev_stage):
    stage.import_previous(prev_stage)
    helper = BindingHelper(self.units)
    resolver = PastVariableResolver(None)
    for element in stage.elements:
      element.accept(resolver)
      element.apply(stage, helper)

