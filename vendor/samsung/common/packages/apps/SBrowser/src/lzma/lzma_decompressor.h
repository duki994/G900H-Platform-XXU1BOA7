// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LZMA_DECOMPRESSOR_H_
#define LZMA_DECOMPRESSOR_H_

#include <stdio.h>

#include <jni.h>
#include "base/basictypes.h"

#include <third_party/lzma_sdk/LzmaDec.h>
#include <third_party/lzma_sdk/Types.h>

class __attribute__((visibility("default"))) LzmaDecompressor {
 public:
  explicit LzmaDecompressor(int out_buffer_size);
  ~LzmaDecompressor();

  bool Initialize(const char* out_path_name);

  void Deinitialize();

  int DecompressChunk(char* data, int length);

  static bool RegisterLzmaDecompressorAndroidJni(JNIEnv* env);

 private:
  int out_buffer_size_;
  bool first_chunk_;

  FILE* out_file_;
  Byte* out_data_;

  CLzmaDec dec_;
  ISzAlloc alloc_;

  DISALLOW_COPY_AND_ASSIGN(LzmaDecompressor);
};

#endif  //LZMA_DECOMPRESSOR_H_
