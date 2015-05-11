#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import optparse
import os
import re
import threading
import time
import sys

from sets import Set
from string import Template

sys.path.append(os.path.join(sys.path[0], os.pardir, os.pardir, os.pardir,
                             'build','android'))
from pylib import android_commands
from pylib import constants

def _RunCollectMemConsumption(package_name, interval):
  should_quit = threading.Event()
  memory_stats = dict()
  adb = android_commands.AndroidCommands()

  def _CollectStats(pid):
      memdump = adb.RunShellCommand('dumpsys meminfo ' + pid)

      result = dict()

      for line in memdump:
        items = re.split("  +", line.lstrip())
        if items[0] == "Dalvik":
          result["dalvik_pss"] = items[1]
          result["dalvik_heap"] = items[4]
          result["dalvik_alloc"] = items[5]

        if items[0] == "Dalvik Heap":
          result["dalvik_pss"] = items[1]
          result["dalvik_heap"] = items[5]
          result["dalvik_alloc"] = items[6]

        if items[0] == "Dalvik Other":
          result["dalvik_pss"] += items[1]

        if items[0] == "Other dev":
          result["other_dev"] = items[1]

        if items[0] == "Unknown":
          result["unknown"] = items[1]

        if items[0] == ".so mmap":
          result["so_mmap"] = items[1]

        if items[0] == "TOTAL":
          result["cpu_total"] = items[1]

        if items[0] == "Graphics":
          result["graphics_in_meminfo"] = items[1]

        if items[0] == "GL":
          result["gl_in_meminfo" ] = items[1]

      if not result.has_key('cpu_total') or not result.has_key('dalvik_pss'):
        return None

      adb.RunShellCommandWithSU('setenforce 0')
      graphics_dump = adb.RunShellCommand('showmap ' + pid)

      for line in graphics_dump:
        items = line.split()
        if "dma" in line:
          result["dma"] = items[0]

        if "kgsl" in line:
          # VM Size - PSS. What is this number supposed to tell us?
          result["kgsl"] = str(int(items[0]) - int(items[2]))

      graphics_dump = adb.RunShellCommand(
              'cat /sys/kernel/debug/ion/ion-system-extra')

      for line in graphics_dump:
        if "No such file or directory" in line:
          break

        items = re.split(" +", line.lstrip())
        if len(items) > 1 and items[1] == pid:
          result["ion_system"] = items[2]
          break

      return result


  def ExtractProcessDescs(process_name):
    descs = []
    for line in adb.RunShellCommand('ps', log_result=False):
      data = line.split()
      try:
        if process_name in data[-1]:  # name is in the last column
          descs.append({'pid': data[1], 'name': data[-1]})
      except IndexError:
        pass
    return descs

  def _Loop():
    pdescs = ExtractProcessDescs(package_name)

    for pdesc in pdescs:
      memory_stats[pdesc['pid']] = {'name': pdesc['name'], 'results': []}

    count = 0
    while not should_quit.is_set():
      print >> sys.stderr, 'Collecting ', count
      for key,value in memory_stats.items():
        result = _CollectStats(key)
        if result:
          value['results'].append(result)
      count += 1
      should_quit.wait(interval)

  t = threading.Thread(target=_Loop)

  print >>sys.stderr, 'Press enter or CTRL+C to stop'
  t.start()
  try:
    _ = raw_input()
  except KeyboardInterrupt:
    pass
  finally:
    should_quit.set()

  t.join()

  print 'data=', memory_stats


def main(argv):
  parser = optparse.OptionParser(usage='Usage: %prog [options]',
                                 description=__doc__)
  parser.add_option('-p',
                    '--package',
                    default=constants.PACKAGE_INFO['chrome'].package,
                    help='Package name to collect.')
  parser.add_option('-i',
                    '--interval',
                    default=1,
                    type='int',
                    help='Interval in seconds for manual collections.')
  options, args = parser.parse_args(argv)
  return _RunCollectMemConsumption(options.package, options.interval)


if __name__ == '__main__':
  main(sys.argv)
