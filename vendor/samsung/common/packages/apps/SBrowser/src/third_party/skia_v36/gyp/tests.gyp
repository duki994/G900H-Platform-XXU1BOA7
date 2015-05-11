# GYP file to build unit tests.
{
  'includes': [
    'apptype_console.gypi',
  ],
  'targets': [
    {
      'target_name': 'tests',
      'type': 'executable',
      'includes': [
        'pathops_unittest.gypi',
        'tests.gypi',
      ],
      'sources': [
        '../tests/skia_test.cpp',
      ],
      'conditions': [
        [ 'skia_android_framework == 1', {
          'libraries': [
            '-lskia',
            '-lcutils',
          ],
          'libraries!': [
            '-lz',
            '-llog',
          ],
        }],
        [ 'skia_gpu == 1', {
          'include_dirs': [
            '../src/gpu',
          ],
        }],
      ],
    },
  ],
}
