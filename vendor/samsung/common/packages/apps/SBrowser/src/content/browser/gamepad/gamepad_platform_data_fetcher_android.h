// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_ANDROID_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_ANDROID_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/synchronization/lock.h"
#include "content/browser/gamepad/gamepad_data_fetcher.h"
#include "content/browser/gamepad/gamepad_standard_mappings.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

class GamepadPlatformDataFetcherAndroid : public GamepadDataFetcher {
 public:
  GamepadPlatformDataFetcherAndroid();
  virtual ~GamepadPlatformDataFetcherAndroid();

  // GamepadDataFetcher implementation.
  virtual void GetGamepadData(blink::WebGamepads* pads,
                              bool devices_changed_hint) OVERRIDE;
  virtual void PauseHint(bool paused) OVERRIDE;

  // Called by GamepadAdapter (java).
  void RefreshDevice(JNIEnv* env,
                     jobject obj,
                     int index,
                     bool connected,
                     jstring id,
                     jstring mapping,
                     long timestamp,
                     jfloatArray axes,
                     jfloatArray buttons);

  static bool RegisterGamepadAdapter(JNIEnv* env);

 private:
  // Data to fetch into.
  blink::WebGamepads* data_;

  DISALLOW_COPY_AND_ASSIGN(GamepadPlatformDataFetcherAndroid);
};

} // namespace content

#endif // CONTENT_BROWSER_GAMEPAD_GAMEPAD_DATA_FETCHER_ANDROID_H_
