// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_CONNECTION_EVENT_MESSAGE_PARAMS
#define CONTENT_COMMON_GAMEPAD_CONNECTION_EVENT_MESSAGE_PARAMS

#include <vector>

#include "third_party/WebKit/public/platform/WebGamepad.h"

namespace content {

struct GamepadConnectionEventMessageParams {
  GamepadConnectionEventMessageParams();
  GamepadConnectionEventMessageParams(int index,
                                      const blink::WebGamepad& gamepad);
  ~GamepadConnectionEventMessageParams();

  void GetWebGamepad(blink::WebGamepad* gamepad) const;

  std::vector<blink::WebUChar> id_characters;
  std::vector<blink::WebUChar> mapping_characters;
  int index;
  unsigned long long timestamp;
  int axes_length;
  int buttons_length;
  bool connected;
};

} // namespace content

#endif // CONTENT_COMMON_GAMEPAD_CONNECTION_EVENT_MESSAGE_PARAMS
