# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from telemetry import decorators
from telemetry.core.platform.profiler import android_prebuilt_profiler_helper
import telemetry.core.platform.power_monitor as power_monitor


SAMPLE_RATE_HZ = 2 # The data is collected from the ds2784 fuel gauge chip
                   # that only updates its data every 3.5s.
FUEL_GAUGE_PATH = '/sys/class/power_supply/ds2784-fuelgauge'
CHARGE_COUNTER = os.path.join(FUEL_GAUGE_PATH, 'charge_counter_ext')
CURRENT = os.path.join(FUEL_GAUGE_PATH, 'current_now')
VOLTAGE = os.path.join(FUEL_GAUGE_PATH, 'voltage_now')


class DS2784PowerMonitor(power_monitor.PowerMonitor):
  def __init__(self, adb):
    super(DS2784PowerMonitor, self).__init__()
    self._adb = adb
    self._powermonitor_process_port = None
    android_prebuilt_profiler_helper.InstallOnDevice(adb, 'file_poller')
    self._file_poller_binary = android_prebuilt_profiler_helper.GetDevicePath(
        'file_poller')


  def _IsDeviceCharging(self):
    for line in self._adb.RunShellCommand('dumpsys battery'):
      if 'powered: ' in line:
        if 'true' == line.split('powered: ')[1]:
          return True
    return False

  @decorators.Cache
  def _HasFuelGauge(self):
    return self._adb.FileExistsOnDevice(CHARGE_COUNTER)

  def CanMonitorPowerAsync(self):
    if not self._HasFuelGauge():
      return False
    if self._IsDeviceCharging():
      logging.warning('Can\'t monitor power usage since device is charging.')
      return False
    return True

  def StartMonitoringPowerAsync(self):
    assert not self._powermonitor_process_port, (
        'Must call StopMonitoringPowerAsync().')
    self._powermonitor_process_port = int(self._adb.RunShellCommand(
        '%s %d %s %s %s' % (self._file_poller_binary, SAMPLE_RATE_HZ,
            CHARGE_COUNTER, CURRENT, VOLTAGE))[0])

  def StopMonitoringPowerAsync(self):
    assert self._powermonitor_process_port, (
        'StartMonitoringPowerAsync() not called.')
    try:
      result = '\n'.join(self._adb.RunShellCommand(
          '%s %d' % (self._file_poller_binary,
                     self._powermonitor_process_port)))
      assert result, 'PowerMonitor produced no output'
      return DS2784PowerMonitor.ParseSamplingOutput(result)
    finally:
      self._powermonitor_process_port = None

  @staticmethod
  def ParseSamplingOutput(powermonitor_output):
    """Parse output of powermonitor command line utility.

    Returns:
        Dictionary in the format returned by StopMonitoringPowerAsync().
    """
    power_samples = []
    total_energy_consumption_mwh = 0
    def ParseSample(sample):
      values = [float(x) for x in sample.split(' ')]
      res = {}
      (res['timestamp_s'],
       res['charge_nah'],
       res['current_ua'],
       res['voltage_uv']) = values
      return res
    # The output contains a sample per line.
    samples = map(ParseSample, powermonitor_output.split('\n')[:-1])
    # Keep track of the last sample that found an updated reading.
    last_updated_sample = samples[0]
    # Compute average voltage.
    voltage_sum_uv = 0
    voltage_count = 0
    for sample in samples:
      if sample['charge_nah'] != last_updated_sample['charge_nah']:
        charge_difference_nah = (sample['charge_nah'] -
                                 last_updated_sample['charge_nah'])
        # Use average voltage for the energy consumption.
        voltage_sum_uv += sample['voltage_uv']
        voltage_count += 1
        average_voltage_uv = voltage_sum_uv / voltage_count
        total_energy_consumption_mwh += (-charge_difference_nah *
                                         average_voltage_uv / 10 ** 12)
        last_updated_sample = sample
        voltage_sum_uv = 0
        voltage_count = 0
      # Update average voltage.
      voltage_sum_uv += sample['voltage_uv']
      voltage_count += 1
      # Compute energy of the sample.
      energy_consumption_mw = (-sample['current_ua'] * sample['voltage_uv'] /
                               10 ** 9)

      power_samples.append(energy_consumption_mw)
    # Because the data is stalled for a few seconds, compute the remaining
    # energy consumption using the last available current reading.
    last_sample = samples[-1]
    remaining_time_h = (
        last_sample['timestamp_s'] - last_updated_sample['timestamp_s']) / 3600
    average_voltage_uv = voltage_sum_uv / voltage_count

    remaining_energy_consumption_mwh = (-last_updated_sample['current_ua'] *
                                        average_voltage_uv *
                                        remaining_time_h  / 10 ** 9)
    total_energy_consumption_mwh += remaining_energy_consumption_mwh

    # -------- Collect and Process Data -------------
    out_dict = {}
    # Raw power usage samples.
    out_dict['identifier'] = 'ds2784'
    out_dict['power_samples_mw'] = power_samples
    out_dict['energy_consumption_mwh'] = total_energy_consumption_mwh

    return out_dict
