# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import tile
from telemetry.page import page_measurement

class Tile(page_measurement.PageMeasurement):
  def __init__(self):
    super(Tile, self).__init__('stress_tile_management')
    self._tile_metric = None

  def CustomizeBrowserOptions(self, options):
    tile.TileMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'stress_tile_management')

  def WillRunActions(self, page, tab):
    self._tile_metric = tile.TileMetric()
    self._tile_metric.Start(page, tab)

  def DidRunActions(self, page, tab):
    self._tile_metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._tile_metric.AddResults(tab, results)
