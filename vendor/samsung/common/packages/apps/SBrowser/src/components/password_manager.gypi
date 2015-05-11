# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'password_manager_core_browser',
      'type': 'static_library',
      'dependencies': [
        'autofill_core_common',
        'encryptor',
        'password_manager_core_common',
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../sql/sql.gyp:sql',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
	       '<(android_src)/external/wbs/include/',
      ],
      'sources': [
        'password_manager/core/browser/login_database.cc',
        'password_manager/core/browser/login_database.h',
        'password_manager/core/browser/login_database_mac.cc',
        'password_manager/core/browser/login_database_posix.cc',
        'password_manager/core/browser/login_database_win.cc',
        'password_manager/core/browser/login_model.h',
        'password_manager/core/browser/password_manager_metrics_util.cc',
        'password_manager/core/browser/password_manager_metrics_util.h',
        'password_manager/core/browser/password_store.cc',
        'password_manager/core/browser/password_store.h',
        'password_manager/core/browser/password_store_change.h',
        'password_manager/core/browser/password_store_consumer.cc',
        'password_manager/core/browser/password_store_consumer.h',
        'password_manager/core/browser/password_store_default.cc',
        'password_manager/core/browser/password_store_default.h',
        'password_manager/core/browser/psl_matching_helper.cc',
        'password_manager/core/browser/psl_matching_helper.h',
      ],
        'link_settings': {
		'libraries': [
              '<(android_libs_dir)/libwbs.so',
        ],
	},
      'variables': {
        'conditions': [
          ['android_webview_build == 1 or enable_sync == 0', {
            # Android WebView doesn't support sync.
            'password_manager_enable_sync%': 0,
          }, {
            'password_manager_enable_sync%': 1,
          }],
        ],
      },
      'conditions': [
        ['OS=="mac"', {
          'sources!': [
            # TODO(blundell): Provide the iOS login DB implementation and then
            # also exclude the POSIX one from iOS. http://crbug.com/341429
            'password_manager/core/browser/login_database_posix.cc',
          ],
        }],
        ['password_manager_enable_sync == 1', {
          'defines': [
            'PASSWORD_MANAGER_ENABLE_SYNC',
          ],
          'dependencies': [
            '../sync/sync.gyp:sync',
          ],
          'direct_dependent_settings': {
            'defines': [
              'PASSWORD_MANAGER_ENABLE_SYNC',
            ],
          },
          'sources': [
            'password_manager/core/browser/password_syncable_service.cc',
            'password_manager/core/browser/password_syncable_service.h',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'password_manager_core_browser_test_support',
      'type': 'static_library',
      'dependencies': [
        'autofill_core_common',
        '../base/base.gyp:base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'password_manager/core/browser/mock_password_store.cc',
        'password_manager/core/browser/mock_password_store.h',
        'password_manager/core/browser/password_form_data.cc',
        'password_manager/core/browser/password_form_data.h',
        'password_manager/core/browser/test_password_store.cc',
        'password_manager/core/browser/test_password_store.h',
      ],
    },
    {
      'target_name': 'password_manager_core_common',
      'type': 'static_library',
      'dependencies': [
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'password_manager/core/common/password_manager_pref_names.cc',
        'password_manager/core/common/password_manager_pref_names.h',
        'password_manager/core/common/password_manager_switches.cc',
        'password_manager/core/common/password_manager_switches.h',
      ],
    },
  ],
}
