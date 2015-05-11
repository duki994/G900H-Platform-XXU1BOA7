# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from telemetry import test
from telemetry.page import page_measurement
from telemetry.core.platform.profiler.samsung_memreport_profiler \
        import SamsungMemReportProfiler


def get_time():
  return "%s.%s" % (time.strftime("%H:%M:%S"), repr(time.time()).split('.')[1])

class SamsungMemoryMeasurement(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(SamsungMemoryMeasurement, self).__init__(*args, **kwargs)
    self.samsung_profiler = None

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    print "WillNavigateToPage: called", get_time()
    self.samsung_profiler = SamsungMemReportProfiler(
            tab.browser.browser_backend, None, "/tmp/samsung_memreport_output",
            None)
    print "WillNavigateToPage: samsung memreport initialized", get_time()

  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    time.sleep(2) # Wait for dynamic page loading after onload (FIXME properly)

    print "PAGE MEASURED", get_time()
    self.samsung_profiler.CollectProfile()
    result_dict = self.samsung_profiler.results

    mem_data = {}

    for value in result_dict.values():
      mem_data[value['name']] = {}
      for result in value['results']:
        for k, v in result.items():
          if k not in mem_data[value['name']]:
            mem_data[value['name']][k] = []
          mem_data[value['name']][k].append(v)

    cpu_total = []
    cpu_total_tmp = zip(*[x['cpu_total'] for x in mem_data.values()])
    for a, b in cpu_total_tmp:
      cpu_total.append(int(a)+int(b))

    cpu_high = max(cpu_total)
    cpu_low = min(cpu_total)
    cpu_avg = sum(cpu_total) / float(len(cpu_total))

    results.Add('cpu_high', 'kB', cpu_high, chart_name="CPU memory")
    results.Add('cpu_avg', 'kB', cpu_avg, chart_name="CPU memory")
    results.Add('cpu_low', 'kB', cpu_low, chart_name="CPU memory")

    gpu_total = []
    for value in mem_data.values():
      dma = value.get('dma', None)
      kgsl = value.get('kgsl', None)
      if dma and kgsl:
        for row in zip(dma, kgsl):
          gpu_total.append(sum(map(int, row)))
      else:
        ion_system = value.get('ion_system', None)
        if ion_system:
          gpu_total.extend(map(int, ion_system))

    gpu_high = max(gpu_total)
    gpu_low = min(gpu_total)
    gpu_avg = sum(gpu_total) / float(len(gpu_total))

    results.Add('gpu_high', 'kB', gpu_high, chart_name="GPU memory")
    results.Add('gpu_avg', 'kB', gpu_avg, chart_name="GPU memory")
    results.Add('gpu_low', 'kB', gpu_low, chart_name="GPU memory")

    results.AddSummary('cpu_gpu_high', 'kB', cpu_high + gpu_high,
            chart_name="Total")
    results.AddSummary('cpu_gpu_avg', 'kB', cpu_avg + gpu_avg,
            chart_name="Total")
    results.AddSummary('cpu_gpu_low', 'kB', cpu_low + gpu_low,
            chart_name="Total")


class SamsungMemoryTest(test.Test):
  test = SamsungMemoryMeasurement
  page_set = 'page_sets/samsung_memory.json'

