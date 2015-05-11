# Copyright (c) 2012-2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{    
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'lzma',
      'type': 'shared_library',      
      'defines': [
        '_LZMA_PROB32',
      ],
      'include_dirs': [        
        '..',
      ],
      'sources': [         
        'lzma_decompressor.cc', 
        'lzma_decompressor.h',          
      ],       
      'dependencies': [
	 '../base/base.gyp:base',
	 '../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
         'lzmadecompressor_jni_headers',
      ],                
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'lzmadecompressor_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/lzma/LzmaDecompressor.java',
          ],
          'variables': {
            'jni_gen_package': 'lzma',
            'jni_generator_ptr_type': 'long',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        }, 
        {
          'target_name': 'lzma_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../lzma/android/java',
          },          
          'includes': [ '../build/java.gypi'  ],
        }      
      ]
    }],
  ]
}
