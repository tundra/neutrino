# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


import framework
import python
import sys
import testdata


_GENERATORS = {
  'python': python.PythonGenerator
}


# Main entry-point.
def generate_tests(gen, out):
  names = []
  gen.emit_preamble(out)
  for (name, value) in testdata.Tests.__dict__.iteritems():
    if name.startswith('gen_'):
      names.append(name)
  names.sort()
  for name in names:
    test = testdata.Tests(gen)
    test.name = name[4:]
    test.assembly = gen.new_assembly()
    case = getattr(test, name)
    try:
      test.begin()
      case()
    finally:
      test.end()
    gen.emit_test(test, out)
  gen.emit_epilogue(out)


def main(args):
  [language] = args
  gen = (_GENERATORS[language])()
  out = framework.OutputStream(gen)
  generate_tests(gen, out)
  print out.flush()


if __name__ == '__main__':
  main(sys.argv[1:])
