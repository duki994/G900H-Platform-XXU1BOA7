// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GPU_MEMORY_BUFFER_IMPL_MOCK_H_
#define UI_GL_GPU_MEMORY_BUFFER_IMPL_MOCK_H_

#include "ui/gfx/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace content {

class GpuMemoryBufferImplAndroidSEC {
  public:
    GpuMemoryBufferImplAndroidSEC(const gfx::Size& size, unsigned internalformat);
    virtual ~GpuMemoryBufferImplAndroidSEC();

    bool InitializeFromClientSide(const gfx::GpuMemoryBufferHandle& handle);
};

} // namespace content

#endif // UI_GL_GPU_MEMORY_BUFFER_IMPL_MOCK_H_
