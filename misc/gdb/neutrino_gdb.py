# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE)

# GDB pretty-printer for neutrino values.

import gdb

# A convenience wrapper around a value.
class Value(object):

  def __init__(self, value):
    self.value = value

  # Returns the value's domain as a string (ie. "vdObject").
  def get_domain(self):
    return str(self.value['as_unknown']['domain'])

  # Returns the integer value of this tagged integer value.
  def get_integer_value(self):
    return int(self.value['as_integer']['value'])

  # Returns the cause of this tagged signal as a string.
  def get_signal_cause(self):
    return str(self.value['as_signal']['cause'])

  # Returns the address value of this object as a gdb.Value.
  def get_object_address(self):
    return self.value['encoded']

  # Returns the index'th value field of this object as a gdb.Value.
  def get_object_field(self, index):
    addr = self.get_object_address()
    type = gdb.Type.pointer(gdb.lookup_type('value_t'))
    ptr = addr.cast(type)
    return Value(ptr[index])

  # Returns the family of this object as a string.
  def get_object_family(self):
    header = self.get_object_field(0)
    family_index = header.get_object_field(1).get_integer_value()
    families = gdb.lookup_type('object_family_t')
    for (name, field) in families.items():
      if field.enumval == family_index:
        return name
    return None


# A printer that holds an atomic value with no children.
class AtomicValuePrinter(object):

  def __init__(self, value):
    self.value = value

  def to_string(self):
    return "n[%s]" % self.value


# A dumb object value printer.
class ObjectValuePrinter(object):

  def __init__(self, family, value):
    self.family = family
    self.value = value

  def to_string(self):
    return "n[#<object: %s>]" % self.family


# Returns the appropriate value printer for the given object wrapper.
def new_object_printer(value):
  family = value.get_object_family()
  return ObjectValuePrinter(family, value)


# Returns the appropriate value printer for the given value wrapper.
def new_value_printer(value):
  domain = value.get_domain()
  if domain == 'vdInteger':
    return AtomicValuePrinter(value.get_integer_value())
  elif domain == 'vdSignal':
    return AtomicValuePrinter('%%<signal: %s>' % value.get_signal_cause())
  elif domain == 'vdObject':
    return new_object_printer(value)
  else:
    return None


# Look up the appropriate pretty-printer for the given value if it's once we
# know how to print, otherwise return None.
def resolve_neutrino_printer(value):
  # Get the value's type.
  type = value.type
  # Strip any typedefs to get to the raw type.
  stripped = type.strip_typedefs()
  # Get the tag of the union or struct or, if it's not one, None.
  tag = stripped.tag
  if tag == 'value_t':
    return new_value_printer(Value(value))
  else:
    return None


# Register the neutrino pretty-printers with gdb.
def register_printers():
  gdb.pretty_printers.append(resolve_neutrino_printer)
