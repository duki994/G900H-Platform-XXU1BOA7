# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # Default value for all libraries.
  'custom_configure_flags': '',
  'custom_c_compiler_flags': '',
  'custom_cxx_compiler_flags': '',
  'custom_linker_flags': '',
  'run_before_build': '',

  'variables': {
    'verbose_libraries_build%': 0,
  },
  'conditions': [
    ['asan==1', {
      'sanitizer_type': 'asan',
    }],
    ['msan==1', {
      'sanitizer_type': 'msan',
    }],
    ['verbose_libraries_build==1', {
      'verbose_libraries_build_flag': '--verbose',
    }, {
      'verbose_libraries_build_flag': '',
    }],
    ['use_goma==1', {
      'cc': '<(gomadir)/gomacc <!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang',
      'cxx': '<(gomadir)/gomacc <!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang++',
    }, {
      'cc': '<!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang',
      'cxx': '<!(cd <(DEPTH) && pwd -P)/<(make_clang_dir)/bin/clang++',
    }],
  ],
  'targets': [
    {
      'target_name': 'instrumented_libraries',
      'type': 'none',
      'variables': {
        'prune_self_dependency': 1,
      },
      'dependencies': [
        '<(_sanitizer_type)-libcairo2',
        '<(_sanitizer_type)-libexpat1',
        '<(_sanitizer_type)-libffi6',
        '<(_sanitizer_type)-libgcrypt11',
        '<(_sanitizer_type)-libgpg-error0',
        '<(_sanitizer_type)-libnspr4',
        '<(_sanitizer_type)-libp11-kit0',
        '<(_sanitizer_type)-libpcre3',
        '<(_sanitizer_type)-libpng12-0',
        '<(_sanitizer_type)-libx11-6',
        '<(_sanitizer_type)-libxau6',
        '<(_sanitizer_type)-libxcb1',
        '<(_sanitizer_type)-libxcomposite1',
        '<(_sanitizer_type)-libxcursor1',
        '<(_sanitizer_type)-libxdamage1',
        '<(_sanitizer_type)-libxdmcp6',
        '<(_sanitizer_type)-libxext6',
        '<(_sanitizer_type)-libxfixes3',
        '<(_sanitizer_type)-libxi6',
        '<(_sanitizer_type)-libxinerama1',
        '<(_sanitizer_type)-libxrandr2',
        '<(_sanitizer_type)-libxrender1',
        '<(_sanitizer_type)-libxss1',
        '<(_sanitizer_type)-libxtst6',
        '<(_sanitizer_type)-zlib1g',
        '<(_sanitizer_type)-libglib2.0-0',
        '<(_sanitizer_type)-libdbus-1-3',
        '<(_sanitizer_type)-libdbus-glib-1-2',
        '<(_sanitizer_type)-nss',
        '<(_sanitizer_type)-libfontconfig1',
        '<(_sanitizer_type)-pulseaudio',
        '<(_sanitizer_type)-libasound2',
        '<(_sanitizer_type)-libcups2',
        '<(_sanitizer_type)-pango1.0',
        '<(_sanitizer_type)-libcap2',
        '<(_sanitizer_type)-libudev0',
        '<(_sanitizer_type)-libtasn1-3',
      ],
      'conditions': [
        ['asan==1', {
          'dependencies': [
            '<(_sanitizer_type)-libpixman-1-0',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'fix_rpaths',
          'inputs': [
            'fix_rpaths.sh',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/rpaths.fixed.txt',
          ],
          'action': [
            '<(DEPTH)/third_party/instrumented_libraries/fix_rpaths.sh',
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)'
          ],
        },
      ],
    },
    {
      'library_name': 'freetype',
      'dependencies=': [],
      'custom_configure_flags': '',
      'run_before_build': 'freetype.sh',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libcairo2',
      'dependencies=': [],
      'custom_configure_flags': '--disable-gtk-doc',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libdbus-1-3',
      'dependencies=': [
        '<(_sanitizer_type)-libglib2.0-0',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libdbus-glib-1-2',
      'dependencies=': [
        '<(_sanitizer_type)-libglib2.0-0',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libexpat1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libffi6',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libfontconfig1',
      'dependencies=': [
        '<(_sanitizer_type)-freetype',
      ],
      'custom_configure_flags': '--disable-docs',
      'run_before_build': 'libfontconfig.sh',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libgcrypt11',
      'dependencies=': [],
      'custom_linker_flags': '-Wl,-z,muldefs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libglib2.0-0',
      'dependencies=': [],
      'custom_configure_flags': [
        '--disable-gtk-doc',
        '--disable-gtk-doc-html',
        '--disable-gtk-doc-pdf',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libgpg-error0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libnspr4',
      'dependencies=': [],
      'custom_configure_flags': '--enable-64bit',
      'run_before_build': 'libnspr4.sh',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libp11-kit0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libpcre3',
      'dependencies=': [],
      'custom_configure_flags': [
        '--enable-utf8',
        '--enable-unicode-properties',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libpixman-1-0',
      'dependencies=': [
        '<(_sanitizer_type)-libglib2.0-0',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libpng12-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libx11-6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxau6',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxcb1',
      'dependencies=': [],
      'custom_configure_flags': '--disable-build-docs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxcomposite1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxcursor1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxdamage1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxdmcp6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-docs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxext6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxfixes3',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxi6',
      'dependencies=': [],
      'custom_configure_flags': [
        '--disable-specs',
        '--disable-docs',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxinerama1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxrandr2',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxrender1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxss1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxtst6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'zlib1g',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'nss',
      'dependencies=': [
        '<(_sanitizer_type)-libnspr4',
      ],
      'run_before_build': 'nss.sh',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'pulseaudio',
      'dependencies=': [],
      'run_before_build': 'pulseaudio.sh',
      'custom_configure_flags': '--with-udev-rules-dir=<(INTERMEDIATE_DIR)/udev/rules.d',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libasound2',
      'dependencies=': [],
      'run_before_build': 'libasound2.sh',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libcups2',
      'dependencies=': [],
      'run_before_build': 'libcups2.sh',
      'custom_configure_flags': [
        # Do not touch system-wide directories.
        '--with-rcdir=no',
        '--with-xinetd=no',
        '--with-dbusdir=no',
        '--with-menudir=no',
        '--with-icondir=no',
        '--with-docdir=no'
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'pango1.0',
      'dependencies=': [
        '<(_sanitizer_type)-libglib2.0-0',
      ],
      'custom_configure_flags': [
        # Avoid https://bugs.gentoo.org/show_bug.cgi?id=425620
        '--enable-introspection=no',
        # More flags are set in download_build_install.py.
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libcap2',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libudev0',
      'dependencies=': [],
      'custom_configure_flags': [
          # Without this flag there's a linking step that doesn't honor LDFLAGS
          # and fails.
          # TODO(earthdok): find a better fix.
          '--disable-gudev'
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libtasn1-3',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
  ],
}
