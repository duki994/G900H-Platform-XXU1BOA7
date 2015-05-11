# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import cpu
from metrics import memory
from telemetry.page import page_measurement


class WebRTC(page_measurement.PageMeasurement):
  """Gathers WebRTC-related metrics on a page set."""

  def __init__(self):
    super(WebRTC, self).__init__('webrtc')
    self._cpu_metric = None
    self._memory_metric = None

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.CpuMetric(browser)
    self._memory_metric = memory.MemoryMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._cpu_metric.Start(page, tab)
    self._memory_metric.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--use-fake-device-for-media-stream')
    options.AppendExtraBrowserArgs('--use-fake-ui-for-media-stream')

  def MeasurePage(self, page, tab, results):
    """Measure the page's performance."""
    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)

    self._cpu_metric.Stop(page, tab)
    self._cpu_metric.AddResults(tab, results)

    tab.EvaluateJavaScript('checkForErrors();')
