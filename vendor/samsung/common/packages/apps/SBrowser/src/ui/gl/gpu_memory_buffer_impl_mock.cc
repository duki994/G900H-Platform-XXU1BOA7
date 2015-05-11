// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gpu_memory_buffer_impl_mock.h"

namespace content {

GpuMemoryBufferImplAndroidSEC::GpuMemoryBufferImplAndroidSEC(const gfx::Size& size, unsigned internalformat) {
}

GpuMemoryBufferImplAndroidSEC::~GpuMemoryBufferImplAndroidSEC() {
}

bool GpuMemoryBufferImplAndroidSEC::InitializeFromClientSide(const gfx::GpuMemoryBufferHandle& handle) {
  return false;
}

} // namespace content
