#!/usr/bin/python

# Main entry-point for the neutrino parser.


# Set up appropriate path. Bit of a hack but hey.
import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))


# Remaining imports.
import analysis
import ast
import data
import optparse
import parser
import plankton
import re
import schedule
import token


def parse_plankton_option(option, opt_str, value, parser):
  if value.startswith('p64/'):
    decoder = plankton.Decoder()
    value = decoder.base64decode(value[4:])
  setattr(parser.values, option.dest, value)


# Encapsulates stats relating to the main script.
class Main(object):

  def __init__(self):
    self.args = None
    self.flags = None
    self.units = {}
    self.scheduler = schedule.TaskScheduler()

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
    parser.add_option('--compile', action='callback', type='string', callback=parse_plankton_option)
    return parser

  # Parses the script arguments, storing the values in the appropriate fields.
  def parse_arguments(self):
    parser = self.build_option_parser()
    (self.flags, self.args) = parser.parse_args()
    if self.flags.compile:
      self.compile_flags = self.flags.compile.get_flags()
    else:
      self.compile_flags = None

  # If the filter option is set, filters input and return True. Otherwise
  # returns False.
  def run_filter(self):
    if not self.flags.filter:
      return False
    pattern = re.compile(r'^p64/([a-zA-Z0-9=+/]+)$')
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
    if self.run_filter():
      return
    # First load the units to compile without actually doing it.
    self.schedule_expressions()
    self.schedule_programs()
    self.schedule_files()
    self.schedule_libraries()
    # Then compile everything in the right order.
    self.scheduler.run()

  # Processes any --expression arguments. These are used by the golden tests.
  def schedule_expressions(self):
    self.run_parse_input(self.flags.expression,
        lambda tokens: parser.Parser(tokens, "expression").parse_expression_program())

  # Processes any --program arguments. These are used by the golden tests.
  def schedule_programs(self):
    self.run_parse_input(self.flags.program,
        lambda tokens: parser.Parser(tokens, "program").parse_program())

  # Processes any --file arguments. These are used by the nunit tests.
  def schedule_files(self):
    for filename in self.flags.file:
      source = open(filename, "rt").read()
      tokens = token.tokenize(source)
      unit = parser.Parser(tokens, filename).parse_program()
      self.schedule_for_compile(unit)
      self.schedule_for_output(unit)

  def schedule_libraries(self):
    if not self.compile_flags or not self.compile_flags.build_library:
      return
    library = data.Library()
    library_spec = self.compile_flags.build_library
    for module_file in library_spec['modules']:
      module_basename = os.path.basename(module_file)
      (module_name, ext) = os.path.splitext(module_basename)
      module_source = open(module_file, "rt").read()
      tokens = token.tokenize(module_source)
      unit = parser.Parser(tokens, module_name).parse_program()
      analysis.scope_analyze(unit)
      module = unit.as_unbound_module()
      library.add_module(module.path, module)
    blob = plankton.Encoder().encode(library)
    handle = open(library_spec['out'], 'wb')
    handle.write(blob)
    handle.close()

  # Schedules a unit for compilation at the appropriate time relative to any
  # of its dependencies.
  def schedule_for_compile(self, unit):
    # Analysis doesn't depend on anything else so we can just go ahead and get
    # that out of the way.
    analysis.scope_analyze(unit)

  # Schedules the present program of the given unit to be output to stdout when
  # all the prerequisites for doing so have been run.
  def schedule_for_output(self, unit):
    program = unit.get_present_program()
    self.output_value(program)

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
