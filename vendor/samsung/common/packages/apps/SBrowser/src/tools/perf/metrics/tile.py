# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from metrics import histogram_util
from metrics import Metric

_HISTOGRAMS = [
    {'name': 'LayerTreeHostImpl.NumMissingTiles', 'units': 'count',
     'display_name': 'tiles_num_missing',
     'type': histogram_util.RENDERER_HISTOGRAM}]

class TileMetric(Metric):
  def __init__(self):
    super(TileMetric, self).__init__()
    self._histogram_start = None
    self._histogram_delta = None

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs([
      '--enable-stats-collection-bindings'])

  def Start(self, page, tab):
    self._histogram_start = dict()
    for h in _HISTOGRAMS:
      histogram_data = histogram_util.GetHistogram(
          h['type'], h['name'], tab)
      # Histogram data may not be available
      if not histogram_data or histogram_data == '{}':
        continue
      self._histogram_start[h['name']] = histogram_data

  def Stop(self, page, tab):
    assert self._histogram_start is not None, 'Must call Start() first'
    self._histogram_delta = dict()
    for h in _HISTOGRAMS:
      histogram_data = histogram_util.GetHistogram(
          h['type'], h['name'], tab)
      # Histogram data may not be available
      if not histogram_data or histogram_data == '{}':
        continue
      self._histogram_delta[h['name']] = histogram_util.SubtractHistogram(
          histogram_data, self._histogram_start.get(h['name'], '{}'))

  def AddResults(self, tab, results):
    assert self._histogram_delta is not None, 'Must call Stop() first'
    for h in _HISTOGRAMS:
      delta = self._histogram_delta.get(h['name'])
      results.Add(h['display_name'], h['units'],
                  json.loads(delta)['count'] if delta else 0,
                  data_type='unimportant')
