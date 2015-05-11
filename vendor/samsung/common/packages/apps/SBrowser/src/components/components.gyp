# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
  },
  'includes': [
    'autofill.gypi',
    'auto_login_parser.gypi',
    'breakpad.gypi',
    'cloud_devices.gypi',
    'dom_distiller.gypi',
    'json_schema.gypi',
    'language_usage_metrics.gypi',
    'navigation_metrics.gypi',
    'onc.gypi',
    'password_manager.gypi',
    'policy.gypi',
    'precache.gypi',
    'signin.gypi',
    'startup_metric_utils.gypi',
    'translate.gypi',
    'url_matcher.gypi',
    'user_prefs.gypi',
    'variations.gypi',
    'webdata.gypi',
  ],
  'conditions': [
    ['OS != "ios"', {
      'includes': [
        'browser_context_keyed_service.gypi',
        'navigation_interception.gypi',
        'plugins.gypi',
        'sessions.gypi',
        'storage_monitor.gypi',
        'visitedlink.gypi',
        'web_contents_delegate_android.gypi',
        'web_modal.gypi',
        'wifi.gypi',
      ],
    }],
    ['android_webview_build == 0 and enable_sync == 1', {
      # Android WebView fails to build if a dependency on sync.gyp:sync is
      # introduced.
      'includes': [
        'sync_driver.gypi',
      ],
    }],
    ['enable_signin==0', {
      'includes!': [
        'signin.gypi',
      ],
    }],
  ],
}
