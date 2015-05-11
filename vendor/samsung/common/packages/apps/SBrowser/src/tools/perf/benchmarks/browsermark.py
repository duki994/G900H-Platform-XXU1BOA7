# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Browsermark CSS, DOM, WebGL, JS, resize and page load benchmarks.

Browsermark benchmark suite have five test groups:
a) CSS group: measures your browsers 2D and 3D performance, and finally executes
  CSS Crunch test
b) DOM group: measures variety of areas, like how well your browser traverse in
  Document Object Model Tree or how fast your browser can create dynamic content
c) General group: measures areas like resize and page load times
d) Graphics group: tests browsers Graphics Processing Unit power by measuring
  WebGL and Canvas performance
e) Javascript group: executes number crunching by doing selected Array and
  String operations
Additionally Browsermark will test your browsers conformance, but conformance
tests are not included in this suite.
"""

import os, unicodedata, re

from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set


def to_slug(s):
  """Convert string to slug that is safely usable as part of URL
  or variable."""
  slug = unicodedata.normalize('NFKD', s)
  slug = slug.encode('ascii', 'ignore').lower()
  slug = re.sub(r'[^a-z0-9]+', '_', slug).strip('-')
  slug = re.sub(r'[-]+', '_', slug)
  return slug


class BrowsermarkMeasurement(page_measurement.PageMeasurement):

  @staticmethod
  def estimate_total_score(category_scores):
    cs = category_scores
    return ((cs['CSS'] + cs['DOM'] + cs['Javascript'])
        + 2*(cs['General'] + cs['Graphics'])) / 20

  def MeasurePage(self, _, tab, results):
    # Select nearest server(North America=1) and start test.
    js_start_test =  """
        for (var i=0; i < $('#continent a').length; i++) {
          if (($('#continent a')[i]).getAttribute('data-id') == '1') {
            $('#continent a')[i].click();
            $('.start_test.enabled').click();
          }
        }
        """
    js_benchmark_available_test = 'window.benchmark !== undefined'
    js_test_results_available_test = 'window.capturedData !== null'
    js_browsermark_finished_test = \
      'window.location.pathname.indexOf("/results") != -1'
    js_overwrite_submitResult = """
        window.capturedData = null;
        window.benchmark.submitResultOrig = window.benchmark.submitResult;
        window.benchmark.submitResult = function(raw_score, guide, metadata) {
          window.capturedData = {
            'test_name': guide.testName,
            'raw_score': raw_score,
            'compare_score': guide.compareScore,
          };
          window.benchmark.submitResultOrig(raw_score, guide, metadata);
        }
    """
    js_get_captured_results = """
      window.capturedDataTmp = window.capturedData;
      window.capturedData = null;
      window.capturedDataTmp;
    """

    tab.ExecuteJavaScript(js_start_test)
    print "Overriding submitResults..."
    tab.WaitForJavaScriptExpression(js_benchmark_available_test, 2*60)
    tab.ExecuteJavaScript(js_overwrite_submitResult)

    score_overrides = { 'General Page Load': 5479.13 }
    category_scores = {}

    while not tab.EvaluateJavaScript(js_browsermark_finished_test):
      tab.WaitForJavaScriptExpression(js_test_results_available_test, 5*60)
      captured_results = tab.EvaluateJavaScript(js_get_captured_results)
      if captured_results is None:
        print "Could not capture data, retrying..."
        continue
      test_name = captured_results[u'test_name']
      test_category = test_name.split(' ', 1)[0]
      raw_score = captured_results[u'raw_score']
      compare_score = captured_results[u'compare_score']

      if test_name in score_overrides:
        test_score = score_overrides[test_name]
      elif compare_score == 1:
        test_score = raw_score
      else:
        test_score = 1000 * (raw_score / compare_score)

      if test_category not in category_scores:
        category_scores[test_category] = 0
      category_scores[test_category] += test_score

      print "Test score for %s: %f" % (test_name, test_score)
      results.Add(test_name, to_slug(test_name), test_score)

    print 'Category scores', category_scores
    results.Add('Score', 'score', self.estimate_total_score(category_scores))


class Browsermark(test.Test):
  """Browsermark suite tests CSS, DOM, resize, page load, WebGL and JS."""
  test = BrowsermarkMeasurement
  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../page_sets/data/browsermark.json',
        'make_javascript_deterministic': False,
        'pages': [
          { 'url':
              'http://browsermark.rightware.com/tests/'}
           ]
        }, os.path.abspath(__file__))

