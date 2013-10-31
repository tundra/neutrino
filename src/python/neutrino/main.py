#!/usr/bin/python

# Main entry-point for the neutrino parser.


# Set up appropriate path. Bit of a hack but hey.
import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))


# Remaining imports.
import analysis
import ast
import bindings
import optparse
import parser
import plankton
import re
import schedule
import token


# Encapsulates stats relating to the main script.
class Main(object):

  def __init__(self):
    self.args = None
    self.flags = None
    self.units = {}
    self.scheduler = schedule.TaskScheduler()
    self.binder = bindings.Binder(self.units)

  # Builds and returns a new option parser for the flags understood by this
  # script.
  def build_option_parser(self):
    parser = optparse.OptionParser()
    parser.add_option('--expression', action='append', default=[])
    parser.add_option('--program', action='append', default=[])
    parser.add_option('--file', action='append', default=[])
    parser.add_option('--filter', action='store_true', default=False)
    parser.add_option('--base64', action='store_true', default=False)
    parser.add_option('--disass', action='store_true', default=False)
    parser.add_option('--module', action='append', default=[])
    return parser

  # Parses the script arguments, storing the values in the appropriate fields.
  def parse_arguments(self):
    parser = self.build_option_parser()
    (self.flags, self.args) = parser.parse_args()

  # If the filter option is set, filters input and return True. Otherwise
  # returns False.
  def run_filter(self):
    if not self.flags.filter:
      return False
    pattern = re.compile(r'^p64/([a-zA-Z0-9=+]+)$')
    for line in sys.stdin:
      match = pattern.match(line.strip())
      if match:
        code = match.group(1)
        decoder = plankton.Decoder({})
        if self.flags.disass:
          print decoder.base64disassemble(code)
        else:
          data = decoder.base64decode(code)
          print plankton.stringify(data)
      else:
        print line
    return True

  # Main entry-point.
  def run(self):
    self.parse_arguments()
    self.load_modules()
    if self.run_filter():
      return
    # First load the units to compile without actually doing it.
    self.schedule_expressions()
    self.schedule_programs()
    self.schedule_files()
    # Then compile everything in the right order.
    self.scheduler.run()

  # Load any referenced modules.
  def load_modules(self):
    # Load the modules before scheduling them since they all have to be
    # available, though not necessarily compiled, before we can set them up to
    # be compiled in the right order.
    for arg in self.flags.module:
      filename = os.path.basename(arg)
      (module_name, ext) = os.path.splitext(filename)
      contents = None
      with open(arg, "rt") as stream:
        contents = stream.read()
      tokens = token.tokenize(contents)
      unit = parser.Parser(tokens, module_name).parse_program()
      self.units[module_name] = unit
    # Then schedule them to be compiled.
    for unit in self.units.values():
      self.schedule_for_compile(unit)

  # Processes any --expression arguments.
  def schedule_expressions(self):
    self.run_parse_input(self.flags.expression,
        lambda tokens: parser.Parser(tokens, "expression").parse_expression_program())

  # Processes any --program arguments.
  def schedule_programs(self):
    self.run_parse_input(self.flags.program,
        lambda tokens: parser.Parser(tokens, "program").parse_program())

  # Processes any --file arguments.
  def schedule_files(self):
    for filename in self.flags.file:
      source = open(filename, "rt").read()
      tokens = token.tokenize(source)
      unit = parser.Parser(tokens, filename).parse_program()
      self.schedule_for_compile(unit)
      self.schedule_for_output(unit)

  # Schedules a unit for compilation at the appropriate time relative to any
  # of its dependencies.
  def schedule_for_compile(self, unit):
    # Analysis doesn't depend on anything else so we can just go ahead and get
    # that out of the way.
    analysis.scope_analyze(unit)
    for (index, stage) in unit.get_stages():
      self.scheduler.add_task(self.binder.new_task(unit, stage))

  # Schedules the present program of the given unit to be output to stdout when
  # all the prerequisites for doing so have been run.
  def schedule_for_output(self, unit):
    def output_unit():
      program = unit.get_present_program()
      self.binder.bind_program(program)
      self.output_value(program)
    self.scheduler.add_task(schedule.SimpleTask(
      schedule.ActionId("output program"),
      [unit.get_present().get_bind_action()],
      output_unit))

  def run_parse_input(self, inputs, parse_thunk):
    for expr in inputs:
      tokens = token.tokenize(expr)
      unit = parse_thunk(tokens)
      self.schedule_for_compile(unit)
      self.schedule_for_output(unit)

  def output_value(self, value):
    encoder = plankton.Encoder()
    if self.flags.base64:
      print "p64/%s" % encoder.base64encode(value)
    else:
      sys.stdout.write(encoder.encode(value))


if __name__ == '__main__':
  Main().run()
