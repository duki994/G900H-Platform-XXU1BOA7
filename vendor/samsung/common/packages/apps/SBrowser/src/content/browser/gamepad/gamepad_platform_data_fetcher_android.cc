// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_platform_data_fetcher_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "base/strings/string16.h"
#include "jni/GamepadAdapter_jni.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

using base::android::AttachCurrentThread;
using base::string16;
using blink::WebGamepad;
using blink::WebGamepads;

namespace content {

namespace {

template <size_t array_length>
void CopyJavaStringToWebUCharArray(
    JNIEnv* env, jstring src, blink::WebUChar* array) {
  COMPILE_ASSERT(sizeof(string16::value_type) == sizeof(blink::WebUChar),
                 string16_and_WebUChar_are_same_size);
  string16 data;
  base::android::ConvertJavaStringToUTF16(env, src, &data);
  COMPILE_ASSERT(array_length > 0, array_length_at_least_1);
  const size_t characters_to_copy = std::min(data.size(), array_length - 1);
  memcpy(array, data.data(), characters_to_copy * sizeof(string16::value_type));
  array[characters_to_copy] = 0;
}

}

GamepadPlatformDataFetcherAndroid::GamepadPlatformDataFetcherAndroid() {
  Java_GamepadAdapter_setDataRequested(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this), true);
}

GamepadPlatformDataFetcherAndroid::~GamepadPlatformDataFetcherAndroid() {
}

void GamepadPlatformDataFetcherAndroid::GetGamepadData(
    blink::WebGamepads* pads,
    bool devices_changed_hint) {
  TRACE_EVENT0("GAMEPAD", "GetGamepadData");
  data_ = pads;
  data_->length = WebGamepads::itemsLengthCap;
  Java_GamepadAdapter_getGamepadData(AttachCurrentThread());
  data_ = NULL;
}

void GamepadPlatformDataFetcherAndroid::PauseHint(bool paused) {
  Java_GamepadAdapter_setDataRequested(
        AttachCurrentThread(), reinterpret_cast<intptr_t>(this), !paused);
}

void GamepadPlatformDataFetcherAndroid::RefreshDevice(
    JNIEnv* env,
    jobject obj,
    int index,
    bool connected,
    jstring id,
    jstring mapping,
    long timestamp,
    jfloatArray axes,
    jfloatArray buttons) {
  CHECK(data_);
  WebGamepad& pad = data_->items[index];
  pad.connected = connected;
  if (!connected)
    return;

  CopyJavaStringToWebUCharArray<WebGamepad::idLengthCap>(env, id, pad.id);
  CopyJavaStringToWebUCharArray<WebGamepad::mappingLengthCap>(
      env, mapping, pad.mapping);

  pad.timestamp = timestamp;

  std::vector<float> axes_data;
  base::android::JavaFloatArrayToFloatVector(env, axes, &axes_data);
  pad.axesLength = std::min(axes_data.size(), WebGamepad::axesLengthCap);
  memcpy(pad.axes, axes_data.begin(), pad.axesLength * sizeof(float));

  std::vector<float> buttons_data;
  base::android::JavaFloatArrayToFloatVector(env, buttons, &buttons_data);
  pad.buttonsLength =
      std::min(buttons_data.size(), WebGamepad::buttonsLengthCap);
  for (unsigned i = 0; i < pad.buttonsLength; ++i) {
    float value = buttons_data[i];
    pad.buttons[i].pressed = value;
    pad.buttons[i].value = value;
  }
}

bool GamepadPlatformDataFetcherAndroid::RegisterGamepadAdapter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}
