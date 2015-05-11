# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import multiprocessing
import time

from telemetry.core import exceptions
from telemetry.core.platform import profiler
from telemetry.core.platform.profiler import monsoon
from telemetry.core.backends import adb_commands

TARGET_TEMPERATURE = 45
MAX_DIFF = 1

def _GetTemperature(adb):
  result = adb.RunShellCommand(
          'cat /sys/devices/virtual/thermal/thermal_zone0/temp')
  return int(result[0].strip())


def _GetCPUStates(adb):
  cpu_count = int(adb.RunShellCommand(
      'cat /sys/devices/system/cpu/kernel_max')[0])+1
  cpu_governor = adb.RunShellCommand(
          'cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor')[0]
  cpu_online_status = []
  for i in range(1, cpu_count):
    cpu_online_status.append((i,
        adb.RunShellCommand('cat /sys/devices/system/cpu/cpu%d/online' % i)[0]))
  return (cpu_governor, cpu_online_status, cpu_count)


def _LogCPUStates(adb, message):
  cpu_governor, cpu_online_status, _ = _GetCPUStates(adb)
  print message
  print "CPU0 governor:", cpu_governor
  print "CPU states:", cpu_online_status


def _HeatDevice(adb, temperature_diff):
  print "Heating device ..."
  # Calculate approximate repeat count to heat device
  repeats = str(temperature_diff**2 * 6000)
  adb.RunShellCommand("""
    start_time=$(date +%s)
    while [ $(date +%s) -lt $(( $start_time + 1 )) ]; do
      i=0
      while [ $i -lt """ + repeats + """ ]; do
        i=$(( $i + 1 ))
      done
    done""")


def _CoolDevice(adb, temperature_diff):
  print "Turning CPU's offline..."
  cpu_governor, cpu_online_status, cpu_count = _GetCPUStates(adb)
  cpu_governor_path = '/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor'
  adb.RunShellCommand('echo "powersave" > ' + cpu_governor_path)
  for i in range(cpu_count):
    adb.RunShellCommand('echo "0" > /sys/devices/system/cpu/cpu%d/online' % i)

  # Use approximate wait to cool down the device
  time.sleep(min(max(1, temperature_diff**2/2), 15))

  print "Restoring settings..."
  adb.RunShellCommand('echo "%s" > "%s"' % (cpu_governor, cpu_governor_path))
  for i, state in cpu_online_status:
    adb.RunShellCommand(
            'echo %s > /sys/devices/system/cpu/cpu%d/online' % (state, i))

  print "... Turned CPU's online and restored scaling governor!"


def _NormalizeTemperature():
  adb = adb_commands.AdbCommands(None)
  temp = _GetTemperature(adb)
  print "Device temperature is:", temp

  _LogCPUStates(adb, "CPU states before temperature normalization:")

  while abs(temp - TARGET_TEMPERATURE) > MAX_DIFF:
    temp_diff = abs(temp - TARGET_TEMPERATURE)
    if temp < TARGET_TEMPERATURE - MAX_DIFF:
      _HeatDevice(adb, temp_diff)
    elif temp > TARGET_TEMPERATURE + MAX_DIFF:
      _CoolDevice(adb, temp_diff)
    temp = _GetTemperature(adb)
    print "Device temperature is:", temp

  _LogCPUStates(adb, "CPU states after temperature normalization:")

def _CollectData(output_path, is_collecting, result_list):
  mon = monsoon.Monsoon(wait=True)
  # Note: Telemetry requires the device to be connected by USB, but that
  # puts it in charging mode. This increases the power consumption.
  mon.SetUsbPassthrough(0)
  # Nominal Li-ion voltage is 3.7V, but it puts out 4.2V at max capacity. Use
  # 4.0V to simulate a "~80%" charged battery. Google "li-ion voltage curve".
  # This is true only for a single cell. (Most smartphones, some tablets.)
  mon.SetVoltage(4.0)

  samples = []
  try:
    _NormalizeTemperature()
    mon.StartDataCollection()
    # Do one CollectData() to make the Monsoon set up, which takes about
    # 0.3 seconds, and only signal that we've started after that.
    mon.CollectData()
    is_collecting.set()
    while is_collecting.is_set():
      samples += mon.CollectData()
  finally:
    mon.StopDataCollection()

  # Add x-axis labels.
  plot_data = [(i / 5000., current, voltage)
          for i, (current, voltage) in enumerate(samples)]
  result_list.extend(plot_data)

  # Print data in csv.
  with open(output_path, 'w') as output_file:
    output_writer = csv.writer(output_file)
    output_writer.writerows(plot_data)
    output_file.flush()

  print 'To view the Monsoon profile, run:'
  print ('  echo "set datafile separator \',\'; plot \'%s\' with lines" | '
      'gnuplot --persist' % output_path)


class MonsoonProfiler(profiler.Profiler):
  """Profiler that tracks current using Monsoon Power Monitor.

  http://www.msoon.com/LabEquipment/PowerMonitor/
  The Monsoon device measures current in amps at 5000 samples/second.
  """
  def __init__(self, browser_backend, platform_backend, output_path, state):
    super(MonsoonProfiler, self).__init__(
        browser_backend, platform_backend, output_path, state)
    # We collect the data in a separate process, so we can continuously
    # read the samples from the USB port while running the test.
    self._multiprocessing_manager = multiprocessing.Manager()
    self._result_list = self._multiprocessing_manager.list()
    self._is_collecting = multiprocessing.Event()
    self._collector = multiprocessing.Process(
        target=_CollectData,
        args=(output_path, self._is_collecting, self._result_list))
    self._collector.start()
    if not self._is_collecting.wait(timeout=90.0):
      self._collector.terminate()
      raise exceptions.ProfilingException('Failed to start data collection.')

  @classmethod
  def name(cls):
    return 'monsoon'

  @classmethod
  def is_supported(cls, browser_type):
    return True

  @property
  def results(self):
    """Get results after CollectProfile is called."""
    assert(not self._is_collecting.is_set())
    return self._result_list

  def CollectProfile(self):
    self._is_collecting.clear()
    self._collector.join()
    return [self._output_path]
