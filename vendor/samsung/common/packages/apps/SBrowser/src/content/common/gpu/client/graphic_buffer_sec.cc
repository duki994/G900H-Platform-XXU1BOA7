// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/graphic_buffer_sec.h"

#include <dlfcn.h>

#include "base/logging.h"
#include "base/file_util.h"

namespace content {

const char* ANDROID_LIBUI_LIBRARY = "libui.so";
const int NO_ERROR = 0;
// FIXME : actuall buffer size 120 bytes
// but because of uncertain platforms, I double the value.
const int GRAPHIC_BUFFER_SIZE = 240;

// static function pointers of GraphicBuffer
pfnGraphicBufferCtorP0 GraphicBufferSEC::fGraphicBufferCtorP0 = NULL;
pfnGraphicBufferCtorP4 GraphicBufferSEC::fGraphicBufferCtorP4 = NULL;
pfnGraphicBufferDtor GraphicBufferSEC::fGraphicBufferDtor = NULL;
pfnGraphicBufferInitCheck GraphicBufferSEC::fGraphicBufferInitCheck = NULL;
pfnGraphicBufferLock GraphicBufferSEC::fGraphicBufferLock = NULL;
pfnGraphicBufferUnlock GraphicBufferSEC::fGraphicBufferUnlock = NULL;
pfnGraphicBufferGetFlattenedSize GraphicBufferSEC::fGraphicBufferGetFlattenedSize = NULL;
pfnGraphicBufferGetFdCount GraphicBufferSEC::fGraphicBufferGetFdCount = NULL;
pfnGraphicBufferFlatten GraphicBufferSEC::fGraphicBufferFlatten = NULL;
pfnGraphicBufferUnflatten GraphicBufferSEC::fGraphicBufferUnflatten = NULL;
pfnGraphicBufferGetNativeBuffer GraphicBufferSEC::fGraphicBufferGetNativeBuffer = NULL;

GraphicBufferSEC::GraphicBufferSEC()
  : mBuffer(NULL)
  , should_free_the_buffer_(false)
  , stride_(0) {
  if (!EnsureInitialized())
    return;

  DCHECK(fGraphicBufferCtorP0);

  mBuffer = new int [GRAPHIC_BUFFER_SIZE]();
  fGraphicBufferCtorP0(mBuffer);
}

GraphicBufferSEC::GraphicBufferSEC(int width, int height)
  : mBuffer(NULL)
  , should_free_the_buffer_(true)
  , stride_(0) {
  if (!EnsureInitialized())
    return;

  mBuffer = new int [GRAPHIC_BUFFER_SIZE]();
  DCHECK(fGraphicBufferCtorP4);
  fGraphicBufferCtorP4(mBuffer, width, height,
                       HAL_PIXEL_FORMAT_RGBA_8888,
                       GRBEX_USAGE_SW_WRITE_OFTEN |
                       GRBEX_USAGE_SW_READ_OFTEN |
                       GRBEX_USAGE_HW_TEXTURE);
}

GraphicBufferSEC::~GraphicBufferSEC() {
  if (!EnsureInitialized())
    return;

  if (mBuffer && should_free_the_buffer_) {
    DCHECK(fGraphicBufferDtor);
    fGraphicBufferDtor(mBuffer);
    free(mBuffer);
  }
}

bool GraphicBufferSEC::EnsureInitialized() {
  if (ValidateFunctions())
    return true;

  if (fGraphicBufferCtorP0 ||
      fGraphicBufferCtorP4 ||
      fGraphicBufferDtor ||
      fGraphicBufferInitCheck ||
      fGraphicBufferLock ||
      fGraphicBufferUnlock ||
      fGraphicBufferGetFlattenedSize ||
      fGraphicBufferGetFdCount ||
      fGraphicBufferFlatten ||
      fGraphicBufferUnflatten ||
      fGraphicBufferGetNativeBuffer) {
      return false;
    }

  void *handle = dlopen(ANDROID_LIBUI_LIBRARY, RTLD_LAZY);
  if (!handle) {
    LOG(INFO)<<"Can not load ui library.";
    return false;
  }

  fGraphicBufferCtorP0 = (pfnGraphicBufferCtorP0)dlsym(handle,
      "_ZN7android13GraphicBufferC1Ev");
  fGraphicBufferCtorP4 = (pfnGraphicBufferCtorP4)dlsym(handle,
      "_ZN7android13GraphicBufferC1Ejjij");
  fGraphicBufferDtor = (pfnGraphicBufferDtor)dlsym(handle,
      "_ZN7android13GraphicBufferD1Ev");
  fGraphicBufferInitCheck = (pfnGraphicBufferInitCheck)dlsym(handle,
      "_ZNK7android13GraphicBuffer9initCheckEv");
  fGraphicBufferLock = (pfnGraphicBufferLock)dlsym(handle,
      "_ZN7android13GraphicBuffer4lockEjPPv");
  fGraphicBufferUnlock = (pfnGraphicBufferUnlock)dlsym(handle,
      "_ZN7android13GraphicBuffer6unlockEv");
  fGraphicBufferGetFlattenedSize = (pfnGraphicBufferGetFlattenedSize)dlsym(handle,
      "_ZNK7android13GraphicBuffer16getFlattenedSizeEv");
  fGraphicBufferGetFdCount = (pfnGraphicBufferGetFdCount)dlsym(handle,
      "_ZNK7android13GraphicBuffer10getFdCountEv");
  fGraphicBufferFlatten = (pfnGraphicBufferFlatten)dlsym(handle,
      "_ZNK7android13GraphicBuffer7flattenERPvRjRPiS3_");
  fGraphicBufferUnflatten = (pfnGraphicBufferUnflatten)dlsym(handle,
      "_ZN7android13GraphicBuffer9unflattenERPKvRjRPKiS4_");
  fGraphicBufferGetNativeBuffer = (pfnGraphicBufferGetNativeBuffer)dlsym(handle,
      "_ZNK7android13GraphicBuffer15getNativeBufferEv");

   if (ValidateFunctions())
    return true;

  return false;
}

bool GraphicBufferSEC::ValidateFunctions() {
  if (fGraphicBufferCtorP0 &&
      fGraphicBufferCtorP4 &&
      fGraphicBufferDtor &&
      fGraphicBufferInitCheck &&
      fGraphicBufferLock &&
      fGraphicBufferUnlock &&
      fGraphicBufferGetFlattenedSize &&
      fGraphicBufferGetFdCount &&
      fGraphicBufferFlatten &&
      fGraphicBufferUnflatten &&
      fGraphicBufferGetNativeBuffer) {
      return true;
    }

  return false;
}

status_t GraphicBufferSEC::initCheck() {
  DCHECK(fGraphicBufferInitCheck);
  return fGraphicBufferInitCheck(mBuffer);
}

int GraphicBufferSEC::Lock(uint32_t usage, void** vaddr) {
  DCHECK(fGraphicBufferLock);
  return fGraphicBufferLock(mBuffer, usage, vaddr);
}

int GraphicBufferSEC::Unlock() {
  DCHECK(fGraphicBufferUnlock);
  return fGraphicBufferUnlock(mBuffer);
}

void* GraphicBufferSEC::GetNativeBuffer() {
  DCHECK(fGraphicBufferGetNativeBuffer);
  return fGraphicBufferGetNativeBuffer(mBuffer);
}

size_t GraphicBufferSEC::GetFlattenedSize() {
  DCHECK(fGraphicBufferGetFlattenedSize);
  return fGraphicBufferGetFlattenedSize(mBuffer);
}

size_t GraphicBufferSEC::GetFdCount() {
  DCHECK(fGraphicBufferGetFdCount);
  return fGraphicBufferGetFdCount(mBuffer);
}

status_t GraphicBufferSEC::Flatten(void*& buffer,
    size_t& size,
    int*& fds,
    size_t& count) {
  DCHECK(fGraphicBufferFlatten);
  return fGraphicBufferFlatten(mBuffer, buffer, size, fds, count) == NO_ERROR;
}

status_t GraphicBufferSEC::Unflatten(void const*& buffer,
    size_t& size,
    int const*& fds,
    size_t& count) {
  DCHECK(fGraphicBufferUnflatten);
  return fGraphicBufferUnflatten(mBuffer, buffer, size, fds, count) == NO_ERROR;
}
} // namespace content
