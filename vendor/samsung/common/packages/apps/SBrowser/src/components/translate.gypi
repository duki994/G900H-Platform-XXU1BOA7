# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'translate_core_browser',
      'type': 'static_library',
      'dependencies': [
        'component_resources.gyp:component_resources',
        'language_usage_metrics',
        'translate_core_common',
        'user_prefs',
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        '../ui/ui.gyp:ui',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'translate/core/browser/language_state.cc',
        'translate/core/browser/language_state.h',
        'translate/core/browser/page_translated_details.h',
        'translate/core/browser/translate_accept_languages.cc',
        'translate/core/browser/translate_accept_languages.h',
        'translate/core/browser/translate_browser_metrics.cc',
        'translate/core/browser/translate_browser_metrics.h',
        'translate/core/browser/translate_download_manager.cc',
        'translate/core/browser/translate_download_manager.h',
        'translate/core/browser/translate_driver.h',
        'translate/core/browser/translate_error_details.h',
        'translate/core/browser/translate_event_details.cc',
        'translate/core/browser/translate_event_details.h',
        'translate/core/browser/translate_language_list.cc',
        'translate/core/browser/translate_language_list.h',
        'translate/core/browser/translate_prefs.cc',
        'translate/core/browser/translate_prefs.h',
        'translate/core/browser/translate_script.cc',
        'translate/core/browser/translate_script.h',
        'translate/core/browser/translate_url_fetcher.cc',
        'translate/core/browser/translate_url_fetcher.h',
        'translate/core/browser/translate_url_util.cc',
        'translate/core/browser/translate_url_util.h',
       ],
    },
    {
      'target_name': 'translate_core_common',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'translate/core/common/translate_constants.cc',
        'translate/core/common/translate_constants.h',
        'translate/core/common/translate_errors.h',
        'translate/core/common/translate_metrics.cc',
        'translate/core/common/translate_metrics.h',
        'translate/core/common/translate_pref_names.cc',
        'translate/core/common/translate_pref_names.h',
        'translate/core/common/translate_switches.cc',
        'translate/core/common/translate_switches.h',
        'translate/core/common/translate_util.cc',
        'translate/core/common/translate_util.h',
        'translate/core/common/language_detection_details.cc',
        'translate/core/common/language_detection_details.h',
      ],
    },
    {
      'target_name': 'translate_language_detection',
      'type': 'static_library',
      'dependencies': [
        'translate_core_common',
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'translate/language_detection/language_detection_util.cc',
        'translate/language_detection/language_detection_util.h',
      ],
      'conditions': [
        ['cld_version==0 or cld_version==1', {
          'dependencies': [
            '<(DEPTH)/third_party/cld/cld.gyp:cld',
          ],
        }],
        ['cld_version==0 or cld_version==2', {
          'dependencies': [
            '<(DEPTH)/third_party/cld_2/cld_2.gyp:cld_2',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS != "ios"', {
      'targets': [
        {
          'target_name': 'translate_content_browser',
          'type': 'static_library',
          'dependencies': [
            'translate_core_browser',
            '../base/base.gyp:base',
            '../content/content.gyp:content_browser',
          ],
          'include_dirs': [
            '..',
          ],
          'sources': [
            'translate/content/browser/content_translate_driver.cc',
            'translate/content/browser/content_translate_driver.h',
           ],
        },
      ],
    }],
  ],
}
