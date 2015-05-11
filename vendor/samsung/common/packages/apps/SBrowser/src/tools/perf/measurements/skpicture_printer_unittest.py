# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import shutil
import tempfile

from measurements import skpicture_printer
from telemetry.page import page_measurement_unittest_base
from telemetry.unittest import options_for_unittests


class SkpicturePrinterUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.skp_outdir = tempfile.mkdtemp('_skp_test')

  def tearDown(self):
    shutil.rmtree(self._options.skp_outdir)

  def testSkpicturePrinter(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    measurement = skpicture_printer.SkpicturePrinter()
    results = self.RunMeasurement(measurement, ps, options=self._options)

    # Picture printing is not supported on all platforms.
    if results.failures:
      assert 'not supported' in results.failures[0][1]
      return

    saved_picture_count = results.FindAllPageSpecificValuesNamed(
        'saved_picture_count')
    self.assertEquals(len(saved_picture_count), 1)
    self.assertGreater(saved_picture_count[0].GetRepresentativeNumber(), 0)
