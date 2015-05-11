# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from memory_inspector.core import symbol


class Stacktrace(object):
  """Models a stack-trace, which is a sequence of stack |Frame|s."""

  def __init__(self):
    self._frames = []

  def Add(self, frame):
    assert(isinstance(frame, Frame))
    self._frames += [frame]

  @property
  def depth(self):
    return len(self._frames)

  def __getitem__(self, index):
    return self._frames[index]

  def __str__(self):
    return ', '.join([str(x) for x in self._frames])


class Frame(object):
  """Models a stack frame in a |Stacktrace|. It might be symbolized or not."""

  def __init__(self, address):
    """
    Args:
        address: the absolute (virtual) address of the stack frame in the
                 original process virtual address space.
    """
    assert(isinstance(address, int))
    self.address = address
    self.symbol = None
    self.exec_file_rel_path = None

    # Offset is the displacement inside the executable file, calculated as:
    # self.address - mapping_address_of_the_so + mapping_offset_of_the_exec.
    self.offset = None

  def SetExecFileInfo(self, exec_file_rel_path, offset):
    """Sets the base file + offset information required for symbolization.

    Args:
        exec_file_rel_path: the path of the mapped executable (binary or lib)
            relative to the target device (e.g., /system/lib/libc.so).
        offset: the offset in the executable.
    """
    assert(isinstance(offset, int))
    self.exec_file_rel_path = exec_file_rel_path
    self.offset = offset

  def SetSymbolInfo(self, sym):
    """Sets the symbolization information."""
    assert(isinstance(sym, symbol.Symbol))
    assert(not self.symbol)
    self.symbol = sym

  @property
  def exec_file_name(self):
    """Returns the file name (stripped of the path) of the executable."""
    return os.path.basename(self.exec_file_rel_path)

  def __str__(self):
    if self.symbol:
      return str(self.symbol)
    elif self.exec_file_rel_path:
      return '%s +0x%x' % (self.exec_file_name, self.offset)
    else:
      return '0x%x' % self.address
