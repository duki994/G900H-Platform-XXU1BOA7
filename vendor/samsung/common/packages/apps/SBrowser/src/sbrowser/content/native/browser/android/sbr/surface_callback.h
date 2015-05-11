// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(SBROWSER_GRAPHICS_GETBITMAP)
#ifndef CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_
#define CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_

#include <jni.h>
#include "base/callback.h"

struct ANativeWindow;

namespace base {
    class MessageLoopProxy;
    class WaitableEvent;
}
struct BitmapParams {
    int xPos;
    int yPos;
    int width;
    int height;
    int imageFormat;

    BitmapParams(int x, int y, int widthValue, int heightValue,
        int bitmapFormat)
        : xPos(x)
        , yPos(y)
        , width(widthValue)
        , height(heightValue)
        , imageFormat(bitmapFormat)
        {}
};
namespace content {

// This file implements support for passing surface handles from Java
// to the correct thread on the native side. On Android, these surface
// handles can only be passed across processes through Java IPC (Binder),
// which means calls from Java come in on arbitrary threads. Hence the
// static nature and the need for the client to register a callback with
// the corresponding message loop.

// Asynchronously sets the Surface. This can be called from any thread.
// The Surface will be set to the proper thread based on the type. The
// nature of primary_id and secondary_id depend on the type of surface
// and are used to route the surface to the correct client.
// This method will call release() on the jsurface object to release
// all the resources. So after calling this method, the caller should
// not use the jsurface object again.

int SetBitmapAsync(JNIEnv* env, int x, int y, int width, int height,
                     jobject jbitmap,
                     int imageFormat,
                     int primary_id,
                     int secondary_id,
                     base::WaitableEvent* completion);

typedef base::Callback<void(
    BitmapParams,int*, int, int, void**, base::WaitableEvent*)> NativeGetBitmapCallback;
void RegisterNativeGetBitmapCallback(base::MessageLoopProxy* loop,
                                  NativeGetBitmapCallback& callback);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_CALLBACK_H_
#endif //SBROWSER_GRAPHICS_GETBITMAP

