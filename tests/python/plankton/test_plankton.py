# This test was generated from tests/gen/plankton/testdata.py.
# Don't modify it by hand.


import unittest
import planktontest


class PlanktonTests(planktontest.TestCase):
  
  def test_1027(self):
    ctx = self.new_context()
    input = 1027
    assemble = lambda assm: assm.tag(0).int32(1027)
    self.run_test(input, assemble)
  
  def test_1027_cube(self):
    ctx = self.new_context()
    input = 1083206683
    assemble = lambda assm: assm.tag(0).int32(1083206683)
    self.run_test(input, assemble)
  
  def test_1027_square(self):
    ctx = self.new_context()
    input = 1054729
    assemble = lambda assm: assm.tag(0).int32(1054729)
    self.run_test(input, assemble)
  
  def test_array_repetition(self):
    ctx = self.new_context()
    input = [(ctx.new_object(id=0)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(0), ctx.get_ref(0)]
    assemble = lambda assm: assm.tag(2).uint32(3).tag(7).tag(4).tag(4).tag(8).uint32(0).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_empty_array(self):
    ctx = self.new_context()
    input = []
    assemble = lambda assm: assm.tag(2).uint32(0)
    self.run_test(input, assemble)
  
  def test_empty_map(self):
    ctx = self.new_context()
    input = {
    }
    assemble = lambda assm: assm.tag(3).uint32(0)
    self.run_test(input, assemble)
  
  def test_empty_string(self):
    ctx = self.new_context()
    input = ""
    assemble = lambda assm: assm.tag(1).uint32(0).blob(bytearray([]))
    self.run_test(input, assemble)
  
  def test_false(self):
    ctx = self.new_context()
    input = False
    assemble = lambda assm: assm.tag(6)
    self.run_test(input, assemble)
  
  def test_foo_string(self):
    ctx = self.new_context()
    input = "foo!"
    assemble = lambda assm: assm.tag(1).uint32(4).blob(bytearray([102, 111, 111, 33]))
    self.run_test(input, assemble)
  
  def test_header_cycle(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=1)
      .set_header((ctx.new_object(id=2)
        .set_header(None)
        .set_payload(None)))
      .set_payload([ctx.get_ref(1), ctx.get_ref(2)]))
    assemble = lambda assm: assm.tag(7).tag(7).tag(4).tag(4).tag(2).uint32(2).tag(8).uint32(1).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_long_string(self):
    ctx = self.new_context()
    input = "12-3401283094812309417249p583274o51893212"
    assemble = lambda assm: assm.tag(1).uint32(41).blob(bytearray([49, 50, 45, 51, 52, 48, 49, 50, 56, 51, 48, 57, 52, 56, 49, 50, 51, 48, 57, 52, 49, 55, 50, 52, 57, 112, 53, 56, 51, 50, 55, 52, 111, 53, 49, 56, 57, 51, 50, 49, 50]))
    self.run_test(input, assemble)
  
  def test_minus_1027(self):
    ctx = self.new_context()
    input = -1027
    assemble = lambda assm: assm.tag(0).int32(-1027)
    self.run_test(input, assemble)
  
  def test_minus_1027_cube(self):
    ctx = self.new_context()
    input = -1083206683
    assemble = lambda assm: assm.tag(0).int32(-1083206683)
    self.run_test(input, assemble)
  
  def test_minus_1027_square(self):
    ctx = self.new_context()
    input = -1054729
    assemble = lambda assm: assm.tag(0).int32(-1054729)
    self.run_test(input, assemble)
  
  def test_minus_one(self):
    ctx = self.new_context()
    input = -1
    assemble = lambda assm: assm.tag(0).int32(-1)
    self.run_test(input, assemble)
  
  def test_mixed_repeated_array_repetition(self):
    ctx = self.new_context()
    input = [(ctx.new_object(id=3)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(3), ctx.new_env_ref("x", id=4), ctx.get_ref(3), ctx.get_ref(4), (ctx.new_object(id=5)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(3), ctx.get_ref(4), ctx.get_ref(5)]
    assemble = lambda assm: assm.tag(2).uint32(9).tag(7).tag(4).tag(4).tag(8).uint32(0).tag(9).tag(1).uint32(1).blob(bytearray([120])).tag(8).uint32(1).tag(8).uint32(0).tag(7).tag(4).tag(4).tag(8).uint32(2).tag(8).uint32(1).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_nested_array(self):
    ctx = self.new_context()
    input = [1, [2, [3, [4, [5, [6]]]]]]
    assemble = lambda assm: assm.tag(2).uint32(2).tag(0).int32(1).tag(2).uint32(2).tag(0).int32(2).tag(2).uint32(2).tag(0).int32(3).tag(2).uint32(2).tag(0).int32(4).tag(2).uint32(2).tag(0).int32(5).tag(2).uint32(1).tag(0).int32(6)
    self.run_test(input, assemble)
  
  def test_null(self):
    ctx = self.new_context()
    input = None
    assemble = lambda assm: assm.tag(4)
    self.run_test(input, assemble)
  
  def test_one(self):
    ctx = self.new_context()
    input = 1
    assemble = lambda assm: assm.tag(0).int32(1)
    self.run_test(input, assemble)
  
  def test_one_two_three_array(self):
    ctx = self.new_context()
    input = [1, 2, 3]
    assemble = lambda assm: assm.tag(2).uint32(3).tag(0).int32(1).tag(0).int32(2).tag(0).int32(3)
    self.run_test(input, assemble)
  
  def test_pair_close_cycle(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=6)
      .set_header(None)
      .set_payload(ctx.get_ref(6)))
    assemble = lambda assm: assm.tag(7).tag(4).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_pair_nested(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=7)
      .set_header(None)
      .set_payload({
        "x": (ctx.new_object(id=8)
          .set_header(None)
          .set_payload({
            "x": 1,
            "y": 2})),
        "y": (ctx.new_object(id=9)
          .set_header(None)
          .set_payload({
            "x": 3,
            "y": 4}))}))
    assemble = lambda assm: assm.tag(7).tag(4).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(7).tag(4).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(0).int32(1).tag(1).uint32(1).blob(bytearray([121])).tag(0).int32(2).tag(1).uint32(1).blob(bytearray([121])).tag(7).tag(4).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(0).int32(3).tag(1).uint32(1).blob(bytearray([121])).tag(0).int32(4)
    self.run_test(input, assemble)
  
  def test_pair_one(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=10)
      .set_header(None)
      .set_payload({
        "x": 1,
        "y": 2}))
    assemble = lambda assm: assm.tag(7).tag(4).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(0).int32(1).tag(1).uint32(1).blob(bytearray([121])).tag(0).int32(2)
    self.run_test(input, assemble)
  
  def test_repeated_array_env(self):
    ctx = self.new_context()
    input = [ctx.new_env_ref(0, id=11), ctx.get_ref(11), ctx.get_ref(11)]
    assemble = lambda assm: assm.tag(2).uint32(3).tag(9).tag(0).int32(0).tag(8).uint32(0).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_repeated_array_repetition(self):
    ctx = self.new_context()
    input = [(ctx.new_object(id=12)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(12), (ctx.new_object(id=13)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(12), ctx.get_ref(13), (ctx.new_object(id=14)
      .set_header(None)
      .set_payload(None)), ctx.get_ref(12), ctx.get_ref(13), ctx.get_ref(14)]
    assemble = lambda assm: assm.tag(2).uint32(9).tag(7).tag(4).tag(4).tag(8).uint32(0).tag(7).tag(4).tag(4).tag(8).uint32(1).tag(8).uint32(0).tag(7).tag(4).tag(4).tag(8).uint32(2).tag(8).uint32(1).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_reverse_small_map(self):
    ctx = self.new_context()
    input = {
      "a": 2,
      "b": 1}
    assemble = lambda assm: assm.tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([97])).tag(0).int32(2).tag(1).uint32(1).blob(bytearray([98])).tag(0).int32(1)
    self.run_test(input, assemble)
  
  def test_shorthand_1027(self):
    ctx = self.new_context()
    input = 1027
    assemble = lambda assm: assm.tag(0).int32(1027)
    self.run_test(input, assemble)
  
  def test_shorthand_empty_string(self):
    ctx = self.new_context()
    input = ""
    assemble = lambda assm: assm.tag(1).uint32(0).blob(bytearray([]))
    self.run_test(input, assemble)
  
  def test_shorthand_false(self):
    ctx = self.new_context()
    input = False
    assemble = lambda assm: assm.tag(6)
    self.run_test(input, assemble)
  
  def test_shorthand_foo_string(self):
    ctx = self.new_context()
    input = "foo!"
    assemble = lambda assm: assm.tag(1).uint32(4).blob(bytearray([102, 111, 111, 33]))
    self.run_test(input, assemble)
  
  def test_shorthand_long_string(self):
    ctx = self.new_context()
    input = "12-3401283094812309417249p583274o51893212"
    assemble = lambda assm: assm.tag(1).uint32(41).blob(bytearray([49, 50, 45, 51, 52, 48, 49, 50, 56, 51, 48, 57, 52, 56, 49, 50, 51, 48, 57, 52, 49, 55, 50, 52, 57, 112, 53, 56, 51, 50, 55, 52, 111, 53, 49, 56, 57, 51, 50, 49, 50]))
    self.run_test(input, assemble)
  
  def test_shorthand_minus_1027(self):
    ctx = self.new_context()
    input = -1027
    assemble = lambda assm: assm.tag(0).int32(-1027)
    self.run_test(input, assemble)
  
  def test_shorthand_null(self):
    ctx = self.new_context()
    input = None
    assemble = lambda assm: assm.tag(4)
    self.run_test(input, assemble)
  
  def test_shorthand_one_two_three_array(self):
    ctx = self.new_context()
    input = [1, 2, 3]
    assemble = lambda assm: assm.tag(2).uint32(3).tag(0).int32(1).tag(0).int32(2).tag(0).int32(3)
    self.run_test(input, assemble)
  
  def test_shorthand_pair_close_cycle(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=15)
      .set_header(None)
      .set_payload(ctx.get_ref(15)))
    assemble = lambda assm: assm.tag(7).tag(4).tag(8).uint32(0)
    self.run_test(input, assemble)
  
  def test_shorthand_pair_one(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=16)
      .set_header(None)
      .set_payload({
        "x": 1,
        "y": 2}))
    assemble = lambda assm: assm.tag(7).tag(4).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(0).int32(1).tag(1).uint32(1).blob(bytearray([121])).tag(0).int32(2)
    self.run_test(input, assemble)
  
  def test_shorthand_simple_env(self):
    ctx = self.new_context()
    input = ctx.new_env_ref(0, id=17)
    assemble = lambda assm: assm.tag(9).tag(0).int32(0)
    self.run_test(input, assemble)
  
  def test_shorthand_small_map(self):
    ctx = self.new_context()
    input = {
      "a": 1,
      "b": 2}
    assemble = lambda assm: assm.tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([97])).tag(0).int32(1).tag(1).uint32(1).blob(bytearray([98])).tag(0).int32(2)
    self.run_test(input, assemble)
  
  def test_shorthand_true(self):
    ctx = self.new_context()
    input = True
    assemble = lambda assm: assm.tag(5)
    self.run_test(input, assemble)
  
  def test_simple_env(self):
    ctx = self.new_context()
    input = ctx.new_env_ref(0, id=18)
    assemble = lambda assm: assm.tag(9).tag(0).int32(0)
    self.run_test(input, assemble)
  
  def test_simple_object(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=19)
      .set_header("a")
      .set_payload("b"))
    assemble = lambda assm: assm.tag(7).tag(1).uint32(1).blob(bytearray([97])).tag(1).uint32(1).blob(bytearray([98]))
    self.run_test(input, assemble)
  
  def test_slightly_less_simple_object(self):
    ctx = self.new_context()
    input = (ctx.new_object(id=20)
      .set_header("a")
      .set_payload({
        "x": None,
        "y": False}))
    assemble = lambda assm: assm.tag(7).tag(1).uint32(1).blob(bytearray([97])).tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([120])).tag(4).tag(1).uint32(1).blob(bytearray([121])).tag(6)
    self.run_test(input, assemble)
  
  def test_small_map(self):
    ctx = self.new_context()
    input = {
      "a": 1,
      "b": 2}
    assemble = lambda assm: assm.tag(3).uint32(2).tag(1).uint32(1).blob(bytearray([97])).tag(0).int32(1).tag(1).uint32(1).blob(bytearray([98])).tag(0).int32(2)
    self.run_test(input, assemble)
  
  def test_string_env(self):
    ctx = self.new_context()
    input = ctx.new_env_ref("hey", id=21)
    assemble = lambda assm: assm.tag(9).tag(1).uint32(3).blob(bytearray([104, 101, 121]))
    self.run_test(input, assemble)
  
  def test_true(self):
    ctx = self.new_context()
    input = True
    assemble = lambda assm: assm.tag(5)
    self.run_test(input, assemble)
  
  def test_zero(self):
    ctx = self.new_context()
    input = 0
    assemble = lambda assm: assm.tag(0).int32(0)
    self.run_test(input, assemble)


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)

