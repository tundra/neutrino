# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# GDB pretty-printer for neutrino values.

import gdb

_DOMAIN_TAG_SIZE = 3
_DOMAIN_TAG_MASK = (1 << _DOMAIN_TAG_SIZE) - 1

_PHYLUM_TAG_SIZE = 8
_PHYLUM_TAG_MASK = (1 << _PHYLUM_TAG_SIZE) - 1

_VALUE_SIZE = 8

_OBJECT_TAG = 1
_DERIVED_TAG = 5

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

  def is_integer(self):
    return False

  # Returns the value's domain as a string (ie. "vdObject").
  def get_domain(self):
    code = self.get_domain_code()
    return Value.get_enum_name(code, 'value_domain_t')

  # Returns the numeric domain code for this value.
  def get_domain_code(self):
    return self.value['encoded'] & _DOMAIN_TAG_MASK

  # Returning false here will cause gdb to fall back to the default printer when
  # printing this value.
  def use_fallback_printer(self):
    return False

  # Returning true here will cause gdb to call the get_children method to get
  # this value's children. Otherwise it'll assume the value is atomic.
  def is_array(self):
    return False

  # Generate the value's children.
  def get_array_elements(self):
    return None

  def is_map(self):
    return False

  def get_map_items(self):
    return None

  def is_string(self):
    return False

  def is_nothing(self):
    return False

  # Returns the name of the enum in the given type whose ordinal value is as
  # specified. Returns None if one is not found.
  @staticmethod
  def get_enum_name(ordinal, type):
    enums = gdb.lookup_type(type)
    for (name, field) in enums.items():
      if field.enumval == ordinal:
        return name
    return None

  @staticmethod
  def wrap(value):
    unspecific = Value(value)
    if unspecific.is_NULL():
      return NULL(value)
    domain = unspecific.get_domain()
    if domain == 'vdInteger':
      return IntegerValue(value)
    elif domain == 'vdCondition':
      return ConditionValue(value)
    elif domain == 'vdHeapObject':
      return HeapObjectValue.wrap(value)
    elif domain == 'vdCustomTagged':
      return CustomTaggedValue.wrap(value)
    elif domain == 'vdDerivedObject':
      return DerivedObjectValue(value)
    else:
      return unspecific


# This guy turns up occasionally and confuses the heap object logic if not
# handled specially.
class NULL(Value):

  def use_fallback_printer(self):
    return True


class IntegerValue(Value):

  def is_integer(self):
    return True

  # Returns the integer value of this tagged integer value.
  def get_integer_value(self):
    return int(self.value['as_integer']['data']) >> _DOMAIN_TAG_SIZE

  def __str__(self):
    return "&%i" % self.get_integer_value()


class ConditionValue(Value):

  # Returns the cause of this tagged signal as a string.
  def get_condition_cause(self):
    return str(self.value['as_condition']['cause'])

  def __str__(self):
    return "%%<condition: %s>" % self.get_condition_cause()


class CustomTaggedValue(Value):

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

  def is_nothing(self):
    return self.get_phylum() == 'tpNothing'

  def __str__(self):
    phylum = self.get_phylum()
    payload = self.get_custom_tagged_payload()
    if phylum == 'tpNull':
      return '#null'
    elif phylum == 'tpNothing':
      return '#nothing'
    elif phylum == 'tpBoolean':
      if payload == 0:
        return '#false'
      elif payload == 1:
        return '#true'
    elif phylum == 'tpAllocatedMemory':
      return '#allocated'
    return '#<%s (%s)>' % (enum_to_phrase(phylum), payload)

  @staticmethod
  def wrap(value):
    unspecific = CustomTaggedValue(value)
    phylum = unspecific.get_phylum()
    if phylum == 'tpDerivedObjectAnchor':
      return DerivedObjectAnchorValue(value)
    else:
      return unspecific


_DERIVED_OBJECT_GENUS_TAG_SIZE = 6

class DerivedObjectAnchorValue(CustomTaggedValue):

  def get_genus_code(self):
    payload = self.get_custom_tagged_payload()
    return payload & ((1 << _DERIVED_OBJECT_GENUS_TAG_SIZE) - 1)

  def get_host_offset(self):
    return self.get_custom_tagged_payload() >> _DERIVED_OBJECT_GENUS_TAG_SIZE


class DerivedObjectValue(Value):

  # Returns the integer genus code of this value.
  def get_anchor_address(self):
    return self.value['encoded'] - _DERIVED_TAG

  def get_field(self, index):
    type = gdb.Type.pointer(gdb.lookup_type('value_t'))
    return Value.wrap(self.get_anchor_address().cast(type)[index])

  def get_anchor(self):
    return self.get_field(0)

  def get_genus_code(self):
    return self.get_anchor().get_genus_code()

  def get_genus(self):
    code = self.get_genus_code()
    return Value.get_enum_name(code, 'derived_object_genus_t')

  def get_host(self):
    offset = self.get_anchor().get_host_offset()
    host_addr = self.get_anchor_address() - offset
    host_ptr = host_addr + _OBJECT_TAG
    return Value.wrap(host_ptr.cast(gdb.lookup_type('value_t')))

  def __str__(self):
    genus = self.get_genus()
    return '#<%s of %s>' % (enum_to_phrase(genus), self.get_host())


class HeapObjectValue(Value):

  # Returns the i'th field of this object. Note that the object header is not
  # counted, just like the HEAP_OBJECT_FIELD_OFFSET macro in the implementation
  # so the first field after the header is 0.
  def get_field(self, offset):
    return self.get_object_word(1 + offset)

  # Returns the family of this object as a string.
  def get_object_family(self):
    header = HeapObjectValue(self.get_naked_object_word(0))
    code = header.get_object_word(1).get_integer_value() << _DOMAIN_TAG_SIZE
    return Value.get_enum_name(code, 'heap_object_family_t')

  # Returns the address value of this object as a gdb.Value.
  def get_object_address(self):
    return self.value['encoded'] - _OBJECT_TAG

  # Returns the value stored index words into this object, which must be a heap
  # object. The species is stored at word 0 and the object's fields are stored
  # from word 1.
  def get_object_word(self, index):
    return Value.wrap(self.get_naked_object_word(index))

  # Returns the gdb.Value representation of the value stored index words into
  # this object.
  def get_naked_object_word(self, index):
    addr = self.get_object_address()
    type = gdb.Type.pointer(gdb.lookup_type('value_t'))
    ptr = addr.cast(type)
    return ptr[index]

  @staticmethod
  def wrap(value):
    unspecific = HeapObjectValue(value)
    family = unspecific.get_object_family()
    if family == 'ofArray':
      return ArrayObject(value)
    elif family == 'ofUtf8':
      return StringObject(value)
    elif family == 'ofUnknown':
      return UnknownObject(value)
    elif family == 'ofIdHashMap':
      return IdHashMapObject(value)
    elif family == 'ofPath':
      return PathObject(value)
    elif family == 'ofOperation':
      return OperationObject(value)
    else:
      return unspecific

  def __str__(self):
    return "#<%s>" % enum_to_phrase(self.get_object_family())


class ArrayObject(HeapObjectValue):

  def __len__(self):
    return self.get_field(0).get_integer_value()

  def is_array(self):
    return True

  def get_array_elements(self):
    for i in range(0, len(self)):
      yield self[i]

  def __getitem__(self, index):
    return self.get_field(1 + index)

  def __str__(self):
    return "#<an Array [%s]>" % len(self)


class IdHashMapObject(HeapObjectValue):

  def get_size(self):
    return self.get_field(0).get_integer_value()

  def is_map(self):
    return True

  def get_map_items(self):
    capacity = self.get_field(1).get_integer_value()
    entry_array = self.get_field(3)
    cursor = 0
    while cursor < capacity:
      index = 3 * cursor
      hash = entry_array[index + 1]
      if hash.is_integer():
        key = entry_array[index]
        value = entry_array[index + 2]
        yield (key, value)
      cursor += 1

  def __str__(self):
    return "#<an IdHashMap {%s}>" % self.get_size()


class StringObject(HeapObjectValue):

  def get_length(self):
    return self.get_field(0).get_integer_value()

  def get_contents(self):
    length = self.get_length()
    addr = self.get_object_address()
    type = gdb.Type.pointer(gdb.lookup_type('uint8_t'))
    ptr = addr.cast(type)
    header_size = _VALUE_SIZE * 2
    chars = bytes([int(ptr[header_size + i]) for i in range(0, length)])
    return '"%s"' % chars.decode('utf-8')

  def __str__(self):
    return self.get_contents()


class UnknownObject(HeapObjectValue):

  def __str__(self):
    header = self.get_field(0)
    return "#<an Unknown %s>" % str(header)


class PathObject(HeapObjectValue):

  def __str__(self):
    parts = []
    current = self
    while True:
      head = current.get_field(0)
      if head.is_nothing():
        break
      else:
        parts.append(str(head))
        current = current.get_field(1)
    return "#<a Path %s>" % ":".join(parts)


_OPERATION_ASSIGN_TAG = 1
_OPERATION_CALL_TAG = 2
_OPERATION_INDEX_TAG = 3
_OPERATION_INFIX_TAG = 4
_OPERATION_PREFIX_TAG = 5
_OPERATION_SUFFIX_TAG = 6


class OperationObject(HeapObjectValue):

  def __str__(self):
    value = str(self.get_field(1))
    tag = self.get_field(0).get_integer_value()
    name = None
    if tag == _OPERATION_ASSIGN_TAG:
      name = "%s:=" % value
    elif tag == _OPERATION_CALL_TAG:
      name = "()"
    elif tag == _OPERATION_INDEX_TAG:
      name = "[]"
    elif tag == _OPERATION_INFIX_TAG:
      name = ".%s" % value
    elif tag == _OPERATION_PREFIX_TAG:
      name = "%s()" % value
    elif tag == _OPERATION_SUFFIX_TAG:
      name = "()%s" % value
    else:
      name = str(value)
    return "#<an Operation %s>" % name


# Convert an enum value (say "vdGuard") into smalltalk-style "a Guard". This is
# problematic in general but reads better for debugging.
def enum_to_phrase(enum_name):
  name = enum_name[2:]
  if name[0].lower() in "aeiouy":
    return "an %s" % name
  else:
    return "a %s" % name


# Adaptor that tells gdb how to display a wrapped value.
class ValuePrinter(object):

  def __init__(self, value):
    self.value = value

  def to_string(self):
    return str(self.value)

  @staticmethod
  def wrap(value):
    if value.use_fallback_printer():
      return None
    # Annoyingly, gdb behaves differently based on which methods exist, not what
    # they yield, So we need several different implementations to control how
    # the values are displayed.
    if value.is_array():
      return ArrayPrinter(value)
    elif value.is_map():
      return MapPrinter(value)
    elif value.is_string():
      return StringPrinter(value)
    else:
      return ValuePrinter(value)


class ArrayPrinter(ValuePrinter):

  def display_hint(self):
    return "array"

  def children(self):
    index = 0
    for child in self.value.get_array_elements():
      yield ("[%i]" % index, child.to_gdb())
      index += 1


class MapPrinter(ValuePrinter):

  def display_hint(self):
    return "map"

  def children(self):
    index = 0
    for (key, value) in self.value.get_map_items():
      yield ("key[%i]" % index, key.to_gdb())
      yield ("value[%i]" % index, value.to_gdb())
      index += 1


class StringPrinter(ValuePrinter):

  def display_hint(self):
    return "string"


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
    return ValuePrinter.wrap(Value.wrap(value))
  else:
    return None


# Register the neutrino pretty-printers with gdb.
def register_printers():
  gdb.pretty_printers.append(resolve_neutrino_printer)
