# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from memory_inspector.core import stacktrace


class NativeHeap(object):
  """A snapshot of outstanding (i.e. not freed) native allocations.

  This is typically obtained by calling |backends.Process|.DumpNativeHeap()
  """

  def __init__(self):
    self.allocations = []

  def Add(self, allocation):
    assert(isinstance(allocation, Allocation))
    self.allocations += [allocation]


class Allocation(object):
  """A Native allocation, modeled in a size*count fashion.

  |count| is the number of identical stack_traces which performed the allocation
  of |size| bytes.
  """

  def __init__(self, size, count, stack_trace):
    assert(isinstance(stack_trace, stacktrace.Stacktrace))
    self.size = size  # in bytes.
    self.count = count
    self.stack_trace = stack_trace

  @property
  def total_size(self):
    return self.size * self.count

  def __str__(self):
    return '%d x %d : %s' % (self.count, self.size, self.stack_trace)
