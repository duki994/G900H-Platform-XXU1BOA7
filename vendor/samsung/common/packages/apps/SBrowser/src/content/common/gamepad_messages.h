// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "base/memory/shared_memory.h"
#include "content/common/gamepad_connection_event_message_params.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebGamepad.h"

#define IPC_MESSAGE_START GamepadMsgStart

IPC_STRUCT_TRAITS_BEGIN(blink::WebGamepadButton)
  IPC_STRUCT_TRAITS_MEMBER(pressed)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::GamepadConnectionEventMessageParams)
  IPC_STRUCT_TRAITS_MEMBER(id_characters)
  IPC_STRUCT_TRAITS_MEMBER(mapping_characters)
  IPC_STRUCT_TRAITS_MEMBER(index)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(axes_length)
  IPC_STRUCT_TRAITS_MEMBER(buttons_length)
  IPC_STRUCT_TRAITS_MEMBER(connected)
IPC_STRUCT_TRAITS_END()

IPC_MESSAGE_CONTROL1(GamepadMsg_GamepadConnected,
                     content::GamepadConnectionEventMessageParams)

IPC_MESSAGE_CONTROL1(GamepadMsg_GamepadDisconnected,
                     content::GamepadConnectionEventMessageParams)

// Messages sent from the renderer to the browser.

// Asks the browser process to start polling, and return a shared memory
// handles that will hold the data from the hardware. See
// gamepad_hardware_buffer.h for a description of how synchronization is
// handled. The number of Starts should match the number of Stops.
IPC_SYNC_MESSAGE_CONTROL0_1(GamepadHostMsg_StartPolling,
                            base::SharedMemoryHandle /* handle */)

IPC_SYNC_MESSAGE_CONTROL0_0(GamepadHostMsg_StopPolling)
