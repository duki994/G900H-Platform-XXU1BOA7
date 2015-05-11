// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl_android_sec.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>

#include "base/logging.h"
#include "base/file_util.h"

namespace content {

// if we use more FD than 800, change the texture mode from GPU to SharedMemory.
const static int limitFileDescriptorCount = 800;
const static int maxFileDescriptorCount = 1024;

static int max_fds_ = 0;
static int max_native_buffer_ = 0;
static int num_native_buffer_ = 0;
static struct pollfd fd_status_[maxFileDescriptorCount];

GpuMemoryBufferImplAndroidSEC::GpuMemoryBufferImplAndroidSEC(const gfx::Size& size, unsigned internalformat)
    : GpuMemoryBufferImpl(size, internalformat)
    , mGpuBuffer_(NULL) {
}

GpuMemoryBufferImplAndroidSEC::~GpuMemoryBufferImplAndroidSEC() {
  if (mGpuBuffer_) {
    delete mGpuBuffer_;
    --num_native_buffer_;
  }
}

bool GpuMemoryBufferImplAndroidSEC::InitializeFromClientSide(const gfx::GpuMemoryBufferHandle& handle) {
  mGpuBuffer_ = new GraphicBufferSEC();

  if (!mGpuBuffer_ || mGpuBuffer_->initCheck())
    return false;

  if (!RetainHandle(handle))
    return false;

  ++num_native_buffer_;
  return true;
}


bool GpuMemoryBufferImplAndroidSEC::InitializeFromServerSide(const gfx::GpuMemoryBufferHandle& handle, const gfx::Size& size) {
  mGpuBuffer_ = new GraphicBufferSEC(size.width(), size.height());

  if (!mGpuBuffer_ || mGpuBuffer_->initCheck())
    return false;

  ++num_native_buffer_;
  return true;
}

int GpuMemoryBufferImplAndroidSEC::GetMaxFileDescriptorCount() {
  struct rlimit limit;
  getrlimit(RLIMIT_NOFILE, &limit);
  if (static_cast<int>(limit.rlim_cur) > maxFileDescriptorCount)
    return maxFileDescriptorCount;
  return static_cast<int>(limit.rlim_cur);
}

void GpuMemoryBufferImplAndroidSEC::SetMaxNativeBuffer() {

    // check how many file descriptors does one grbuffer use on current hardware
    GraphicBufferSEC* gpu_buffer = new GraphicBufferSEC(64,64);

    size_t flattened_bytes = gpu_buffer->GetFlattenedSize();
    size_t fd_count = gpu_buffer->GetFdCount();
    
    int32 fds[fd_count];
    uint8_t flattened_grbuffer[flattened_bytes];
	
    int* fdArray = fds;
    void* buffer_in = flattened_grbuffer;
    size_t fd_count_in = fd_count;
    size_t flattened_bytes_in = flattened_bytes;
	
    gpu_buffer->Flatten(buffer_in, flattened_bytes_in, fdArray, fd_count_in);
    
    size_t valid_fd_count = 0;
    
    for (size_t i = 0; i < fd_count; i++) {
    	if (fds[i] > 0)
    	   valid_fd_count++;
    }
    
    if (valid_fd_count > 0) {
    	int max_fd_count = max_fds_ - 624; // default to 400 fds with normal 1024 process limit
    	if (max_fd_count < 0)
    	    max_fd_count = 0;
    	max_native_buffer_ = max_fd_count / valid_fd_count;
    }
    else
    	max_native_buffer_ = INT_MAX; // no limitations if grbuffer takes zero fd and no explicit limit set (mali case)
}

bool GpuMemoryBufferImplAndroidSEC::CanUseGpuMemory() {
  if (!GraphicBufferSEC::EnsureInitialized())
    return false;

  if (max_fds_ == 0)
    max_fds_ = GetMaxFileDescriptorCount();

  if(max_native_buffer_ == 0)
    SetMaxNativeBuffer();

  if(num_native_buffer_ >= max_native_buffer_)
    return false;

  int currentFDcount = max_fds_;
  memset(fd_status_, 0, sizeof(struct pollfd) * max_fds_);
  for(int i = 0; i < max_fds_; ++i)
    fd_status_[i].fd = i;

  poll(fd_status_, max_fds_, 0);
  for(int i = 0; i < max_fds_; ++i) {
    if(fd_status_[i].revents & POLLNVAL)
      --currentFDcount;
  }
  if (currentFDcount > (max_fds_ - 430))
    return false;
  return true;
}

void GpuMemoryBufferImplAndroidSEC::Map(AccessMode mode, void** vaddr) {
  DCHECK(!mapped_);
  DCHECK(vaddr);
  *vaddr = NULL;
  DCHECK(mGpuBuffer_);
  if (mGpuBuffer_->Lock(GRBEX_USAGE_SW_WRITE_OFTEN | GRBEX_USAGE_HW_TEXTURE, vaddr)) {
    LOG(ERROR)<<__FUNCTION__<<" : Fail to map the address";
    *vaddr = NULL;
    return;
  }
  mapped_ = true;
}

void GpuMemoryBufferImplAndroidSEC::Unmap() {
  DCHECK(mapped_);
  DCHECK(mGpuBuffer_);
  mGpuBuffer_->Unlock();
  mapped_ = false;
}

gfx::GpuMemoryBufferHandle GpuMemoryBufferImplAndroidSEC::GetHandle() const {
  if (!mGpuBuffer_) {
    LOG(ERROR)<<__FUNCTION__<<" : Invalide GpuMemoryBuffer";
    return gfx::GpuMemoryBufferHandle();
  }

  size_t buffer_size = mGpuBuffer_->GetFlattenedSize();
  size_t fd_count = mGpuBuffer_->GetFdCount();
  if (static_cast<int>(fd_count) > gfx::gpu_memory_buffer_handle_size) {
    LOG(ERROR)<<__FUNCTION__<<" : GPU exceeds default FDs count";
    return gfx::GpuMemoryBufferHandle();
  }

  size_t buffer_bytes = buffer_size + fd_count + 1;
  size_t fds_bytes = fd_count;
  int32 buffer[buffer_bytes];
  int32 fds[fds_bytes];

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::EGL_CLIENT_BUFFER_SEC;

  void* buffer_in = buffer;
  size_t buffer_size_in = buffer_size;
  int* fds_in = fds;
  size_t fd_count_in = fd_count;

  mGpuBuffer_->Flatten(buffer_in, buffer_size_in, fds_in, fd_count_in);
  // stride value is fourth in buffer
  // check out the flatten in GraphicBuffer.cpp
  mGpuBuffer_->SetStride(buffer[3]);

  handle.flattened_buffer.clear();
  handle.flattened_buffer.push_back(size_.width());
  handle.flattened_buffer.push_back(size_.height());
  handle.flattened_buffer.push_back(buffer_size);
  handle.flattened_buffer.push_back(fd_count);

  for (int i = 0 ; i < static_cast<int>(buffer_size) ; ++i) {
    int32 data = *(static_cast<int32*>(buffer) + i);
    handle.flattened_buffer.push_back(data);
  }

  for (int i = 0 ; i < static_cast<int>(fd_count); ++i)
    handle.handle_fd[i] = base::FileDescriptor(fds[i], false);

  return handle;
}

bool GpuMemoryBufferImplAndroidSEC::RetainHandle(const gfx::GpuMemoryBufferHandle& handle) {
  size_t size = static_cast<size_t>(handle.flattened_buffer[2]);
  size_t count = static_cast<size_t>(handle.flattened_buffer[3]);

  int32 buffer[size + 1];
  int32 fds[count + 1];

  for (int i = 0; i < static_cast<int32>(size); ++i)
    buffer[i] = handle.flattened_buffer[4 + i];

  // stride value is fourth in buffer
  // check out the flatten in GraphicBuffer.cpp
  mGpuBuffer_->SetStride(buffer[3]);

  void const* buffer_in = buffer;
  int const* fds_in = fds;
  size_t buffer_size_in = size;
  size_t fds_count_in = count;

  for (int i = 0; i < static_cast<int>(count); ++i)
    fds[i] = handle.handle_fd[i].fd;

  // check fds before unflattening them.
  for (int i = 0; i < static_cast<int>(count); ++i)
    if (fcntl(fds[i], F_GETFL, 0) == -1) {
      LOG(ERROR)<<__FUNCTION__<<" : Invalid file descriptor";
      return false;
    }

  if (mGpuBuffer_)
    return mGpuBuffer_->Unflatten(buffer_in, buffer_size_in, fds_in, fds_count_in);

  return false;
}

void* GpuMemoryBufferImplAndroidSEC::GetNativeBuffer() {
  DCHECK(mGpuBuffer_);
  return mGpuBuffer_->GetNativeBuffer();
}

uint32 GpuMemoryBufferImplAndroidSEC::GetStride() const {
  DCHECK(mGpuBuffer_);
  if (IsFormatValid(internalformat_)) // GL_BGRA8_EXT or GL_BGRA8_OES , 4 BPP
    return mGpuBuffer_->GetStride() * BytesPerPixel(internalformat_);

  LOG(ERROR)<<"Unknown internal format";
  return GpuMemoryBufferImpl::GetStride();
}

}  // namespace content
