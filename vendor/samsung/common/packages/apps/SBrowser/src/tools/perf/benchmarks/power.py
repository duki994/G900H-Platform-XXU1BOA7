# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import numpy

from telemetry import test
from telemetry.page import page_measurement
from telemetry.core.platform.profiler.monsoon_profiler import MonsoonProfiler


def get_time():
  return "%s.%s" % (time.strftime("%H:%M:%S"), repr(time.time()).split('.')[1])

class MonsoonMeasurement(page_measurement.PageMeasurement):

  def __init__(self, *args, **kwargs):
    super(MonsoonMeasurement, self).__init__(*args, **kwargs)
    self._monsoon_profiler = None

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def CustomizeBrowserOptions(self, options):
    options.no_power_monitor = True

  def _ExecuteStep(self, step, tab):
    if step["action"] == "wait":
      time.sleep(step["seconds"])
    elif step["action"] == "wait_for_javascript":
      tab.WaitForJavaScriptExpression(
              step["expression"], step.get("timeout", 60))
    elif step["action"] == "execute_javascript":
      tab.ExecuteJavaScript(step["expression"])

  def MeasurePage(self, page, tab, results):
    power_consumptions = []
    for i in range(self.repeats):
      print "Measuring power, repeat %d/%d, at %s" \
              % (i+1, self.repeats, get_time())
      self._monsoon_profiler = MonsoonProfiler(None, None, "/dev/null", None)
      for step in self.steps:
        self._ExecuteStep(step, tab)
      print "Steps measured at", get_time()
      self._monsoon_profiler.CollectProfile()
      power_consumption = self.CalculatePower(self._monsoon_profiler.results)
      power_consumptions.append(power_consumption)

    results.Add('Power consumption', 'J', power_consumptions)

  @staticmethod
  def CalculatePower(results):
    x_timestamps, currents, voltages = zip(*results)
    y_powers = [a*v for a, v in zip(currents, voltages)]
    return numpy.trapz(y_powers, x_timestamps)

  @staticmethod
  def WithSteps(name, steps, repeats):
    """Factory method for creating preconfigured multipage measurement with
    given name, repeat count and steps"""
    members = {"steps": steps, "repeats": repeats}
    return type(name, (MonsoonMeasurement,), members)

  @staticmethod
  def WithSinglePageLoadSteps(name, url, wait_after_load_event=2, repeats=20):
    steps = [
      {"action": "execute_javascript",
          "expression": "window.location.href=\"about:blank\""},
      {"action": "execute_javascript",
          "expression": "window.location.href=\"%s\"" % url},
      {"action": "wait_for_javascript",
          "expression": "performance.timing.loadEventStart", "timeout": 60*5},
      {"action": "wait", "seconds": wait_after_load_event},
    ]
    return MonsoonMeasurement.WithSteps(name, steps, repeats)


class EspnLoadPowerTest(test.Test):
  test = MonsoonMeasurement.WithSinglePageLoadSteps("loading_espn",
      url="http://m.espn.go.com")
  page_set = 'page_sets/samsung_loading_espn.json'

class WikipediaLoadPowerTest(test.Test):
  test = MonsoonMeasurement.WithSinglePageLoadSteps("loading_wikipedia",
      url="http://www.wikipedia.com")
  page_set = 'page_sets/samsung_loading_wikipedia.json'

class YoutubeLoadPowerTest(test.Test):
  test = MonsoonMeasurement.WithSinglePageLoadSteps("loading_youtube",
      url="http://m.youtube.com")
  page_set = 'page_sets/samsung_loading_youtube.json'

class OneMinutePowerTest(test.Test):
  steps = [
    {"action": "execute_javascript",
        "expression": "window.location.href=\"http://m.youtube.com\""},
    {"action": "wait", "seconds": 15 },
    {"action": "execute_javascript",
        "expression": "window.location.href=\"http://www.google.com\""},
    {"action": "wait", "seconds": 5 },
    {"action": "execute_javascript",
        "expression": "window.location.href=\"http://m.espn.go.com\""},
    {"action": "wait", "seconds": 15 },
    {"action": "execute_javascript",
        "expression": "window.location.href=\"http://www.google.com\""},
    {"action": "wait", "seconds": 5 },
    {"action": "execute_javascript",
        "expression": "window.location.href=\"http://www.wikipedia.com\""},
    {"action": "wait", "seconds": 14 },
  ]
  test = MonsoonMeasurement.WithSteps("1min", steps, repeats=5)
  page_set = 'page_sets/samsung_1min_no_navigation.json'
