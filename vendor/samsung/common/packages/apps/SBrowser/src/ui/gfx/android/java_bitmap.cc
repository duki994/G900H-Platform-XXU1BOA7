// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/java_bitmap.h"

#include <android/bitmap.h>

#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/BitmapHelper_jni.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

using base::android::AttachCurrentThread;

namespace gfx {

JavaBitmap::JavaBitmap(jobject bitmap)
    : bitmap_(bitmap),
      pixels_(NULL) {
  int err = AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_, &pixels_);
  DCHECK(!err);
  DCHECK(pixels_);

  AndroidBitmapInfo info;
  err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_, &info);
  DCHECK(!err);
  size_ = gfx::Size(info.width, info.height);
  format_ = info.format;
  stride_ = info.stride;
}

JavaBitmap::~JavaBitmap() {
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_);
  DCHECK(!err);
}

// static
bool JavaBitmap::RegisterJavaBitmap(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ScopedJavaLocalRef<jobject> CreateJavaBitmap(int width, int height,
                                                    bool is565_config) {
  return Java_BitmapHelper_createBitmap(AttachCurrentThread(),
      width, height, is565_config);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap* skbitmap) {
  DCHECK(skbitmap);
  SkBitmap::Config config = skbitmap->getConfig();
  DCHECK((config == SkBitmap::kRGB_565_Config) ||
         (config == SkBitmap::kARGB_8888_Config));
  // If the Config is not RGB565 it is default i.e ARGB8888
  ScopedJavaLocalRef<jobject> jbitmap =
      CreateJavaBitmap(skbitmap->width(), skbitmap->height(),
                       (config == SkBitmap::kRGB_565_Config));
  SkAutoLockPixels src_lock(*skbitmap);
  JavaBitmap dst_lock(jbitmap.obj());
  void* src_pixels = skbitmap->getPixels();
  void* dst_pixels = dst_lock.pixels();
  memcpy(dst_pixels, src_pixels, skbitmap->getSize());

  return jbitmap;
}

SkBitmap CreateSkBitmapFromJavaBitmap(JavaBitmap& jbitmap) {
  DCHECK_EQ(jbitmap.format(), ANDROID_BITMAP_FORMAT_RGBA_8888);

  gfx::Size src_size = jbitmap.size();

  SkBitmap skbitmap;
  skbitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     src_size.width(),
                     src_size.height(),
                     jbitmap.stride());
  if (!skbitmap.allocPixels()) {
    LOG(FATAL) << " Failed to allocate bitmap of size " << src_size.width()
               << "x" << src_size.height() << " stride=" << jbitmap.stride();
  }
  SkAutoLockPixels dst_lock(skbitmap);
  void* src_pixels = jbitmap.pixels();
  void* dst_pixels = skbitmap.getPixels();
  CHECK(src_pixels);

  memcpy(dst_pixels, src_pixels, skbitmap.getSize());

  return skbitmap;
}

SkBitmap CreateSkBitmapFromResource(const char* name, gfx::Size size) {
  DCHECK(!size.IsEmpty());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jname(env, env->NewStringUTF(name));
  ScopedJavaLocalRef<jobject> jobj(Java_BitmapHelper_decodeDrawableResource(
      env, jname.obj(), size.width(), size.height()));
  if (jobj.is_null())
    return SkBitmap();

  JavaBitmap jbitmap(jobj.obj());
  SkBitmap bitmap = CreateSkBitmapFromJavaBitmap(jbitmap);
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BOX, size.width(), size.height());
}

}  //  namespace gfx
