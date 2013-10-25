# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).


from framework import *


class Tests(TestAssembler):

  def gen_zero(self):
    self.set_input(0)
    self.get_assembly().tag(tINT32).int32(0)

  def gen_one(self):
    self.set_input(1)
    self.get_assembly().tag(tINT32).int32(1)

  def gen_minus_one(self):
    self.set_input(-1)
    self.get_assembly().tag(tINT32).int32(-1)

  def gen_1027(self):
    self.set_input(1027)
    self.get_assembly().tag(tINT32).int32(1027)

  def gen_1027_square(self):
    self.set_input(1027 ** 2)
    self.get_assembly().tag(tINT32).int32(1027 ** 2)

  def gen_1027_cube(self):
    self.set_input(1027 ** 3)
    self.get_assembly().tag(tINT32).int32(1027 ** 3)

  def gen_minus_1027(self):
    self.set_input(-1027)
    self.get_assembly().tag(tINT32).int32(-1027)

  def gen_minus_1027_square(self):
    self.set_input(-(1027 ** 2))
    self.get_assembly().tag(tINT32).int32(-(1027 ** 2))

  def gen_minus_1027_cube(self):
    self.set_input(-(1027 ** 3))
    self.get_assembly().tag(tINT32).int32(-(1027 ** 3))

  def gen_shorthand_1027(self):
    self.set_input(1027)
    self.get_assembly().int(1027)

  def gen_shorthand_minus_1027(self):
    self.set_input(-1027)
    self.get_assembly().int(-1027)

  def gen_empty_string(self):
    self.set_input("")
    self.get_assembly().tag(tSTRING).uint32(0).blob(bytearray())

  def gen_foo_string(self):
    self.set_input("foo!")
    self.get_assembly().tag(tSTRING).uint32(4).blob(bytearray('foo!'))

  def gen_long_string(self):
    self.set_input("12-3401283094812309417249p583274o51893212")
    self.get_assembly().tag(tSTRING).uint32(41).blob(bytearray("12-3401283094812309417249p583274o51893212"))

  def gen_shorthand_empty_string(self):
    self.set_input("")
    self.get_assembly().string('')

  def gen_shorthand_foo_string(self):
    self.set_input("foo!")
    self.get_assembly().string('foo!')

  def gen_shorthand_long_string(self):
    self.set_input("12-3401283094812309417249p583274o51893212")
    self.get_assembly().string("12-3401283094812309417249p583274o51893212")

  def gen_null(self):
    self.set_input(None)
    self.get_assembly().tag(tNULL)

  def gen_true(self):
    self.set_input(True)
    self.get_assembly().tag(tTRUE)

  def gen_false(self):
    self.set_input(False)
    self.get_assembly().tag(tFALSE)

  def gen_shorthand_null(self):
    self.set_input(None)
    self.get_assembly().null()

  def gen_shorthand_true(self):
    self.set_input(True)
    self.get_assembly().true()

  def gen_shorthand_false(self):
    self.set_input(False)
    self.get_assembly().false()

  def gen_empty_array(self):
    self.set_input([])
    self.get_assembly().tag(tARRAY).uint32(0)

  def gen_one_two_three_array(self):
    self.set_input([1, 2, 3])
    (self.get_assembly()
      .tag(tARRAY).uint32(3)
        .int(1)
        .int(2)
        .int(3))

  def gen_shorthand_one_two_three_array(self):
    self.set_input([1, 2, 3])
    self.get_assembly().array(3).int(1).int(2).int(3)

  def gen_nested_array(self):
    self.set_input([1, [2, [3, [4, [5, [6]]]]]])
    (self.get_assembly()
      .array(2)
        .int(1)
        .array(2)
          .int(2)
          .array(2)
            .int(3)
            .array(2)
              .int(4)
              .array(2)
                .int(5)
                .array(1)
                  .int(6))

  def gen_empty_map(self):
    self.set_input({})
    self.get_assembly().tag(tMAP).uint32(0)

  def gen_small_map(self):
    self.set_input({'a': 1, 'b': 2})
    (self.get_assembly()
      .tag(tMAP).uint32(2)
        .string('a').int(1)
        .string('b').int(2))

  def gen_shorthand_small_map(self):
    self.set_input({'a': 1, 'b': 2})
    (self.get_assembly()
      .map(2)
        .string('a').int(1)
        .string('b').int(2))

  def gen_reverse_small_map(self):
    self.set_input({'b': 1, 'a': 2})
    (self.get_assembly()
      .map(2)
        .string('a').int(2)
        .string('b').int(1))

  def gen_simple_object(self):
    self.set_input(new_object().set_header('a').set_payload('b'))
    (self.get_assembly().tag(tOBJECT).string('a').string('b'))

  def gen_slightly_less_simple_object(self):
    self.set_input(new_object().set_header('a').set_payload({
      'x': None,
      'y': False
    }))
    (self.get_assembly()
      .tag(tOBJECT)
        .string('a')
        .map(2)
          .string('x').null()
          .string('y').false())

  def gen_pair_one(self):
    self.set_input(new_object().set_payload({
      'x': 1,
      'y': 2
    }))
    (self.get_assembly()
      .tag(tOBJECT)
        .null()
        .map(2)
          .string('x').int(1)
          .string('y').int(2))

  def gen_shorthand_pair_one(self):
    self.set_input(new_object().set_payload({
      'x': 1,
      'y': 2
    }))
    (self.get_assembly()
      .object()
        .null()
        .map(2)
          .string('x').int(1)
          .string('y').int(2))

  def gen_pair_nested(self):
    self.set_input(new_object().set_payload({
      'x': new_object().set_payload({'x': 1, 'y': 2}),
      'y': new_object().set_payload({'x': 3, 'y': 4})
    }))
    (self.get_assembly()
      .object()
        .null()
        .map(2)
          .string('x')
            .object()
              .null()
              .map(2).string('x').int(1).string('y').int(2)
          .string('y')
            .object()
              .null()
              .map(2).string('x').int(3).string('y').int(4))

  def gen_pair_close_cycle(self):
    self.set_input(new_object(id="x").set_payload(ref("x")))
    self.get_assembly().object().null().tag(tREF).uint32(0)

  def gen_shorthand_pair_close_cycle(self):
    self.set_input(new_object(id="x").set_payload(ref("x")))
    self.get_assembly().object().null().ref(0)

  def gen_array_repetition(self):
    self.set_input([new_object(id="x"), ref("x"), ref("x")])
    (self.get_assembly()
      .array(3)
        .object()
          .null()
          .null()
        .ref(0)
        .ref(0))

  def gen_repeated_array_repetition(self):
    self.set_input([
      new_object(id=0), ref(0),
      new_object(id=1), ref(0), ref(1),
      new_object(id=2), ref(0), ref(1), ref(2)])
    (self.get_assembly()
      .array(9)
        .object().null().null().ref(0)
        .object().null().null().ref(1).ref(0)
        .object().null().null().ref(2).ref(1).ref(0))

  def gen_header_cycle(self):
    self.set_input(new_object(id="x")
      .set_header(new_object(id="y"))
      .set_payload([ref("x"), ref("y")]))
    (self.get_assembly()
      .object()
        .object().null().null()
        .array(2)
          .ref(1)
          .ref(0))


if __name__ == '__main__':
  main()
