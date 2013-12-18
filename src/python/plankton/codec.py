# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Utilities for encoding and decoding of plankton data.


import base64
import collections


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
  elif (t == dict) or (t == collections.OrderedDict):
    return visitor.visit_map(data)
  elif data is None:
    return visitor.visit_null(data)
  elif (data is True) or (data is False):
    return visitor.visit_bool(data)
  else:
    return visitor.visit_object(data)


class EncodingAssembler(object):

  def __init__(self):
    self.bytes = bytearray()

  # Writes a value tag.
  def tag(self, value):
    self._add_byte(value)
    return self

  # Writes a naked unsigned int32.
  def uint32(self, value):
    self._encode_uint32(value)
    return self

  # Writes a naked signed int32.
  def int32(self, value):
    self._encode_uint32((value << 1) ^ (value >> 31))
    return self

  # Writes a raw blob of data.
  def blob(self, value):
    self.bytes.extend(value)
    return self

  # Adds a single byte to the stream.
  def _add_byte(self, byte):
    self.bytes.append(byte)

    # Writes a naked unsigned int32.
  def _encode_uint32(self, value):
    assert value >= 0
    while value > 0x7F:
      part = value & 0x7F
      self._add_byte(part | 0x80)
      value = value >> 7
    self._add_byte(value)


# Encapsulates state relevant to writing plankton data.
class DataOutputStream(object):

  def __init__(self, assembler, resolver):
    self.assm = assembler
    self.object_index = {}
    self.object_offset = 0
    self.resolver = resolver

  # Writes a value to the stream.
  def write_object(self, obj):
    visit_data(obj, self)

  # Emit a tagged integer.
  def visit_int(self, obj):
    self.assm.tag(_INT32_TAG)
    self.assm.int32(obj)

  # Emit a tagged string.
  def visit_string(self, value):
    self.assm.tag(_STRING_TAG)
    bytes = bytearray(value, "utf-8")
    self.assm.uint32(len(bytes))
    self.assm.blob(bytes)

  # Emit a tagged array.
  def visit_array(self, value):
    self.assm.tag(_ARRAY_TAG)
    self.assm.uint32(len(value))
    for elm in value:
      self.write_object(elm)

  # Emit a tagged map.
  def visit_map(self, value):
    self.assm.tag(_MAP_TAG)
    self.assm.uint32(len(value))
    for k in sorted(value.keys()):
      self.write_object(k)
      self.write_object(value[k])

  # Emit a tagged null.
  def visit_null(self, obj):
    self.assm.tag(_NULL_TAG)

  # Emit a tagged boolean.
  def visit_bool(self, obj):
    if obj:
      self.assm.tag(_TRUE_TAG)
    else:
      self.assm.tag(_FALSE_TAG)

  def acquire_offset(self):
    index = self.object_offset
    self.object_offset += 1
    return index

  def resolve_object(self, obj):
    if isinstance(obj, EnvironmentReference):
      return obj.key
    else:
      return (self.resolver)(obj)

  # Emit a tagged object or reference.
  def visit_object(self, obj):
    if obj in self.object_index:
      # We've already emitted this object so just reference the previous
      # instance.
      self.assm.tag(_REFERENCE_TAG)
      offset = self.object_offset - self.object_index[obj] - 1
      self.assm.uint32(offset)
    else:
      resolved = self.resolve_object(obj)
      if resolved is None:
        # Find the appropriate meta info and, if necessary, replacement
        # object.
        meta_info = None
        last_obj = obj
        while meta_info is None:
          meta_info = _CLASS_REGISTRY.get_nearest_meta_info(last_obj.__class__)
          if meta_info is None:
            raise Exception(last_obj.__class__)
          replacement = meta_info.get_replacement(last_obj)
          if not replacement is last_obj:
            meta_info = None
            last_obj = replacement
        index = self.acquire_offset()
        self.assm.tag(_OBJECT_TAG)
        self.object_index[obj] = -1
        self.write_object(meta_info.get_header(obj))
        self.object_index[obj] = index
        self.write_object(meta_info.get_payload(obj))
      else:
        index = self.acquire_offset()
        self.assm.tag(_ENVIRONMENT_TAG)
        self.object_index[obj] = -1
        self.write_object(resolved)
        self.object_index[obj] = index


# Encapsulates state relevant to reading plankton data.
class DataInputStream(object):

  def __init__(self, bytes, access, default_object):
    self.bytes = bytes
    self.cursor = 0
    self.object_index = {}
    self.object_offset = 0
    self.access = access
    self.default_object = default_object

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
    meta_info = _CLASS_REGISTRY.get_meta_info_by_header(header)
    if not meta_info is None:
      instance = meta_info.new_empty_instance(header)
    else:
      instance = (self.default_object)(header)
    self.object_index[index] = instance
    payload = self.read_object()
    if not meta_info is None:
      meta_info.set_instance_contents(instance, payload)
    else:
      instance.set_contents(payload)
    return instance

  def _disassemble_object(self, indent):
    index = self.grab_index()
    header = self.disassemble_object(indent + "^ ")
    payload = self.disassemble_object(indent + "  ")
    return "%sobject (@%i)\n%s\n%s" % (indent, index, header, payload)

  def access_environment(self, key):
    if type(key) is list:
      return EnvironmentReference(tuple(key))
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


# Returns the fully qualified class name of a class object.
def class_name(klass):
  return "%s.%s" % (klass.__module__, klass.__name__)


# Function that always returns none. Easy.
def always_none(value):
  return None


# Configuration object that sets up how to perform plankton encoding.
class Encoder(object):

  def __init__(self):
    self.resolver = always_none

  # Encodes the given object into a byte array.
  def encode(self, obj):
    assembler = EncodingAssembler()
    self.write(obj, assembler)
    return assembler.bytes

  def write(self, obj, assembler):
    stream = DataOutputStream(assembler, self.resolver)
    stream.write_object(obj)

  # Encodes the given object into a base64 string.
  def base64encode(self, obj):
    return base64.b64encode(self.encode(obj))

  # Sets the environment resolver to use when encoding. If the given value is
  # None the resolver is left unchanged.
  def set_resolver(self, value):
    if not value is None:
      self.resolver = value
    return self


# Function that always throws its argument.
def always_throw(key):
  raise Exception(key)


class DefaultObject(object):

  def __init__(self, header=None):
    self.header = header
    self.payload = None

  def set_header(self, value):
    self.header = value
    return self

  def get_header(self):
    return self.header

  def set_payload(self, value):
    self.payload = value
    return self

  def set_contents(self, value):
    return self.set_payload(value)

  def get_payload(self):
    return self.payload

  def __str__(self):
    return "#<? %s %s>" % (self.header, self.payload)



class Decoder(object):

  def __init__(self, access=str, default_object=DefaultObject):
    self.access = access
    self.default_object = default_object

  # Decodes a byte array into a plankton object.
  def decode(self, data):
    stream = DataInputStream(data, self.access, self.default_object)
    return stream.read_object()

  def disassemble(self, data):
    stream = DataInputStream(data, self.access, None)
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
        in value.items()
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


# An empty object type used to create new class instances without calling init.
# Hacky, granted.
class Empty(object):
  pass


# A collection of information about how to serialize a python object.
class ClassMetaInfo(object):

  def __init__(self, klass):
    self.klass = klass
    self.has_analyzed_class = False
    self.default_header = None
    self.header_getter = None
    self.payload_getter = None
    self.replacer = None
    self.fields = []
    self.instance_factory = None

  # Ensure that we have scanned through the class members and stored the
  # handlers appropriately.
  def ensure_analyzed(self):
    if not self.has_analyzed_class:
      self.has_analyzed_class = True
      self.analyze()

  # Scan through the class members and store the handlers appropriately
  def analyze(self):
    for (name, value) in self.klass.__dict__.items():
      value = peel_method_wrappers(value)
      attribs = MethodAttributes.get(value)
      if attribs.is_header_getter:
        self.header_getter = value
      if attribs.is_payload_getter:
        self.payload_getter = value
      if attribs.is_replacer:
        self.replacer = value
      if attribs.is_instance_factory:
        self.instance_factory = value
      if len(attribs.fields) > 0:
        # The fields have been added in reverse order of how they appear in the
        # source code because the decorators are applied from the inside out.
        self.fields = list(reversed(attribs.fields))

  # Returns the header to use for the given value.
  def get_header(self, value):
    self.ensure_analyzed()
    if not self.header_getter is None:
      return (self.header_getter)(value)
    else:
      return self.default_header

  # Returns the payload to use for the given value.
  def get_payload(self, value):
    self.ensure_analyzed()
    if self.payload_getter is None:
      return self.get_default_payload(value)
    else:
      return (self.payload_getter)(value)

  # Builds and returns the default payload for the given value, based on the
  # field decorations.
  def get_default_payload(self, value):
    self.ensure_analyzed()
    result = {}
    for field in self.fields:
      field_value = getattr(value, field)
      result[field] = field_value
    return result

  # Returns the replacement object to use for the given value, if replacement
  # is used. Otherwise returns the object itself.
  def get_replacement(self, value):
    self.ensure_analyzed()
    if self.replacer is None:
      return value
    else:
      return self.replacer(value)

  # Sets the value to use as the fixed header object.
  def set_default_header(self, value):
    self.default_header = value

  # Returns the default header value.
  def get_default_header(self):
    return self.default_header

  # Creates a new empty instance of the type of class described by this meta
  # info.
  def new_empty_instance(self, header):
    if self.instance_factory is None:
      result = Empty()
      result.__class__ = self.klass
      return result
    else:
      return (self.instance_factory)()

  def set_instance_contents(self, instance, payload):
    for (name, value) in payload.iteritems():
      setattr(instance, name, value)

  def __str__(self):
    return "class meta info %s" % self.klass


# The mapping between python classes and plankton meta-info.
class ClassRegistry(object):

  def __init__(self):
    self.class_to_info = {}
    self.name_to_info = {}
    self.nearest_cache = {}

  def ensure_meta_info(self, klass):
    if not klass in self.class_to_info:
      self.class_to_info[klass] = ClassMetaInfo(klass)
    return self.class_to_info[klass]

  def ensure_name_to_info(self):
    for meta_info in self.class_to_info.values():
      default_header = meta_info.get_default_header()
      if not default_header is None:
        self.name_to_info[default_header] = meta_info

  def get_meta_info_by_header(self, header):
    self.ensure_name_to_info()
    return self.name_to_info.get(header, None)

  def get_nearest_meta_info(self, klass):
    if not klass in self.nearest_cache:
      if klass in self.class_to_info:
        self.nearest_cache[klass] = self.class_to_info[klass]
      else:
        self.nearest_cache[klass] = None
        for base in klass.__bases__:
          nearest = self.get_nearest_meta_info(base)
          if not nearest is None:
            self.nearest_cache[klass] = nearest
            break
    return self.nearest_cache[klass]


# An object that will always be resolved as an environment reference.
class EnvironmentReference(object):

  def __init__(self, key):
    self.key = key

  def __hash__(self):
    return ~hash(self.key)

  def __eq__(self, that):
    if not isinstance(that, EnvironmentReference):
      return False
    else:
      return self.key == that.key

  # Create an environment reference whose key is the array of the given values.
  @staticmethod
  def path(*key):
    return EnvironmentReference(key)


# Peel off any method wrappers (i.e. staticmethod) and return the naked
# underlying function. Safe to call on non-functions, the value is just returned
# directly.
def peel_method_wrappers(fun):
  if isinstance(fun, staticmethod):
    return fun.__func__
  else:
    return fun


# A set of attributes that can be set by plankton on a given method. This is
# later pulled into the class meta info but there's a while between method
# and class definition time where there's no meta info so in the meantime this
# if where it gets stored.
class MethodAttributes(object):

  def __init__(self):
    self.is_header_getter = False
    self.is_payload_getter = False
    self.is_replacer = False
    self.is_instance_factory = False
    self.fields = []

  # Returns the method attributes of the given function (which corresponds to
  # a method). If it doesn't have attributes a set is created, hence the result
  # can safely be modified.
  @staticmethod
  def ensure(fun):
    fun = peel_method_wrappers(fun)
    if not hasattr(fun, '_plankton_method_attributes_'):
      fun._plankton_method_attributes_ = MethodAttributes()
    return fun._plankton_method_attributes_

  # Returns the method attributes of the given function. If it has none the
  # empty attributes will be returned. Hence it's _not_ safe to modify the
  # result.
  @staticmethod
  def get(fun):
    fun = peel_method_wrappers(fun)
    if hasattr(fun, '_plankton_method_attributes_'):
      return fun._plankton_method_attributes_
    else:
      return _EMPTY_METHOD_ATTRIBUTES


_EMPTY_METHOD_ATTRIBUTES = MethodAttributes()


# Marks a type as being serializable
def serializable(header=None):
  if type(header) == type:
    _CLASS_REGISTRY.ensure_meta_info(header)
    return header
  else:
    def add_meta_info(klass):
      resolved_header = header
      if resolved_header is None:
        resolved_header = class_name(klass)
      meta_info = _CLASS_REGISTRY.ensure_meta_info(klass)
      meta_info.set_default_header(resolved_header)
      return klass
    return add_meta_info


# Decorates a method that will be called to produce the plankton header when
# serializing objects it belongs to.
def header(method):
  attribs = MethodAttributes.ensure(method)
  attribs.is_header_getter = True
  return method


# Decorates a method that will be called to produce the plankton payload when
# serializing objects it belongs to.
def payload(method):
  attribs = MethodAttributes.ensure(method)
  attribs.is_payload_getter = True
  return method


# Decorates a method that will be called to produce a replacement object when
# serializing the holding object.
def replacement(method):
  attribs = MethodAttributes.ensure(method)
  attribs.is_replacer = True
  return method


# Decorates a method that will be called to produce new instances of a class
# during deserialization. If none is specified new instances will be created
# by foul hackery.
def factory(method):
  attribs = MethodAttributes.ensure(method)
  attribs.is_instance_factory = True
  return method


# Records a field to serialize by default
def field(name):
  def add_fields(method):
    attribs = MethodAttributes.ensure(method)
    attribs.fields.append(name)
    return method
  return add_fields


# The singleton class registry that holds all the meta info used by plankton.
_CLASS_REGISTRY = ClassRegistry()
