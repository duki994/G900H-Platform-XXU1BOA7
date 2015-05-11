# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Bitmap is a basic wrapper for image pixels. It includes some basic processing
tools: crop, find bounding box of a color and compute histogram of color values.
"""

import array
import base64
import cStringIO
import struct
import subprocess
import sys

from telemetry.core import util

util.AddDirToPythonPath(util.GetTelemetryDir(), 'third_party', 'png')
import png  # pylint: disable=F0401


class RgbaColor(object):
  """Encapsulates an RGBA color retreived from a Bitmap"""

  def __init__(self, r, g, b, a=255):
    self.r = r
    self.g = g
    self.b = b
    self.a = a

  def __int__(self):
    return (self.r << 16) | (self.g << 8) | self.b

  def IsEqual(self, expected_color, tolerance=0):
    """Verifies that the color is within a given tolerance of
    the expected color"""
    r_diff = abs(self.r - expected_color.r)
    g_diff = abs(self.g - expected_color.g)
    b_diff = abs(self.b - expected_color.b)
    a_diff = abs(self.a - expected_color.a)
    return (r_diff <= tolerance and g_diff <= tolerance
        and b_diff <= tolerance and a_diff <= tolerance)

  def AssertIsRGB(self, r, g, b, tolerance=0):
    assert self.IsEqual(RgbaColor(r, g, b), tolerance)

  def AssertIsRGBA(self, r, g, b, a, tolerance=0):
    assert self.IsEqual(RgbaColor(r, g, b, a), tolerance)


WEB_PAGE_TEST_ORANGE = RgbaColor(222, 100,  13)
WHITE =                RgbaColor(255, 255, 255)


class _BitmapTools(object):
  """Wraps a child process of bitmaptools and allows for one command."""
  CROP_PIXELS = 0
  HISTOGRAM = 1
  BOUNDING_BOX = 2

  def __init__(self, dimensions, pixels):
    suffix = '.exe' if sys.platform == 'win32' else ''
    binary = util.FindSupportBinary('bitmaptools' + suffix)
    assert binary, 'You must build bitmaptools first!'

    self._popen = subprocess.Popen([binary],
                                   stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)

    # dimensions are: bpp, width, height, boxleft, boxtop, boxwidth, boxheight
    packed_dims = struct.pack('iiiiiii', *dimensions)
    self._popen.stdin.write(packed_dims)
    # If we got a list of ints, we need to convert it into a byte buffer.
    if type(pixels) is not bytearray:
      pixels = bytearray(pixels)
    self._popen.stdin.write(pixels)

  def _RunCommand(self, *command):
    assert not self._popen.stdin.closed, (
      'Exactly one command allowed per instance of tools.')
    packed_command = struct.pack('i' * len(command), *command)
    self._popen.stdin.write(packed_command)
    self._popen.stdin.close()
    length_packed = self._popen.stdout.read(struct.calcsize('i'))
    if not length_packed:
      raise Exception(self._popen.stderr.read())
    length = struct.unpack('i', length_packed)[0]
    return self._popen.stdout.read(length)

  def CropPixels(self):
    return self._RunCommand(_BitmapTools.CROP_PIXELS)

  def Histogram(self, ignore_color, tolerance):
    ignore_color = -1 if ignore_color is None else int(ignore_color)
    response = self._RunCommand(_BitmapTools.HISTOGRAM, ignore_color, tolerance)
    out = array.array('i')
    out.fromstring(response)
    return out

  def BoundingBox(self, color, tolerance):
    response = self._RunCommand(_BitmapTools.BOUNDING_BOX, int(color),
                                tolerance)
    unpacked = struct.unpack('iiiii', response)
    box, count = unpacked[:4], unpacked[-1]
    if box[2] < 0 or box[3] < 0:
      box = None
    return box, count


class Bitmap(object):
  """Utilities for parsing and inspecting a bitmap."""

  def __init__(self, bpp, width, height, pixels, metadata=None):
    assert bpp in [3, 4], 'Invalid bytes per pixel'
    assert width > 0, 'Invalid width'
    assert height > 0, 'Invalid height'
    assert pixels, 'Must specify pixels'
    assert bpp * width * height == len(pixels), 'Dimensions and pixels mismatch'

    self._bpp = bpp
    self._width = width
    self._height = height
    self._pixels = pixels
    self._metadata = metadata or {}
    self._crop_box = None

  @property
  def bpp(self):
    """Bytes per pixel."""
    return self._bpp

  @property
  def width(self):
    """Width of the bitmap."""
    return self._crop_box[2] if self._crop_box else self._width

  @property
  def height(self):
    """Height of the bitmap."""
    return self._crop_box[3] if self._crop_box else self._height

  def _PrepareTools(self):
    """Prepares an instance of _BitmapTools which allows exactly one command.
    """
    crop_box = self._crop_box or (0, 0, self._width, self._height)
    return _BitmapTools((self._bpp, self._width, self._height) + crop_box,
                        self._pixels)

  @property
  def pixels(self):
    """Flat pixel array of the bitmap."""
    if self._crop_box:
      self._pixels = self._PrepareTools().CropPixels()
      _, _, self._width, self._height = self._crop_box
      self._crop_box = None
    if type(self._pixels) is not bytearray:
      self._pixels = bytearray(self._pixels)
    return self._pixels

  @property
  def metadata(self):
    self._metadata['size'] = (self.width, self.height)
    self._metadata['alpha'] = self.bpp == 4
    self._metadata['bitdepth'] = 8
    return self._metadata

  def GetPixelColor(self, x, y):
    """Returns a RgbaColor for the pixel at (x, y)."""
    pixels = self.pixels
    base = self._bpp * (y * self._width + x)
    if self._bpp == 4:
      return RgbaColor(pixels[base + 0], pixels[base + 1],
                       pixels[base + 2], pixels[base + 3])
    return RgbaColor(pixels[base + 0], pixels[base + 1],
                     pixels[base + 2])

  def WritePngFile(self, path):
    with open(path, "wb") as f:
      png.Writer(**self.metadata).write_array(f, self.pixels)

  @staticmethod
  def FromPng(png_data):
    width, height, pixels, meta = png.Reader(bytes=png_data).read_flat()
    return Bitmap(4 if meta['alpha'] else 3, width, height, pixels, meta)

  @staticmethod
  def FromPngFile(path):
    with open(path, "rb") as f:
      return Bitmap.FromPng(f.read())

  @staticmethod
  def FromBase64Png(base64_png):
    return Bitmap.FromPng(base64.b64decode(base64_png))

  def IsEqual(self, other, tolerance=0):
    """Determines whether two Bitmaps are identical within a given tolerance."""

    # Dimensions must be equal
    if self.width != other.width or self.height != other.height:
      return False

    # Loop over each pixel and test for equality
    if tolerance or self.bpp != other.bpp:
      for y in range(self.height):
        for x in range(self.width):
          c0 = self.GetPixelColor(x, y)
          c1 = other.GetPixelColor(x, y)
          if not c0.IsEqual(c1, tolerance):
            return False
    else:
      return self.pixels == other.pixels

    return True

  def Diff(self, other):
    """Returns a new Bitmap that represents the difference between this image
    and another Bitmap."""

    # Output dimensions will be the maximum of the two input dimensions
    out_width = max(self.width, other.width)
    out_height = max(self.height, other.height)

    diff = [[0 for x in xrange(out_width * 3)] for x in xrange(out_height)]

    # Loop over each pixel and write out the difference
    for y in range(out_height):
      for x in range(out_width):
        if x < self.width and y < self.height:
          c0 = self.GetPixelColor(x, y)
        else:
          c0 = RgbaColor(0, 0, 0, 0)

        if x < other.width and y < other.height:
          c1 = other.GetPixelColor(x, y)
        else:
          c1 = RgbaColor(0, 0, 0, 0)

        offset = x * 3
        diff[y][offset] = abs(c0.r - c1.r)
        diff[y][offset+1] = abs(c0.g - c1.g)
        diff[y][offset+2] = abs(c0.b - c1.b)

    # This particular method can only save to a file, so the result will be
    # written into an in-memory buffer and read back into a Bitmap
    diff_img = png.from_array(diff, mode='RGB')
    output = cStringIO.StringIO()
    try:
      diff_img.save(output)
      diff = Bitmap.FromPng(output.getvalue())
    finally:
      output.close()

    return diff

  def GetBoundingBox(self, color, tolerance=0):
    """Finds the minimum box surrounding all occurences of |color|.
    Returns: (top, left, width, height), match_count
    Ignores the alpha channel."""
    return self._PrepareTools().BoundingBox(color, tolerance)

  def Crop(self, left, top, width, height):
    """Crops the current bitmap down to the specified box."""
    cur_box = self._crop_box or (0, 0, self._width, self._height)
    cur_left, cur_top, cur_width, cur_height = cur_box

    if (left < 0 or top < 0 or
        (left + width) > cur_width or
        (top + height) > cur_height):
      raise ValueError('Invalid dimensions')

    self._crop_box = cur_left + left, cur_top + top, width, height
    return self

  def ColorHistogram(self, ignore_color=None, tolerance=0):
    """Computes a histogram of the pixel colors in this Bitmap.
    Args:
      ignore_color: An RgbaColor to exclude from the bucket counts.
      tolerance: A tolerance for the ignore_color.

    Returns:
      A list of 3x256 integers formatted as
      [r0, r1, ..., g0, g1, ..., b0, b1, ...].
    """
    return self._PrepareTools().Histogram(ignore_color, tolerance)
