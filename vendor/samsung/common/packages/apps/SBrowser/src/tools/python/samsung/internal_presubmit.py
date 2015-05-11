# Copyright $2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit helper functions."""

import re

def PanProjectChecks(input_api, output_api, excluded_paths=None,
                     text_files=None, license_header=None,
                     project_name=None, maxlen=80):
  """Our version of PanProjectChecks from
  depot_tools/presubmit_canned_checks.py. We need to fork this function
  because we want to run a different set of default checks and we do not
  want to patch depot_tools."""
  excluded_paths = tuple(excluded_paths or [])
  text_files = tuple(text_files or (
     r'.+\.txt$',
     r'.+\.json$',
  ))
  black_list = input_api.DEFAULT_BLACK_LIST + excluded_paths
  white_list = input_api.DEFAULT_WHITE_LIST + text_files
  sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)
  text_files = lambda x: input_api.FilterSourceFile(
      x, black_list=black_list, white_list=white_list)

  results = []
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, maxlen, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeHasNoStrayWhitespace(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckSingletonInHeaders(
      input_api, output_api, source_file_filter=sources))
  results.extend(input_api.canned_checks.CheckChangeSvnEolStyle(
      input_api, output_api, source_file_filter=text_files))
  results.extend(input_api.canned_checks.CheckSvnForCommonMimeTypes(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckDoNotSubmitInDescription(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckDoNotSubmitInFiles(
      input_api, output_api))
  return results


def CheckDescription(input_api, output_api):
  """Check commit message requirements."""
  description_text = input_api.change.FullDescriptionText()
  lines = description_text.splitlines()
  results = []
  if len(lines) <= 1 or not (lines[0] != '' and lines[1] == ''):
    results.extend([output_api.PresubmitError(
        'Commit message should start with title followed by an '
        'empty line and detailed description.')])
  TAG_LINE_RE = re.compile(
      '^[ \t]*(?P<key>[\w-]*)[ \t]*:[ \t]*(?P<value>.*)[ \t]*$')
  tags = {}
  for line in lines:
    if len(line) > 80:
      results.extend([output_api.PresubmitError(
          'Found long line in commit message. '
          'Should be no longer then 80 characters.')])
    m = TAG_LINE_RE.match(line)
    if m:
      tags[m.group('key')] = m.group('value')
  if not 'Bug' in tags:
    results.extend([output_api.PresubmitError(
        'Missing bug url. Add Bug: [bugzilla url]')])
  if not 'Change-Id' in tags:
    results.extend([output_api.PresubmitError(
        'Missing Change-id. Forgot to add commit-hook?')])
  if not 'Signed-off-by' in tags:
    results.extend([output_api.PresubmitError(
        'Missing singe-off. Use git commit -s.')])
  return results
