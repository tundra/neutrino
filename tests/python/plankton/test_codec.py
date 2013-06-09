#!/usr/bin/python


import plankton
import unittest


@plankton.serializable
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


class PlanktonTest(unittest.TestCase):

  def check_transcoding(self, value):
    raw_encoded = plankton.serialize(value)
    raw_decoded = plankton.deserialize(raw_encoded)
    self.assertEquals(value, raw_decoded)
    base64_encoded = plankton.base64encode(value)
    base64_decoded = plankton.base64decode(base64_encoded)
    self.assertEquals(value, base64_decoded)

  def transcode(self, value):
    raw_encoded = plankton.serialize(value)
    return plankton.deserialize(raw_encoded)

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

if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
