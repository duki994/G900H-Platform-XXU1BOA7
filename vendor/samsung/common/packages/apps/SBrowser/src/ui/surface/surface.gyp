# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'conditions': [
      ['use_x11 == 1', {
        'include_dirs': [
          '<(DEPTH)/third_party/khronos',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'surface',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/ui/events/events.gyp:events_base',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui',
      ],
      'sources': [
        'accelerated_surface_mac.cc',
        'accelerated_surface_mac.h',
        'surface_export.h',
        'transport_dib.h',
        'transport_dib.cc',
        'transport_dib_gtk.cc',
        'transport_dib_posix.cc',
        'transport_dib_win.cc',
      ],
      'defines': [
        'SURFACE_IMPLEMENTATION',
      ],
    },
  ],
}
