# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# This gypi file contains the shim header generation and other settings to use
# the system version of skia on Android.
{
  'direct_dependent_settings': {
    'include_dirs': [
      ##### System  Includes     
      '<(android_src)/system/core/include',
      ##### SKIA Includes     
      '<(android_src)/external/skia/src/core',
      '<(android_src)/external/skia/include',       
      '<(android_src)/external/skia/include/core', 
      '<(android_src)/external/skia/include/config',
      '<(android_src)/external/skia/include/effects',
      '<(android_src)/external/skia/include/gpu',
      '<(android_src)/external/skia/include/images',
      '<(android_src)/external/skia/include/lazy',
      '<(android_src)/external/skia/include/pathops',
      '<(android_src)/external/skia/include/pdf',
      '<(android_src)/external/skia/include/pipe',
      '<(android_src)/external/skia/include/ports',
      '<(android_src)/external/skia/include/utils',
      '<(android_src)/external/skia/include/views',
      '<(android_src)/external/skia/include/xml',
    ],
  },
  'link_settings': {
    'ldflags': [
     	'-Wl,--no-fatal-warnings',
    ],
    'ldflags!': [
    	'-Wl,--fatal-warnings',
    ],
    'libraries': [ '<(android_libs_dir)/libskia.so' ],
  },
  'variables': {
    'headers_root_path': '../third_party/skia/include',
  },
  'includes': [
    '../third_party/skia/gyp/public_headers.gypi',
    '../build/shim_headers.gypi',
  ],
}
