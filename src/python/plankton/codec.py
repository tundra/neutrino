# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Utilities for encoding and decoding of plankton data.


import base64


_INT32_TAG = 0
_STRING_TAG = 1
_ARRAY_TAG = 2
_MAP_TAG = 3
_NULL_TAG = 4
_TRUE_TAG = 5
_FALSE_TAG = 6
_OBJECT_TAG = 7
_REFERENCE_TAG = 8
_ENVIRONMENT_TAG = 9


# Dispatches to the appropriate method on the given visitor depending on the
# type and value of the data object.
def visit_data(data, visitor):
  t = type(data)
  if t == int:
    return visitor.visit_int(data)
  elif t == str:
    return visitor.visit_string(data)
  elif (t == list) or (t == tuple):
    return visitor.visit_array(data)
  elif t == dict:
    return visitor.visit_map(data)
  elif data is None:
    return visitor.visit_null(data)
  elif (data is True) or (data is False):
    return visitor.visit_bool(data)
  else:
    return visitor.visit_object(data)


# Encapsulates state relevant to writing plankton data.
class DataOutputStream(object):

  def __init__(self, resolver):
    self.bytes = bytearray()
    self.object_index = {}
    self.object_offset = 0
    self.resolver = resolver

  # Writes a value to the stream.
  def write_object(self, obj):
    visit_data(obj, self)

  # Emit a tagged integer.
  def visit_int(self, obj):
    self._add_byte(_INT32_TAG)
    self._encode_int32(obj)

  # Emit a tagged string.
  def visit_string(self, value):
    self._add_byte(_STRING_TAG)
    self._encode_uint32(len(value))
    self.bytes.extend(value)

  # Emit a tagged array.
  def visit_array(self, value):
    self._add_byte(_ARRAY_TAG)
    self._encode_uint32(len(value))
    for elm in value:
      self.write_object(elm)

  # Emit a tagged map.
  def visit_map(self, value):
    self._add_byte(_MAP_TAG)
    self._encode_uint32(len(value))
    for (k, v) in value.items():
      self.write_object(k)
      self.write_object(v)

  # Emit a tagged null.
  def visit_null(self, obj):
    self._add_byte(_NULL_TAG)

  # Emit a tagged boolean.
  def visit_bool(self, obj):
    if obj:
      self._add_byte(_TRUE_TAG)
    else:
      self._add_byte(_FALSE_TAG)

  def acquire_offset(self):
    index = self.object_offset
    self.object_offset += 1
    return index

  def resolve_object(self, obj):
    if isinstance(obj, EnvironmentPlaceholder):
      return obj.key
    else:
      return (self.resolver)(obj)

  # Emit a tagged object or reference.
  def visit_object(self, obj):
    if obj in self.object_index:
      # We've already emitted this object so just reference the previous
      # instance.
      self._add_byte(_REFERENCE_TAG)
      offset = self.object_offset - self.object_index[obj] - 1
      self._encode_uint32(offset)
    else:
      resolved = self.resolve_object(obj)
      if resolved is None:
        # This object is not an environment reference so just emit it
        # directly.
        name = obj.__class__.__name__
        desc = _DESCRIPTORS.get(name, None)
        if desc is None:
          raise Exception(obj)
        index = self.acquire_offset()
        self._add_byte(_OBJECT_TAG)
        self.object_index[obj] = -1
        if desc.virtual:
          self.write_object(obj.get_header())
          self.object_index[obj] = index
          self.write_object(obj.get_payload())
        else:
          self.write_object(desc.get_header())
          self.object_index[obj] = index
          self.write_object(desc.get_payload(obj))
      else:
        index = self.acquire_offset()
        self._add_byte(_ENVIRONMENT_TAG)
        self.object_index[obj] = -1
        self.write_object(resolved)
        self.object_index[obj] = index

  # Writes a naked unsigned int32.
  def _encode_uint32(self, value):
    assert value >= 0
    while value > 0x7F:
      part = value & 0x7F
      self._add_byte(part | 0x80)
      value = value >> 7
    self._add_byte(value)

  # Writes a naked signed int32.
  def _encode_int32(self, value):
    self._encode_uint32((value << 1) ^ (value >> 31))

  # Adds a single byte to the stream.
  def _add_byte(self, byte):
    self.bytes.append(byte)


# Encapsulates state relevant to reading plankton data.
class DataInputStream(object):

  def __init__(self, bytes, descriptors, access):
    self.bytes = bytes
    self.cursor = 0
    self.object_index = {}
    self.object_offset = 0
    self.descriptors = descriptors
    self.access = access

  # Reads the next value from the stream.
  def read_object(self):
    tag = self._get_byte()
    if tag == _INT32_TAG:
      return self._decode_int32()
    elif tag == _STRING_TAG:
      return self._decode_string()
    elif tag == _ARRAY_TAG:
      return self._decode_array()
    elif tag == _MAP_TAG:
      return self._decode_map()
    elif tag == _NULL_TAG:
      return None
    elif tag == _TRUE_TAG:
      return True
    elif tag == _FALSE_TAG:
      return False
    elif tag == _OBJECT_TAG:
      return self._decode_object()
    elif tag == _REFERENCE_TAG:
      return self._decode_reference()
    elif tag == _ENVIRONMENT_TAG:
      return self._decode_environment()
    else:
      raise Exception(tag)

  def disassemble_object(self, indent=""):
    tag = self._get_byte()
    if tag == _INT32_TAG:
      return "%sint32 %i" % (indent, self._decode_int32())
    elif tag == _STRING_TAG:
      return "%sstring '%s'" % (indent, self._decode_string())
    elif tag == _ARRAY_TAG:
      return self._disassemble_array(indent)
    elif tag == _MAP_TAG:
      return self._disassemble_map(indent)
    elif tag == _NULL_TAG:
      return "%snull" % indent
    elif tag == _TRUE_TAG:
      return "%strue" % indent
    elif tag == _FALSE_TAG:
      return "%sfalse" % indent
    elif tag == _OBJECT_TAG:
      return self._disassemble_object(indent)
    elif tag == _REFERENCE_TAG:
      return self._disassemble_reference(indent)
    elif tag == _ENVIRONMENT_TAG:
      return self._disassemble_environment(indent)
    else:
      return str(tag)


  # Reads a naked unsigned from the stream.
  def _decode_uint32(self):
    current = 0xFF
    result = 0
    offset = 0
    while (current & 0x80) != 0:
      current = self._get_byte()
      result = result | ((current & 0x7f) << offset)
      offset += 7
    return result

  # Reads a naked integer from the stream.
  def _decode_int32(self):
    value = self._decode_uint32()
    return (value >> 1) ^ ((-(value & 1)))

  # Reads a naked string from the stream.
  def _decode_string(self):
    length = self._decode_uint32()
    bytes = bytearray()
    for i in xrange(0, length):
      bytes.append(self._get_byte())
    return str(bytes)

  # Reads a naked array from the stream.
  def _decode_array(self):
    length = self._decode_uint32()
    result = []
    for i in xrange(0, length):
      result.append(self.read_object())
    return result

  def _disassemble_array(self, indent):
    length = self._decode_uint32()
    children = []
    for i in xrange(0, length):
      new_indent = "%s%-2i" % (indent, i)
      children.append(self.disassemble_object(new_indent))
    return "%sarray %i\n%s" % (indent, length, "\n".join(children))

  # Reads a naked map from the stream.
  def _decode_map(self):
    length = self._decode_uint32()
    result = {}
    for i in xrange(0, length):
      k = self.read_object()
      v = self.read_object()
      result[k] = v
    return result

  def _disassemble_map(self, indent):
    length = self._decode_uint32()
    children = []
    for i in xrange(0, length):
      key = self.disassemble_object(indent + ": ")
      value = self.disassemble_object(indent + "  ")
      children.append(key + "\n" + value)
    return "%smap %i\n%s" % (indent, length, "\n".join(children))

  # Reads a naked object from the stream.
  def _decode_object(self):
    index = self.grab_index()
    self.object_index[index] = None
    header = self.read_object()
    desc = self.descriptors.get(header, None)
    if desc is None:
      desc = _DEFAULT_DESCRIPTOR
    instance = desc.new_instance(header)
    self.object_index[index] = instance
    payload = self.read_object()
    desc.apply_payload(instance, payload)
    return instance

  def _disassemble_object(self, indent):
    index = self.grab_index()
    header = self.disassemble_object(indent + "^ ")
    payload = self.disassemble_object(indent + "  ")
    return "%sobject (@%i)\n%s\n%s" % (indent, index, header, payload)

  def access_environment(self, key):
    if (type(key) is list) and not self.descriptors.get(tuple(key), None) is None:
      return EnvironmentPlaceholder(tuple(key))
    else:
      return (self.access)(key)

  def _decode_environment(self):
    index = self.grab_index()
    self.object_index[index] = None
    key = self.read_object()
    value = self.access_environment(key)
    self.object_index[index] = value
    return value

  # Acquires the next object index.
  def grab_index(self):
    result = self.object_offset
    self.object_offset += 1
    return result

  def _disassemble_environment(self, indent):
    index = self.grab_index()
    key = self.disassemble_object(indent + "  ")
    return "%senvironment (@%i)\n%s" % (indent, index, key)

  # Reads a raw object reference from the stream.
  def _decode_reference(self):
    offset = self._decode_uint32()
    index = self.object_offset - offset - 1
    return self.object_index[index]

  def _disassemble_reference(self, indent):
    offset = self._decode_uint32()
    index = self.object_offset - offset - 1
    return "%sreference %i (=@%i)" % (indent, offset, index)


  # Reads a single byte from the stream.
  def _get_byte(self):
    result = self.bytes[self.cursor]
    self.cursor += 1
    return result


# A parsed object of an unknown type.
class UnknownObject(object):

  def __init__(self, header):
    self.header = header
    self.payload = None

  def set_payload(self, payload):
    self.payload = payload

  def __str__(self):
    return "#<%s: %s>" % (self.header, self.payload)


# The default object descriptor which is used if no more specific descriptor
# can be found.
class DefaultDescriptor(object):

  def new_instance(self, header):
    return UnknownObject(header)

  def apply_payload(self, instance, payload):
    instance.set_payload(payload)


_DESCRIPTORS = {}
_DEFAULT_DESCRIPTOR = DefaultDescriptor()


# An object that will always be resolved as an environment reference.
class EnvironmentPlaceholder(object):

  def __init__(self, key):
    self.key = key

  def __hash__(self):
    return ~hash(self.key)

  def __eq__(self, that):
    if not isinstance(that, EnvironmentPlaceholder):
      return False
    else:
      return self.key == that.key


# A description of how to serialize and deserialize values of a particular
# type.
class ObjectDescriptor(object):

  def __init__(self, klass, environment):
    self.klass = klass
    if environment is None:
      self.header = self.klass.__name__
    else:
      self.header = EnvironmentPlaceholder(environment)
    self.fields = None
    self.virtual = False

  # Returns the header to use when encoding the given instance
  def get_header(self):
    return self.header

  # Returns the data payload to use when encoding the given instance.
  def get_payload(self, instance):
    fields = {}
    for field in self._field_names():
      fields[field] = getattr(instance, field)
    return fields

  # Returns the list of names of serializable instance fields.
  def _field_names(self):
    if self.fields is None:
      init = self.klass.__init__
      self.fields = list(init._plankton_fields_)
      self.fields.reverse()
    return self.fields

  # Creates a new empty instance of this type of object.
  def new_instance(self, header):
    return (self.klass)()

  # Sets the data in the given instance based on the given payload.
  def apply_payload(self, instance, payload):
    for (name, value) in payload.iteritems():
      setattr(instance, name, value)


# Marks the given class as serializable.
def serializable(environment=None):
  def callback(klass):
    descriptor = ObjectDescriptor(klass, environment)
    # We need to be able to access the descriptor through the class' name,
    # that's how we get access to it when serializing an instance.
    _DESCRIPTORS[klass.__name__] = descriptor
    if not environment is None:
      # If there is an environment we also key the descriptor under the
      # environment key such that when we meet the key during deserialization
      # we know there's a descriptor.
      _DESCRIPTORS[environment] = descriptor
      # Finally we key it under the environment placeholder such that once
      # the header has been read as an environment placeholder the object
      # construction code can get it that way.
      _DESCRIPTORS[EnvironmentPlaceholder(environment)] = descriptor
    return klass
  return callback


# Marks the descriptor as requiring the object to be transformed during
# serialization.
def virtual(klass):
  descriptor = _DESCRIPTORS[klass.__name__]
  descriptor.virtual = True
  return klass


# When used to decorate __init__ indicates that a serializable class has the
# given field.
def field(name):
  def callback(fun):
    if not hasattr(fun, '_plankton_fields_'):
      fun._plankton_fields_ = []
    fun._plankton_fields_.append(name)
    return fun
  return callback


# Function that always returns none. Easy.
def always_none(value):
  return None


# Configuration object that sets up how to perform plankton encoding.
class Encoder(object):

  def __init__(self):
    self.resolver = always_none
    self.descriptors = _DESCRIPTORS

  # Encodes the given object into a byte array.
  def encode(self, obj):
    stream = DataOutputStream(self.resolver)
    stream.write_object(obj)
    return stream.bytes

  # Encodes the given object into a base64 string.
  def base64encode(self, obj):
    return base64.b64encode(str(self.encode(obj)))

  # Sets the environment resolver to use when encoding. If the given value is
  # None the resolver is left unchanged.
  def set_resolver(self, value):
    if not value is None:
      self.resolver = value
    return self


# Function that always throws its argument.
def always_throw(key):
  raise Exception(key)


class Decoder(object):

  def __init__(self, descriptors=_DESCRIPTORS):
    self.descriptors = descriptors
    self.access = str

  # Decodes a byte array into a plankton object.
  def decode(self, data):
    stream = DataInputStream(data, self.descriptors, self.access)
    return stream.read_object()

  def disassemble(self, data):
    stream = DataInputStream(data, self.descriptors, self.access)
    return stream.disassemble_object("")

  # Decodes a base64 encoded string into a plankton object.
  def base64decode(self, data):
    return self.decode(bytearray(base64.b64decode(data)))

  def base64disassemble(self, data):
    return self.disassemble(bytearray(base64.b64decode(data)))

  # Sets the function to use to access environment values by key.
  def set_access(self, value):
    if not value is None:
      self.access = value
    return self


# Holds state used when stringifying a value.
class Stringifier(object):

  def __init__(self):
    pass

  def s(self, value):
    return visit_data(value, self)

  def visit_object(self, value):
    return '#<%s: %s>' % (self.s(value.header), self.s(value.payload))

  def visit_array(self, value):
    return "[%s]" % ", ".join([self.s(e) for e in value ])


  def visit_map(self, value):
    return "{%s}" % ", ".join([
      "%s: %s" % (self.s(k), self.s(v))
        for (k, v)
        in value.iteritems()
    ])

  def visit_string(self, value):
    return '"%s"' % value

  def visit_int(self, value):
    return str(value)

  def visit_null(self, value):
    return "null"


# Returns a human-readable string representation of the given data. The result
# is _not_ intended to be read back in so if you're ever tempted to write a
# parser, don't.
def stringify(data):
  return visit_data(data, Stringifier())
