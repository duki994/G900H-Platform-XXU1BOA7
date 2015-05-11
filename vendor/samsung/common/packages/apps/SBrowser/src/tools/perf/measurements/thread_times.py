# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import timeline
from telemetry.page import page_measurement

class ThreadTimes(page_measurement.PageMeasurement):
  def __init__(self):
    super(ThreadTimes, self).__init__('smoothness')
    self._metric = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-silk-results', action='store_true',
                      help='Report results relevant to silk.')
    parser.add_option('--report-silk-details', action='store_true',
                      help='Report details relevant to silk.')

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillRunActions(self, page, tab):
    self._metric = timeline.ThreadTimesTimelineMetric()
    self._metric.Start(page, tab)
    if self.options.report_silk_results:
      self._metric.results_to_report = timeline.ReportSilkResults
    if self.options.report_silk_details:
      self._metric.details_to_report = timeline.ReportSilkDetails

  def DidRunAction(self, page, tab, action):
    self._metric.AddActionToIncludeInMetric(action)

  def DidRunActions(self, page, tab):
    self._metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._metric.AddResults(tab, results)
