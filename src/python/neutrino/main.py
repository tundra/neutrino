#!/usr/bin/python

# Main entry-point for the neutrino parser.


# Set up appropriate path. Bit of a hack but hey.
import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))


# Remaining imports.
import analysis
import bindings
import optparse
import parser
import plankton
import re
import token


# Encapsulates stats relating to the main script.
class Main(object):

  def __init__(self):
    self.args = None
    self.flags = None

  # Builds and returns a new option parser for the flags understood by this
  # script.
  def build_option_parser(self):
    parser = optparse.OptionParser()
    parser.add_option('--expression', action='append', default=[])
    parser.add_option('--program', action='append', default=[])
    parser.add_option('--filter', action='store_true', default=False)
    parser.add_option('--base64', action='store_true', default=False)
    parser.add_option('--disass', action='store_true', default=False)
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
    if self.run_filter():
      return
    self.run_expressions()
    self.run_programs()

  # Processes any --expression arguments.
  def run_expressions(self):
    self.run_parse_input(self.flags.expression,
        lambda tokens: parser.Parser(tokens).parse_expression_program())

  # Processes any --program arguments.
  def run_programs(self):
    self.run_parse_input(self.flags.program,
        lambda tokens: parser.Parser(tokens).parse_program())

  def run_parse_input(self, inputs, parse_thunk):
    for expr in inputs:
      tokens = token.tokenize(expr)
      ast = parse_thunk(tokens)
      analysis.analyze(ast)
      bindings.bind(ast)
      self.output_value(ast)


  def output_value(self, value):
    encoder = plankton.Encoder()
    if self.flags.base64:
      print "p64/%s" % encoder.base64encode(value)
    else:
      sys.stdout.write(encoder.encode(value))


if __name__ == '__main__':
  Main().run()
