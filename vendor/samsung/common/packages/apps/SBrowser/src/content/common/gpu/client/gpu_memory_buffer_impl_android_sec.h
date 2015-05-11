// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_ANDROID_SEC_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_ANDROID_SEC_H_

#include "content/common/gpu/client/graphic_buffer_sec.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

namespace content {

// Provides implementation of a GPU memory buffer based
// on a gpu memory handle.
class GpuMemoryBufferImplAndroidSEC : public GpuMemoryBufferImpl {
  public:
    GpuMemoryBufferImplAndroidSEC(const gfx::Size& size, unsigned internalformat);
    virtual ~GpuMemoryBufferImplAndroidSEC();

    bool InitializeFromServerSide(const gfx::GpuMemoryBufferHandle& handle, const gfx::Size& size);
    bool InitializeFromClientSide(const gfx::GpuMemoryBufferHandle& handle);

    // Overridden from gfx::GpuMemoryBuffer:
    virtual void Map(AccessMode mode, void** vaddr) OVERRIDE;
    virtual void Unmap() OVERRIDE;
    virtual gfx::GpuMemoryBufferHandle GetHandle() const OVERRIDE;
    bool RetainHandle(const gfx::GpuMemoryBufferHandle& handle);

    virtual void* GetNativeBuffer();
    virtual uint32 GetStride() const;

    static bool CanUseGpuMemory();
    static int GetMaxFileDescriptorCount();
    static void SetMaxNativeBuffer();

  private:
    GraphicBufferSEC* mGpuBuffer_;

    DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferImplAndroidSEC);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_MEMORY_BUFFER_IMPL_ANDROID_SEC_H_
