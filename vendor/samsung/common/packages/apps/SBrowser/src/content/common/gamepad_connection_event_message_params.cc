// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gamepad_connection_event_message_params.h"

#include "base/logging.h"

namespace content {

namespace {

size_t StringLength(const blink::WebUChar* web_char_array, size_t array_size) {
  size_t i = 0;
  while (web_char_array[i] != 0) {
    ++i;
    DCHECK(i < array_size);
  }
  return i;
}

}

GamepadConnectionEventMessageParams::GamepadConnectionEventMessageParams()
    : index(-1),
      timestamp(0),
      axes_length(0),
      buttons_length(0),
      connected(false) {
}

GamepadConnectionEventMessageParams::~GamepadConnectionEventMessageParams() {
}

GamepadConnectionEventMessageParams::GamepadConnectionEventMessageParams(
    int index, const blink::WebGamepad& gamepad)
    : index(index),
      timestamp(gamepad.timestamp),
      axes_length(gamepad.axesLength),
      buttons_length(gamepad.buttonsLength),
      connected(gamepad.connected) {
  size_t length = StringLength(gamepad.id, blink::WebGamepad::idLengthCap);
  const blink::WebUChar* characters = &gamepad.id[0];
  id_characters.assign(characters, characters + length);

  length = StringLength(gamepad.mapping, blink::WebGamepad::mappingLengthCap);
  characters = &gamepad.mapping[0];
  mapping_characters.assign(characters, characters + length);
}

void GamepadConnectionEventMessageParams::GetWebGamepad(
    blink::WebGamepad* gamepad) const {
  DCHECK(index >= 0);
  DCHECK(id_characters.size() < (blink::WebGamepad::idLengthCap - 1));

  size_t num_bytes = id_characters.size() * sizeof(blink::WebUChar);
  memcpy(&gamepad->id[0], &id_characters[0], num_bytes);
  gamepad->id[id_characters.size()] = 0;

  DCHECK(mapping_characters.size() < (blink::WebGamepad::mappingLengthCap - 1));
  num_bytes = mapping_characters.size() * sizeof(blink::WebUChar);
  memcpy(&gamepad->mapping[0], &mapping_characters[0], num_bytes);
  gamepad->mapping[mapping_characters.size()] = 0;

  gamepad->timestamp = timestamp;
  gamepad->axesLength = axes_length;
  gamepad->buttonsLength = buttons_length;
  gamepad->connected = connected;
}

}
