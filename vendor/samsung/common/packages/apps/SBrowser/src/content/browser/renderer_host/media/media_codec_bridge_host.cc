// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_codec_bridge_host.h"

#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/common/media/media_codec_bridge_message.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_codec_bridge.h"

namespace content {

MediaCodecBridgeHost::MediaCodecBridgeHost() {
}

MediaCodecBridgeHost::~MediaCodecBridgeHost() {
}

void MediaCodecBridgeHost::OnChannelClosing() {
}

void MediaCodecBridgeHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

///////////////////////////////////////////////////////////////////////////////
// IPC Messages handler.
bool MediaCodecBridgeHost::OnMessageReceived(const IPC::Message& message,
                                         bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MediaCodecBridgeHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
      MediaCodecBridgeHostMsg_GetSupportedDecoderProfiles,
      OnGetSupportedDecoderProfiles)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
      MediaCodecBridgeHostMsg_GetSupportedEncoderProfiles,
      OnGetSupportedEncoderProfiles)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void MediaCodecBridgeHost::OnGetSupportedDecoderProfiles(
    IPC::Message* reply_msg) {
  // Query (statically) for our supported codecs.
  std::vector<media::VideoDecodeAccelerator::SupportedProfile> profiles =
    GpuVideoDecodeAcceleratorHost::GetSupportedProfiles();
MediaCodecBridgeHostMsg_GetSupportedDecoderProfiles::WriteReplyParams(
                                                           reply_msg, profiles);
  Send(reply_msg);
}

void MediaCodecBridgeHost::OnGetSupportedEncoderProfiles(
    IPC::Message* reply_msg) {
  // Query (statically) for our supported codecs.
  std::vector<media::VideoEncodeAccelerator::SupportedProfile> profiles =
    GpuVideoEncodeAcceleratorHost::GetSupportedProfiles();
MediaCodecBridgeHostMsg_GetSupportedEncoderProfiles::WriteReplyParams(
                                                           reply_msg, profiles);
  Send(reply_msg);
}

} // namespace content
