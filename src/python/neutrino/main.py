#!/usr/bin/python

# Main entry-point for the neutrino parser.


# Set up appropriate path. Bit of a hack but hey.
import sys
import os.path
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))


# Remaining imports.
import optparse
import parser
import plankton
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
    return parser

  # Parses the script arguments, storing the values in the appropriate fields.
  def parse_arguments(self):
    parser = self.build_option_parser()
    (self.flags, self.args) = parser.parse_args()

  # Main entry-point.
  def run(self):
    self.parse_arguments()
    self.run_expressions()

  # Processes any --expression arguments.
  def run_expressions(self):
    for expr in self.flags.expression:
      tokens = token.tokenize(expr)
      ast = parser.Parser(tokens).parse_expression()
      print plankton.base64encode(ast)


if __name__ == '__main__':
  Main().run()
