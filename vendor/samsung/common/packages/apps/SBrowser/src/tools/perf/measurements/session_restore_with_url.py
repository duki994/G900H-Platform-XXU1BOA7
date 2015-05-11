# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import session_restore


class SessionRestoreWithUrl(session_restore.SessionRestore):

  def __init__(self):
    super(SessionRestoreWithUrl, self).__init__(
        action_name_to_run='navigate_steps')

  def CanRunForPage(self, page):
    # Run for every page in the page set that has a startup url.
    return page.navigate_steps[0]['action'] == 'set_startup_url'
