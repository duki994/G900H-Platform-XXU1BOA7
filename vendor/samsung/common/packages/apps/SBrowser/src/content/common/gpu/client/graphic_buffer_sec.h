// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GRAPHIC_BUFFER_SEC_H_
#define CONTENT_COMMON_GPU_CLIENT_GRAPHIC_BUFFER_SEC_H_

#include <sys/types.h>

#include "base/synchronization/lock.h"

typedef int status_t;
// GRALLOC Enum
// taken enums from gralloc.h
enum
{
  GRBEX_USAGE_SW_READ_NEVER = 0x00000000,
  GRBEX_USAGE_SW_READ_RARELY = 0x00000002,
  GRBEX_USAGE_SW_READ_OFTEN = 0x00000003,
  GRBEX_USAGE_SW_READ_MASK = 0x0000000F,
  GRBEX_USAGE_SW_WRITE_NEVER = 0x00000000,
  GRBEX_USAGE_SW_WRITE_RARELY = 0x00000020,
  GRBEX_USAGE_SW_WRITE_OFTEN = 0x00000030,
  GRBEX_USAGE_HW_TEXTURE = 0x00000100,
};

// PIXEL_FORMAT_RGBA Enum
// taken enums from hardware.h
enum {
    HAL_PIXEL_FORMAT_RGBA_8888    = 1,
    HAL_PIXEL_FORMAT_RGBX_8888    = 2,
    HAL_PIXEL_FORMAT_RGB_888      = 3,
    HAL_PIXEL_FORMAT_RGB_565      = 4,
    HAL_PIXEL_FORMAT_BGRA_8888    = 5,
    HAL_PIXEL_FORMAT_RGBA_5551    = 6,
    HAL_PIXEL_FORMAT_RGBA_4444    = 7,
};

typedef void (*pfnGraphicBufferCtorP0)(void*);
typedef void (*pfnGraphicBufferCtorP4)(void*, uint32_t w, uint32_t h, uint32_t format, uint32_t usage);
typedef void (*pfnGraphicBufferDtor)(void*);
typedef int (*pfnGraphicBufferInitCheck)(void*);
typedef int (*pfnGraphicBufferLock)(void*, uint32_t usage, void **addr);
typedef int (*pfnGraphicBufferUnlock)(void*);
typedef int (*pfnGraphicBufferGetFlattenedSize)(void*);
typedef int (*pfnGraphicBufferGetFdCount)(void*);
typedef int (*pfnGraphicBufferFlatten)(void*, void*& buffer, size_t& size, int*& fds, size_t& count);
typedef int (*pfnGraphicBufferUnflatten)(void*, void const*& buffer, size_t& size, int const*& fds, size_t& count);
typedef void* (*pfnGraphicBufferGetNativeBuffer)(void*);

namespace content {

class GraphicBufferSEC {
public:
  GraphicBufferSEC();
  GraphicBufferSEC(int width, int height);

  virtual ~GraphicBufferSEC();

  static bool ValidateFunctions();
  static bool EnsureInitialized();

  status_t initCheck();

  // lock and unlock
  status_t Lock(uint32_t usage, void** vaddr);
  status_t Unlock();

  // Flattenable protocol
  size_t GetFlattenedSize();
  size_t GetFdCount();
  status_t Flatten(void*& buffer, size_t& size, int*& fds, size_t& count);
  status_t Unflatten(void const*& buffer, size_t& size, int const*& fds, size_t& count);

  // get native buffer
  void* GetNativeBuffer();

  void SetStride(int stride) { stride_ = stride; }
  int GetStride() { return stride_; }

private:
  void* mBuffer;
  bool should_free_the_buffer_;
  int stride_;

  static pfnGraphicBufferCtorP0 fGraphicBufferCtorP0;
  static pfnGraphicBufferCtorP4 fGraphicBufferCtorP4;
  static pfnGraphicBufferDtor fGraphicBufferDtor;
  static pfnGraphicBufferInitCheck fGraphicBufferInitCheck;
  static pfnGraphicBufferLock fGraphicBufferLock;
  static pfnGraphicBufferUnlock fGraphicBufferUnlock;
  static pfnGraphicBufferGetFlattenedSize fGraphicBufferGetFlattenedSize;
  static pfnGraphicBufferGetFdCount fGraphicBufferGetFdCount;
  static pfnGraphicBufferFlatten fGraphicBufferFlatten;
  static pfnGraphicBufferUnflatten fGraphicBufferUnflatten;
  static pfnGraphicBufferGetNativeBuffer fGraphicBufferGetNativeBuffer;

};

} // namespace content
#endif
