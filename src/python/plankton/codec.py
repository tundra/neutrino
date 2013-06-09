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


# Encapsulates state relevant to writing plankton data.
class DataOutputStream(object):

  def __init__(self):
    self.bytes = bytearray()
    self.object_index = {}
    self.object_offset = 0

  # Writes a value to the stream.
  def write_object(self, obj):
    t = type(obj)
    if t == int:
      self._add_byte(_INT32_TAG)
      self._encode_int32(obj)
    elif t == str:
      self._add_byte(_STRING_TAG)
      self._encode_string(obj)
    elif t == list:
      self._add_byte(_ARRAY_TAG)
      self._encode_array(obj)
    elif t == dict:
      self._add_byte(_MAP_TAG)
      self._encode_map(obj)
    elif obj is None:
      self._add_byte(_NULL_TAG)
    elif obj is True:
      self._add_byte(_TRUE_TAG)
    elif obj is False:
      self._add_byte(_FALSE_TAG)
    else:
      self._write_object(obj)

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

  # Writes a naked string.
  def _encode_string(self, value):
    self._encode_uint32(len(value))
    self.bytes.extend(value)

  # Writes a naked array.
  def _encode_array(self, value):
    self._encode_uint32(len(value))
    for elm in value:
      self.write_object(elm)

  # Writes a naked map.
  def _encode_map(self, value):
    self._encode_uint32(len(value))
    for (k, v) in value.items():
      self.write_object(k)
      self.write_object(v)

  def _write_object(self, obj):
    if obj in self.object_index:
      self._add_byte(_REFERENCE_TAG)
      offset = self.object_offset - self.object_index[obj] - 1
      self._encode_uint32(offset)
    else:
      name = obj.__class__.__name__
      desc = _DESCRIPTORS.get(name, None)
      if desc is None:
        raise Exception(obj)
      index = self.object_offset
      self.object_offset += 1
      self._add_byte(_OBJECT_TAG)
      self.object_index[obj] = -1
      self.write_object(desc.get_header())
      self.object_index[obj] = index
      self.write_object(desc.get_payload(obj))

  # Adds a single byte to the stream.
  def _add_byte(self, byte):
    self.bytes.append(byte)


# Encapsulates state relevant to reading plankton data.
class DataInputStream(object):

  def __init__(self, bytes):
    self.bytes = bytes
    self.cursor = 0
    self.object_index = {}
    self.object_offset = 0

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
    else:
      raise Exception(tag)

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

  # Reads a naked map from the stream.
  def _decode_map(self):
    length = self._decode_uint32()
    result = {}
    for i in xrange(0, length):
      k = self.read_object()
      v = self.read_object()
      result[k] = v
    return result

  # Reads a naked object from the stream.
  def _decode_object(self):
    index = self.object_offset
    self.object_offset += 1
    self.object_index[index] = None
    header = self.read_object()
    desc = _DESCRIPTORS.get(header, None)
    if desc is None:
      raise Exception(header)
    instance = desc.new_instance()
    self.object_index[index] = instance
    payload = self.read_object()
    desc.apply_payload(instance, payload)
    return instance

  # Reads a raw object reference from the stream.
  def _decode_reference(self):
    offset = self._decode_uint32()
    index = self.object_offset - offset - 1
    return self.object_index[index]

  # Reads a single byte from the stream.
  def _get_byte(self):
    result = self.bytes[self.cursor]
    self.cursor += 1
    return result


_DESCRIPTORS = {}


# A description of how to serialize and deserialize values of a particular
# type.
class ObjectDescriptor(object):

  def __init__(self, klass):
    self.klass = klass
    self.fields = None

  # Returns the header to use when encoding the given instance
  def get_header(self):
    return self.klass.__name__

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
  def new_instance(self):
    return (self.klass)()

  # Sets the data in the given instance based on the given payload.
  def apply_payload(self, instance, payload):
    for (name, value) in payload.iteritems():
      setattr(instance, name, value)

# Marks the given class as serializable.
def serializable(klass):
  name = klass.__name__
  _DESCRIPTORS[name] = ObjectDescriptor(klass)
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


# Encodes an object as plankton, returning a byte array.
def serialize(obj):
  stream = DataOutputStream()
  stream.write_object(obj)
  return stream.bytes


# Encodes an object as plankton, returning a base64 encoded string.
def base64encode(obj):
  return base64.b64encode(str(serialize(obj)))


# Decodes a plankton byte array as an object.
def deserialize(data):
  stream = DataInputStream(data)
  return stream.read_object()


# Decodes a base64 encoded string into a plankton object.
def base64decode(data):
  return deserialize(bytearray(base64.b64decode(data)))
