# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page.actions import page_action

class InteractAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(InteractAction, self).__init__(attributes)
    self._SetTimelineMarkerBaseName('InteractAction::RunAction')

  def RunAction(self, page, tab, previous_action):
    tab.ExecuteJavaScript(
        'console.time("' + self._GetUniqueTimelineMarkerName() + '")')
    raw_input("Interacting... Press Enter to continue.")
    tab.ExecuteJavaScript(
        'console.timeEnd("' + self._GetUniqueTimelineMarkerName() + '")')
