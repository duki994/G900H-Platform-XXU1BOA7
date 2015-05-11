// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(SBROWSER_GRAPHICS_GETBITMAP)
#include <android/native_window_jni.h>
#include "android/native_window.h"
#include <android/bitmap.h>
#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "sbrowser/content/native/browser/android/sbr/surface_callback.h"

using base::WaitableEvent;

namespace content {

namespace {

struct GlobalState {
  base::Lock registration_lock;
  // We hold a reference to a message loop proxy which handles message loop
  // destruction gracefully, which is important since we post tasks from an
  // arbitrary binder thread while the main thread might be shutting down.
  // Also, in single-process mode we have two ChildThread objects for render
  // and gpu thread, so we need to store the msg loops separately.
  scoped_refptr<base::MessageLoopProxy> native_window_loop;
  NativeGetBitmapCallback get_bitmap_callback;
};

base::LazyInstance<GlobalState>::Leaky g_state = LAZY_INSTANCE_INITIALIZER;

 void RunNativeGetBitmapCallback(BitmapParams imageParams, int* ret,
    int32 routing_id,
    int32 renderer_id,
    void** buffer,
    WaitableEvent* completion) {
  g_state.Pointer()->get_bitmap_callback.Run(
     imageParams, ret, routing_id, renderer_id, buffer, completion);
}

}  // namespace <anonymous>

enum BitmapFormat
{
	BITMAP_FORMAT_ALPHA_8 = 0,
	BITMAP_FORMAT_RGB_565 = 1,
	BITMAP_FORMAT_ARGB_4444 = 2,
	BITMAP_FORMAT_ARGB_8888 = 3,
};

int SetBitmapAsync(JNIEnv* env, int x, int y, int width,int height,
    jobject jbitmap, int imageFormat, int primary_id, int secondary_id,
    base::WaitableEvent* completion)
{
	base::AutoLock lock(g_state.Pointer()->registration_lock);
	void *jbitmapBuffer = NULL,*argb8888Buffer = NULL, *glreadPassBuffer = NULL;
	SkBitmap sk_argb_bitmap,sk_rgb565_bitmap;
	int ret = 0;
	int bitmapFormat = 0;
	if((ret = AndroidBitmap_lockPixels(env,jbitmap,&jbitmapBuffer)) < 0)
		return ret;
	bitmapFormat = imageFormat;
	if(bitmapFormat == BITMAP_FORMAT_RGB_565) {
		sk_argb_bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
		sk_argb_bitmap.allocPixels();
		argb8888Buffer = sk_argb_bitmap.getPixels();
		bitmapFormat = BITMAP_FORMAT_ARGB_8888;
		glreadPassBuffer = argb8888Buffer;
	} else if(imageFormat == BITMAP_FORMAT_ARGB_8888) {
		bitmapFormat = BITMAP_FORMAT_ARGB_8888;
		glreadPassBuffer = jbitmapBuffer;
	} else {
		bitmapFormat = BITMAP_FORMAT_ARGB_8888;
		glreadPassBuffer = jbitmapBuffer;
	}
	GlobalState* const global_state = g_state.Pointer();
	// This should only be sent as a reaction to the renderer
	// activating compositing. If the GPU process crashes, we expect this
	// to be resent after the new thread is initialized.
	DCHECK(global_state->native_window_loop);
	if (global_state->native_window_loop)
	{
		//sometimes native_window_loop is NULL
		global_state->native_window_loop->PostTask(FROM_HERE,
      base::Bind(&RunNativeGetBitmapCallback,
      BitmapParams(x, y, width, height, bitmapFormat),
      &ret,
      primary_id,
      static_cast<uint32_t>(secondary_id),
      (void **)&glreadPassBuffer,
      completion));
      completion->Wait();
	}

	if(imageFormat == BITMAP_FORMAT_RGB_565) {
		sk_rgb565_bitmap.setConfig(SkBitmap::kRGB_565_Config, width, height, 0);
		sk_rgb565_bitmap.setPixels(jbitmapBuffer);
		SkCanvas canvas(sk_rgb565_bitmap);
		SkRect rect;
		rect.setXYWH(SkIntToScalar(0),SkIntToScalar(0),SkIntToScalar(width),
        SkIntToScalar(height));
		canvas.drawBitmapRectToRect(sk_argb_bitmap, &rect, rect, NULL);
	}
	AndroidBitmap_unlockPixels(env,jbitmap);
	return ret;
}

void RegisterNativeGetBitmapCallback(base::MessageLoopProxy* loop,
                                  NativeGetBitmapCallback& callback)
{
  GlobalState* const global_state = g_state.Pointer();
  base::AutoLock lock(global_state->registration_lock);
  global_state->native_window_loop = loop;
  global_state->get_bitmap_callback = callback;
}
}  // namespace content
#endif//SBROWSER_GRAPHICS_GETBITMAP

