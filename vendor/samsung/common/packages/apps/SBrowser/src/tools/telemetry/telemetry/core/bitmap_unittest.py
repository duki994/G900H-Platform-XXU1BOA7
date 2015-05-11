# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import os
import unittest

from telemetry import test
from telemetry.core import bitmap
from telemetry.core import util


# This is a simple base64 encoded 2x2 PNG which contains, in order, a single
# Red, Yellow, Blue, and Green pixel.
test_png = """
iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91
JpzAAAAAXNSR0IArs4c6QAAAAlwSFlzAAALEwAACx
MBAJqcGAAAABZJREFUCNdj/M/AwPCfgYGB4T/DfwY
AHAAD/iOWZXsAAAAASUVORK5CYII=
"""
test_png_path = os.path.join(util.GetUnittestDataDir(), 'test_png.png')
test_png_2_path = os.path.join(util.GetUnittestDataDir(), 'test_png_2.png')


class BitmapTest(unittest.TestCase):

  # pylint: disable=C0324

  def testReadFromBase64Png(self):
    bmp = bitmap.Bitmap.FromBase64Png(test_png)

    self.assertEquals(2, bmp.width)
    self.assertEquals(2, bmp.height)

    bmp.GetPixelColor(0, 0).AssertIsRGB(255, 0, 0)
    bmp.GetPixelColor(1, 1).AssertIsRGB(0, 255, 0)
    bmp.GetPixelColor(0, 1).AssertIsRGB(0, 0, 255)
    bmp.GetPixelColor(1, 0).AssertIsRGB(255, 255, 0)

  def testReadFromPngFile(self):
    file_bmp = bitmap.Bitmap.FromPngFile(test_png_path)

    self.assertEquals(2, file_bmp.width)
    self.assertEquals(2, file_bmp.height)

    file_bmp.GetPixelColor(0, 0).AssertIsRGB(255, 0, 0)
    file_bmp.GetPixelColor(1, 1).AssertIsRGB(0, 255, 0)
    file_bmp.GetPixelColor(0, 1).AssertIsRGB(0, 0, 255)
    file_bmp.GetPixelColor(1, 0).AssertIsRGB(255, 255, 0)

  def testWritePngToPngFile(self):
    orig = bitmap.Bitmap.FromPngFile(test_png_path)
    temp_file = tempfile.NamedTemporaryFile().name
    orig.WritePngFile(temp_file)
    new_file = bitmap.Bitmap.FromPngFile(temp_file)
    self.assertTrue(orig.IsEqual(new_file))

  @test.Disabled
  def testWriteCroppedBmpToPngFile(self):
    pixels = [255,0,0, 255,255,0, 0,0,0,
              255,255,0, 0,255,0, 0,0,0]
    orig = bitmap.Bitmap(3, 3, 2, pixels)
    orig.Crop(0, 0, 2, 2)
    temp_file = tempfile.NamedTemporaryFile().name
    orig.WritePngFile(temp_file)
    new_file = bitmap.Bitmap.FromPngFile(temp_file)
    self.assertTrue(orig.IsEqual(new_file))

  def testIsEqual(self):
    bmp = bitmap.Bitmap.FromBase64Png(test_png)
    file_bmp = bitmap.Bitmap.FromPngFile(test_png_path)
    self.assertTrue(bmp.IsEqual(file_bmp))

  def testDiff(self):
    file_bmp = bitmap.Bitmap.FromPngFile(test_png_path)
    file_bmp_2 = bitmap.Bitmap.FromPngFile(test_png_2_path)

    diff_bmp = file_bmp.Diff(file_bmp)

    self.assertEquals(2, diff_bmp.width)
    self.assertEquals(2, diff_bmp.height)

    diff_bmp.GetPixelColor(0, 0).AssertIsRGB(0, 0, 0)
    diff_bmp.GetPixelColor(1, 1).AssertIsRGB(0, 0, 0)
    diff_bmp.GetPixelColor(0, 1).AssertIsRGB(0, 0, 0)
    diff_bmp.GetPixelColor(1, 0).AssertIsRGB(0, 0, 0)

    diff_bmp = file_bmp.Diff(file_bmp_2)

    self.assertEquals(3, diff_bmp.width)
    self.assertEquals(3, diff_bmp.height)

    diff_bmp.GetPixelColor(0, 0).AssertIsRGB(0, 255, 255)
    diff_bmp.GetPixelColor(1, 1).AssertIsRGB(255, 0, 255)
    diff_bmp.GetPixelColor(0, 1).AssertIsRGB(255, 255, 0)
    diff_bmp.GetPixelColor(1, 0).AssertIsRGB(0, 0, 255)

    diff_bmp.GetPixelColor(0, 2).AssertIsRGB(255, 255, 255)
    diff_bmp.GetPixelColor(1, 2).AssertIsRGB(255, 255, 255)
    diff_bmp.GetPixelColor(2, 0).AssertIsRGB(255, 255, 255)
    diff_bmp.GetPixelColor(2, 1).AssertIsRGB(255, 255, 255)
    diff_bmp.GetPixelColor(2, 2).AssertIsRGB(255, 255, 255)

  @test.Disabled
  def testGetBoundingBox(self):
    pixels = [0,0,0, 0,0,0, 0,0,0, 0,0,0,
              0,0,0, 1,0,0, 1,0,0, 0,0,0,
              0,0,0, 0,0,0, 0,0,0, 0,0,0]
    bmp = bitmap.Bitmap(3, 4, 3, pixels)
    box, count = bmp.GetBoundingBox(bitmap.RgbaColor(1, 0, 0))
    self.assertEquals(box, (1, 1, 2, 1))
    self.assertEquals(count, 2)

    box, count = bmp.GetBoundingBox(bitmap.RgbaColor(0, 1, 0))
    self.assertEquals(box, None)
    self.assertEquals(count, 0)

  @test.Disabled
  def testCrop(self):
    pixels = [0,0,0, 1,0,0, 2,0,0, 3,0,0,
              0,1,0, 1,1,0, 2,1,0, 3,1,0,
              0,2,0, 1,2,0, 2,2,0, 3,2,0]
    bmp = bitmap.Bitmap(3, 4, 3, pixels)
    bmp.Crop(1, 2, 2, 1)

    self.assertEquals(bmp.width, 2)
    self.assertEquals(bmp.height, 1)
    bmp.GetPixelColor(0, 0).AssertIsRGB(1, 2, 0)
    bmp.GetPixelColor(1, 0).AssertIsRGB(2, 2, 0)
    self.assertEquals(bmp.pixels, bytearray([1,2,0, 2,2,0]))

  @test.Disabled
  def testHistogram(self):
    pixels = [1,2,3, 1,2,3, 1,2,3, 1,2,3,
              1,2,3, 8,7,6, 5,4,6, 1,2,3,
              1,2,3, 8,7,6, 5,4,6, 1,2,3]
    bmp = bitmap.Bitmap(3, 4, 3, pixels)
    bmp.Crop(1, 1, 2, 2)

    histogram = bmp.ColorHistogram()
    self.assertEquals(sum(histogram), bmp.width * bmp.height * 3)
    self.assertEquals(histogram[1], 0)
    self.assertEquals(histogram[5], 2)
    self.assertEquals(histogram[8], 2)
    self.assertEquals(histogram[2 + 256], 0)
    self.assertEquals(histogram[4 + 256], 2)
    self.assertEquals(histogram[7 + 256], 2)
    self.assertEquals(histogram[3 + 512], 0)
    self.assertEquals(histogram[6 + 512], 4)

  @test.Disabled
  def testHistogramIgnoreColor(self):
    pixels = [1,2,3, 1,2,3, 1,2,3, 1,2,3,
              1,2,3, 8,7,6, 5,4,6, 1,2,3,
              1,2,3, 8,7,6, 5,4,6, 1,2,3]
    bmp = bitmap.Bitmap(3, 4, 3, pixels)

    histogram = bmp.ColorHistogram(ignore_color=bitmap.RgbaColor(1, 2, 3))
    self.assertEquals(histogram[1], 0)
    self.assertEquals(histogram[5], 2)
    self.assertEquals(histogram[8], 2)
    self.assertEquals(histogram[2 + 256], 0)
    self.assertEquals(histogram[4 + 256], 2)
    self.assertEquals(histogram[7 + 256], 2)
    self.assertEquals(histogram[3 + 512], 0)
    self.assertEquals(histogram[6 + 512], 4)

  @test.Disabled
  def testHistogramIgnoreColorTolerance(self):
    pixels = [1,2,3, 4,5,6,
              7,8,9, 8,7,6]
    bmp = bitmap.Bitmap(3, 2, 2, pixels)

    histogram = bmp.ColorHistogram(ignore_color=bitmap.RgbaColor(0, 1, 2),
                                   tolerance=1)
    self.assertEquals(histogram[1], 0)
    self.assertEquals(histogram[4], 1)
    self.assertEquals(histogram[7], 1)
    self.assertEquals(histogram[8], 1)
    self.assertEquals(histogram[2 + 256], 0)
    self.assertEquals(histogram[5 + 256], 1)
    self.assertEquals(histogram[7 + 256], 1)
    self.assertEquals(histogram[8 + 256], 1)
    self.assertEquals(histogram[3 + 512], 0)
    self.assertEquals(histogram[6 + 512], 2)
    self.assertEquals(histogram[9 + 512], 1)
