#!/usr/bin/env bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the flags that should be passed to clang.

THIS_ABS_DIR=$(cd $(dirname $0) && echo $PWD)
CLANG_LIB_PATH=$THIS_ABS_DIR/../../../third_party/llvm-build/Release+Asserts/lib

if uname -s | grep -q Darwin; then
  LIBSUFFIX=dylib
else
  LIBSUFFIX=so
fi

FLAGS=""
if [[ "$1" = "enable-oilpan=1" ]]; then
    FLAGS="$FLAGS -Xclang -plugin-arg-blink-gc-plugin -Xclang enable-oilpan"
fi

echo -Xclang -load -Xclang $CLANG_LIB_PATH/libBlinkGCPlugin.$LIBSUFFIX \
  -Xclang -add-plugin -Xclang blink-gc-plugin $FLAGS
