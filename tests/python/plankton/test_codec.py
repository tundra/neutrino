#!/usr/bin/python


from plankton import codec
import unittest


class PlanktonTest(unittest.TestCase):

  def check_transcoding(self, value):
    raw_encoded = codec.serialize(value)
    raw_decoded = codec.deserialize(raw_encoded)
    self.assertEquals(value, raw_decoded)
    base64_encoded = codec.base64encode(value)
    base64_decoded = codec.base64decode(base64_encoded)
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


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
