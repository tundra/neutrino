#!/usr/bin/python
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

from neutrino import schedule
import random
import unittest


# A dummy task used for testing.
class TestTask(schedule.Task):

  def __init__(self, value, preqs):
    super(TestTask, self).__init__(value)
    self.value = value
    self.preqs = preqs

  def get_prerequisites(self):
    return self.preqs


class ScheduleTest(unittest.TestCase):

  def test_ordering(self):
    ints = list(range(0, 100))
    random.shuffle(ints)
    scheduler = schedule.TaskScheduler()
    for entry in ints:
      if entry == 0:
        preqs = []
      else:
        preqs = [entry - 1]
      scheduler.add_task(TestTask(entry, preqs))
    order = scheduler.run()
    self.assertEquals(range(0, 100), order)

  def test_cycle(self):
    scheduler = schedule.TaskScheduler()
    scheduler.add_task(TestTask(0, [1]))
    scheduler.add_task(TestTask(1, [0]))
    self.assertRaises(schedule.CyclicalScheduleError, lambda: scheduler.run())

  def test_tiebreaker(self):
    scheduler = schedule.TaskScheduler()
    scheduler.add_task(TestTask(0, [3]))
    scheduler.add_task(TestTask(2, [3]))
    scheduler.add_task(TestTask(1, [3]))
    scheduler.add_task(TestTask(3, []))
    self.assertEquals([3, 0, 2, 1], scheduler.run())


if __name__ == '__main__':
  runner = unittest.TextTestRunner(verbosity=0)
  unittest.main(testRunner=runner)
