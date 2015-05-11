# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_test_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'dependencies': [
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',

      ],
      'sources': [
        '<(DEPTH)/media/cast/test/fake_single_thread_task_runner.cc',
        '<(DEPTH)/media/cast/test/fake_single_thread_task_runner.h',
        'input_builder.cc',
        'input_builder.h',
        'audio_utility.cc',
        'audio_utility.h',
        'video_utility.cc',
        'video_utility.h',
      ], # source
    },
  ],
}