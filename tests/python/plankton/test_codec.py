#!/usr/bin/python


import plankton
import unittest


@plankton.serializable()
class Pair(object):

  @plankton.field("first")
  @plankton.field("second")
  def __init__(self, first=None, second=None):
    self.first = first
    self.second = second

  def __eq__(self, that):
    return (self is that) or (self.first == that.first) and (self.second == that.second)

  def __str__(self):
    return "(%s, %s)" % (self.first, self.second)


@plankton.serializable()
class Key(object):

  @plankton.field("id")
  def __init__(self, id=None):
    self.id = id

  def __hash__(self):
    return hash(self.id)


class PlanktonTest(unittest.TestCase):

  def transcode(self, value, resolver=None, access=None):
    encoder = plankton.Encoder().set_resolver(resolver)
    raw_encoded = encoder.encode(value)
    decoder = plankton.Decoder().set_access(access)
    return decoder.decode(raw_encoded)

  def check_transcoding(self, value):
    raw_decoded = self.transcode(value)
    self.assertEquals(value, raw_decoded)
    base64_encoded = plankton.Encoder().base64encode(value)
    base64_decoded = plankton.Decoder().base64decode(base64_encoded)
    self.assertEquals(value, base64_decoded)

  def test_primitive_encoding(self):
    test = self.check_transcoding
    test(1)
    test(-1)
    test(1027)
    test(1027 ** 1)
    test(1027 ** 2)
    test(1027 ** 3)
    test(-1027)
    test(-(1027 ** 1))
    test(-(1027 ** 2))
    test(-(1027 ** 3))
    test("")
    test("foo!")
    test("12-3401283094812309417249p583274o51893212")
    test(None)
    test(True)
    test(False)

  def test_composite_encoding(self):
    test = self.check_transcoding
    test([1, 2, 3])
    test([])
    test(1024 * [4])
    test([1, [2, [3, [4, [5, [6]]]]]])
    test({'a': 1, 'b': 2})
    test({})

  def test_object_encoding(self):
    test = self.check_transcoding
    test(Pair(1, 2))
    test(Pair(Pair(1, 2), Pair(3, 4)))
    pb = Pair("foo")
    pb.second = pb
    pa = self.transcode(pb)
    self.assertEquals("foo", pa.first)
    self.assertTrue(pa is pa.second)
    ab = [pb, pb, pb]
    aa = self.transcode(ab)
    self.assertEquals("foo", aa[0].first)
    self.assertTrue(aa[0] is aa[0].second)
    self.assertTrue(aa[0] is aa[1])
    self.assertTrue(aa[0] is aa[2])

  def test_environment(self):
    val_to_int = {}
    int_to_val = {}
    def add_object(value, index):
      val_to_int[value] = index
      int_to_val[index] = value
    def resolve(obj):
      return val_to_int.get(obj, None)
    def access(key):
      return int_to_val[key]
    obj0 = Key(0)
    dec0 = self.transcode(obj0, resolve, access)
    self.assertFalse(obj0 is dec0)
    add_object(obj0, 0)
    dec0p = self.transcode(obj0, resolve, access)
    self.assertTrue(obj0 is dec0p)

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
