# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# GDB pretty-printer for neutrino values.

import gdb

_DOMAIN_TAG_SIZE = 3
_DOMAIN_TAG_MASK = (1 << _DOMAIN_TAG_SIZE) - 1

_PHYLUM_TAG_SIZE = 8
_PHYLUM_TAG_MASK = (1 << _PHYLUM_TAG_SIZE) - 1

_VALUE_SIZE = 8

# A convenience wrapper around a value.
class Value(object):

  def __init__(self, value):
    self.value = value

  # Returns the underlying GDB value.
  def to_gdb(self):
    return self.value

  # Is this the C NULL pointer? Note that this is different, so very different,
  # from testing whether the value represents neutrino null.
  def is_NULL(self):
    return self.value['encoded'] == 0

  # Returns the value's domain as a string (ie. "vdObject").
  def get_domain(self):
    code = self.get_domain_code()
    return Value.get_enum_name(code, 'value_domain_t')

  # Returns the numeric domain code for this value.
  def get_domain_code(self):
    return self.value['encoded'] & _DOMAIN_TAG_MASK

  # Returns the integer value of this tagged integer value.
  def get_integer_value(self):
    return int(self.value['as_integer']['data']) >> _DOMAIN_TAG_SIZE

  # Returns the cause of this tagged signal as a string.
  def get_condition_cause(self):
    return str(self.value['as_condition']['cause'])

  # Returns the address value of this object as a gdb.Value.
  def get_object_address(self):
    return self.value['encoded'] - 1

  # Returns the integer phylum code of this value.
  def get_phylum_code(self):
    data = self.value['as_custom_tagged']['data']
    return (data >> _DOMAIN_TAG_SIZE) & _PHYLUM_TAG_MASK

  # Returns the integer payload of this value assuming that it's custom tagged.
  def get_custom_tagged_payload(self):
    data = self.value['as_custom_tagged']['data']
    return data >> (_DOMAIN_TAG_SIZE + _PHYLUM_TAG_SIZE)

  # Returns the string name of the phylum of this value assuming that it's
  # custom tagged.
  def get_phylum(self):
    code = self.get_phylum_code()
    return Value.get_enum_name(code, 'custom_tagged_phylum_t')

  # Returns the index'th value field of this object as a gdb.Value.
  def get_object_field(self, index):
    addr = self.get_object_address()
    type = gdb.Type.pointer(gdb.lookup_type('value_t'))
    ptr = addr.cast(type)
    return Value(ptr[index])

  # Returns the family of this object as a string.
  def get_object_family(self):
    header = self.get_object_field(0)
    code = header.get_object_field(1).get_integer_value() << _DOMAIN_TAG_SIZE
    return Value.get_enum_name(code, 'heap_object_family_t')

  # Returns the name of the enum in the given type whose ordinal value is as
  # specified. Returns None if one is not found.
  @staticmethod
  def get_enum_name(ordinal, type):
    enums = gdb.lookup_type(type)
    for (name, field) in enums.items():
      if field.enumval == ordinal:
        return name
    return None

def new_custom_tagged_printer(value):
  phylum = value.get_phylum()
  payload = value.get_custom_tagged_payload()
  if phylum == 'tpNull':
    return StringPrinter('#null')
  elif phylum == 'tpNothing':
    return StringPrinter('#nothing')
  elif phylum == 'tpBoolean':
    if payload == 0:
      return StringPrinter('#false')
    elif payload == 1:
      return StringPrinter('#true')
  return StringPrinter('#<%s (%s)>' % (enum_to_phrase(phylum), payload))


# A printer that holds a display string with no children.
class StringPrinter(object):

  def __init__(self, value):
    self.value = value

  def to_string(self):
    return self.value


# Convert an enum value (say "vdGuard") into smalltalk-style "a Guard". This is
# problematic in general but reads better for debugging.
def enum_to_phrase(enum_name):
  name = enum_name[2:]
  if name[0].lower() in "aeiouy":
    return "an %s" % name
  else:
    return "a %s" % name


# A generic object value printer.
class HeapObjectValuePrinter(object):

  def __init__(self, value):
    self.value = value

  def get_family(self):
    return self.value.get_object_family()

  # Returns the i'th field of this object. Note that the object header is not
  # counted, just like the HEAP_OBJECT_FIELD_OFFSET macro in the implementation
  # so the first field after the header is 0.
  def get_field(self, offset):
    return self.value.get_object_field(1 + offset)

  def to_string(self):
    return "#<%s>" % enum_to_phrase(self.get_family())


# A custom value printer for heap arrays.
class ArrayValuePrinter(HeapObjectValuePrinter):

  def get_length(self):
    return self.get_field(0).get_integer_value()

  def display_hint(self):
    return 'array'

  def children(self):
    for i in range(0, self.get_length()):
      child = self.get_field(1 + i)
      yield (str(i), child.to_gdb())

  def to_string(self):
    return "#<an Array [%s]>" % self.get_length()


class StringValuePrinter(HeapObjectValuePrinter):

  def get_length(self):
    return self.get_field(0).get_integer_value()

  def get_contents(self):
    length = self.get_length()
    addr = self.value.get_object_address()
    type = gdb.Type.pointer(gdb.lookup_type('uint8_t'))
    ptr = addr.cast(type)
    header_size = _VALUE_SIZE * 2
    chars = bytes([int(ptr[header_size + i]) for i in range(0, length)])
    return chars.decode('utf-8')

  def to_string(self):
    return "#<a Utf8 \"%s\">" % self.get_contents()

# Returns the appropriate value printer for the given object wrapper.
def new_heap_object_printer(value):
  family = value.get_object_family()
  if family == 'ofArray':
    return ArrayValuePrinter(value)
  elif family == 'ofUtf8':
    return StringValuePrinter(value)
  else:
    return HeapObjectValuePrinter(value)


# Returns the appropriate value printer for the given value wrapper.
def new_value_printer(value):
  if value.is_NULL():
    # This guy turns up occasionally and confuses the heap object logic if not
    # handled specially.
    return None
  domain = value.get_domain()
  if domain == 'vdInteger':
    return StringPrinter('&' % value.get_integer_value())
  elif domain == 'vdCondition':
    return StringPrinter('%%<condition: %s>' % value.get_condition_cause())
  elif domain == 'vdHeapObject':
    return new_heap_object_printer(value)
  elif domain == 'vdCustomTagged':
    return new_custom_tagged_printer(value)
  else:
    return None


# Look up the appropriate pretty-printer for the given value if it's one we
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
