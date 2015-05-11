#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for upload_symbols.py"""

from __future__ import print_function

import ctypes
import logging
import os
import sys
import urllib2

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.scripts import upload_symbols

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


class UploadSymbolsTest(cros_test_lib.MockTempDirTestCase):
  """Tests for UploadSymbols()"""

  def setUp(self):
    for d in ('foo', 'bar', 'some/dir/here'):
      d = os.path.join(self.tempdir, d)
      osutils.SafeMakedirs(d)
      for f in ('ignored', 'real.sym', 'no.sym.here'):
        f = os.path.join(d, f)
        osutils.Touch(f)

  def _testUploadURL(self, official, expected_url):
    """Helper for checking the url used"""
    m = upload_symbols.UploadSymbol = mock.Mock(return_value=0)
    with parallel_unittest.ParallelMock():
      ret = upload_symbols.UploadSymbols('', official=official, retry=False,
                                         breakpad_dir=self.tempdir, sleep=0)
      self.assertEqual(ret, 0)
      self.assertEqual(m.call_count, 3)
      for call_args in m.call_args_list:
        sym_file, url = call_args[0]
        self.assertEqual(url, expected_url)
        self.assertTrue(sym_file.endswith('.sym'))

  def testOfficialUploadURL(self):
    """Verify we upload to the real crash server for official builds"""
    self._testUploadURL(True, upload_symbols.OFFICIAL_UPLOAD_URL)

  def testUnofficialUploadURL(self):
    """Verify we upload to the staging crash server for unofficial builds"""
    self._testUploadURL(False, upload_symbols.STAGING_UPLOAD_URL)

  def testUploadSymbolFailureSimple(self):
    """Verify that when UploadSymbol fails, the error count is passed up"""
    def UploadSymbol(*_args, **kwargs):
      kwargs['num_errors'].value = 4
    upload_symbols.UploadSymbol = mock.Mock(side_effect=UploadSymbol)
    with parallel_unittest.ParallelMock():
      ret = upload_symbols.UploadSymbols('', breakpad_dir=self.tempdir, sleep=0,
                                         retry=False)
      self.assertEquals(ret, 4)

  def testUploadCount(self):
    """Verify we can limit the number of uploaded symbols"""
    m = upload_symbols.UploadSymbol = mock.Mock(return_value=0)
    for c in xrange(3):
      m.reset_mock()
      with parallel_unittest.ParallelMock():
        ret = upload_symbols.UploadSymbols('', breakpad_dir=self.tempdir,
                                           sleep=0, upload_count=c)
        self.assertEquals(ret, 0)
        self.assertEqual(m.call_count, c)

  def testFailedFileList(self):
    """Verify the failed file list is populated with the right content"""
    def UploadSymbol(*args, **kwargs):
      kwargs['failed_queue'].put(args[0])
      kwargs['num_errors'].value = 4
    upload_symbols.UploadSymbol = mock.Mock(side_effect=UploadSymbol)
    with parallel_unittest.ParallelMock():
      failed_list = os.path.join(self.tempdir, 'list')
      ret = upload_symbols.UploadSymbols('', breakpad_dir=self.tempdir, sleep=0,
                                         retry=False, failed_list=failed_list)
      self.assertEquals(ret, 4)

      # Need to sort the output as parallel/fs discovery can be unordered.
      got_list = sorted(osutils.ReadFile(failed_list).splitlines())
      exp_list = [
          'bar/real.sym',
          'foo/real.sym',
          'some/dir/here/real.sym',
      ]
      self.assertEquals(exp_list, got_list)


class UploadSymbolTest(cros_test_lib.MockTempDirTestCase):
  """Tests for UploadSymbol()"""

  def setUp(self):
    self.sym_file = os.path.join(self.tempdir, 'foo.sym')
    self.url = 'http://eatit'

  def testUploadSymbolNormal(self):
    """Verify we try to upload on a normal file"""
    m = upload_symbols.SymUpload = mock.Mock()
    osutils.Touch(self.sym_file)
    ret = upload_symbols.UploadSymbol(self.sym_file, self.url)
    self.assertEqual(ret, 0)
    m.assert_called_with(self.sym_file, self.url)
    self.assertEqual(m.call_count, 1)

  def testUploadSymbolErrorCountExceeded(self):
    """Verify that when the error count gets too high, we stop uploading"""
    errors = ctypes.c_int(10000)
    # Pass in garbage values so that we crash if num_errors isn't handled.
    ret = upload_symbols.UploadSymbol(None, None, sleep=None, num_errors=errors)
    self.assertEqual(ret, 0)

  def testUploadRetryErrors(self, side_effect=None):
    """Verify that we retry errors (and eventually give up)"""
    if not side_effect:
      side_effect = urllib2.HTTPError('http://', 400, 'fail', {}, None)
    m = upload_symbols.SymUpload = mock.Mock(side_effect=side_effect)
    errors = ctypes.c_int()
    ret = upload_symbols.UploadSymbol('/dev/null', self.url, num_errors=errors)
    self.assertEqual(ret, 1)
    m.assert_called_with('/dev/null', self.url)
    self.assertTrue(m.call_count >= upload_symbols.MAX_RETRIES)

  def testConnectRetryErrors(self):
    """Verify that we retry errors (and eventually give up) w/connect errors"""
    side_effect = urllib2.URLError('foo')
    self.testUploadRetryErrors(side_effect=side_effect)

  def testTruncateTooBigFiles(self):
    """Verify we shrink big files"""
    def SymUpload(sym_file, _url):
      content = osutils.ReadFile(sym_file)
      self.assertEqual(content, 'some junk\n')
    m = upload_symbols.SymUpload = mock.Mock(side_effect=SymUpload)
    content = (
        'STACK CFI 1234',
        'some junk',
        'STACK CFI 1234',
    )
    osutils.WriteFile(self.sym_file, '\n'.join(content))
    ret = upload_symbols.UploadSymbol(self.sym_file, self.url, file_limit=1)
    self.assertEqual(ret, 0)
    self.assertNotEqual(m.call_args[0][1], self.sym_file)
    self.assertEqual(m.call_count, 1)

  def testTruncateReallyLargeFiles(self):
    """Verify we try to shrink really big files"""
    warn_mock = self.PatchObject(cros_build_lib, 'PrintBuildbotStepWarnings')
    m = upload_symbols.SymUpload = mock.Mock()
    with open(self.sym_file, 'w+b') as f:
      f.truncate(upload_symbols.CRASH_SERVER_FILE_LIMIT + 100)
      f.seek(0)
      f.write('STACK CFI 1234\n\n')
    ret = upload_symbols.UploadSymbol(self.sym_file, self.url)
    self.assertEqual(ret, 0)
    self.assertNotEqual(m.call_args[0][1], self.sym_file)
    self.assertEqual(m.call_count, 1)
    self.assertEqual(warn_mock.call_count, 1)


class SymUploadTest(cros_test_lib.MockTempDirTestCase):
  """Tests for SymUpload()"""

  SYM_URL = 'http://localhost/post/it/here'
  SYM_CONTENTS = """MODULE Linux arm 123-456 blkid
PUBLIC 1471 0 main"""

  def setUp(self):
    self.sym_file = os.path.join(self.tempdir, 'test.sym')
    osutils.WriteFile(self.sym_file, self.SYM_CONTENTS)

  def testPostUpload(self):
    """Verify HTTP POST has all the fields we need"""
    m = self.PatchObject(urllib2, 'urlopen', autospec=True)
    upload_symbols.SymUpload(self.sym_file, self.SYM_URL)
    self.assertEquals(m.call_count, 1)
    req = m.call_args[0][0]
    self.assertEquals(req.get_full_url(), self.SYM_URL)
    data = ''.join([x for x in req.get_data()])

    fields = {
        'code_file': 'blkid',
        'debug_file': 'blkid',
        'debug_identifier': '123456',
        'os': 'Linux',
        'cpu': 'arm',
    }
    for key, val in fields.iteritems():
      line = 'Content-Disposition: form-data; name="%s"\r\n' % key
      self.assertTrue(line in data)
      line = '%s\r\n' % val
      self.assertTrue(line in data)
    line = ('Content-Disposition: form-data; name="symbol_file"; '
            'filename="test.sym"\r\n')
    self.assertTrue(line in data)
    self.assertTrue(self.SYM_CONTENTS in data)


if __name__ == '__main__':
  # pylint: disable=W0212
  # Set timeouts small so that if the unit test hangs, it won't hang for long.
  parallel._BackgroundTask.STARTUP_TIMEOUT = 5
  parallel._BackgroundTask.EXIT_TIMEOUT = 5

  # We want to test retry behavior, so make sure we don't sleep.
  upload_symbols.INITIAL_RETRY_DELAY = 0

  # Run the tests.
  cros_test_lib.main(level=logging.INFO)
