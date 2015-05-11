// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "content/common/gamepad_messages.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace blink { class WebGamepadListener; }

namespace content {

struct GamepadHardwareBuffer;

class GamepadSharedMemoryReader : public IPC::ChannelProxy::MessageFilter {
 public:
  GamepadSharedMemoryReader(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);
  void SampleGamepads(blink::WebGamepads&);
  void SetGamepadListener(blink::WebGamepadListener* listener);

 protected:
  virtual ~GamepadSharedMemoryReader();

 private:
  void StartPollingIfNecessary();
  void StopPollingIfNecessary();

  // IPC::ChannelProxy::MessageFilter override. Called on |io_message_loop|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Called when a gamepad is connected to the system
  void OnGamepadConnected(const GamepadConnectionEventMessageParams& params);

  // Called when a gamepad is disconnected from the system
  void OnGamepadDisconnected(const GamepadConnectionEventMessageParams& params);

  void DispatchGamepadConnected(int index, const blink::WebGamepad& gamepad);
  void DispatchGamepadDisconnected(int index, const blink::WebGamepad& gamepad);

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Main thread's message loop.
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  base::SharedMemoryHandle renderer_shared_memory_handle_;
  scoped_ptr<base::SharedMemory> renderer_shared_memory_;
  GamepadHardwareBuffer* gamepad_hardware_buffer_;
  blink::WebGamepadListener* gamepad_listener_;

  bool is_polling_;
  bool ever_interacted_with_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
