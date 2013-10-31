# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Module load order.

# A single abstract task that may depend on other tasks.
class Task(object):

  def __init__(self, key):
    self.key = key
    self.has_run = False

  # Returns the set of prerequisites which must be run before this task.
  def get_prerequisites(self):
    return []

  def run_task(self):
    self.has_run = True


# Exception raised if there is a "deadlock" among the tasks.
class CyclicalSchedule(Exception):

  def __init__(self, key):
    super(CyclicalSchedule, self).__init__(str(key))


# A scheduler that runs tasks in their specified order.
class TaskScheduler(object):

  def __init__(self):
    self.tasks = []
    self.task_map = {}

  # Add a task object to be scheduled by this scheduler.
  def add_task(self, task):
    assert not task.key in self.task_map
    self.tasks.append(task)
    self.task_map[task.key] = task

  # Run all the tasks in a correct order, returning the list of the keys of
  # the tasks in the order they were run. This runs in quadratic time but for
  # what we'll be using it for here that's okay.
  def run(self):
    result = []
    while True:
      task = self.get_next_task()
      if task is None:
        return self.check_done(result)
      result.append(task.key)
      task.run_task()

  # Check that we are indeed done, raising an exception if not.
  def check_done(self, result):
    for task in self.tasks:
      if not task.has_run:
        raise CyclicalSchedule(task.key)
    return result

  # Returns the next task that is ready to run.
  def get_next_task(self):
    for task in self.tasks:
      if task.has_run:
        continue
      is_ready = True
      for preq in task.get_prerequisites():
        preq_task = self.task_map[preq]
        if not preq_task.has_run:
          is_ready = False
          break
      if is_ready:
        return task
