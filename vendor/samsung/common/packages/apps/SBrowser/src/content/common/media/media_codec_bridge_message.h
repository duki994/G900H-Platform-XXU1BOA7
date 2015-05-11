// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_export.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/common/gpu/client/gpu_video_decode_accelerator_host.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "content/browser/renderer_host/media/media_codec_bridge_host.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaCodecBridgeHostMsgStart

IPC_STRUCT_TRAITS_BEGIN(media::VideoDecodeAccelerator::SupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::VideoDecodeAccelerator::SupportedProfile::MaxFrameRate)
  IPC_STRUCT_TRAITS_MEMBER(numerator)
  IPC_STRUCT_TRAITS_MEMBER(denominator)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::VideoEncodeAccelerator::SupportedProfile)
  IPC_STRUCT_TRAITS_MEMBER(profile)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(max_framerate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::VideoEncodeAccelerator::SupportedProfile::MaxFrameRate)
  IPC_STRUCT_TRAITS_MEMBER(numerator)
  IPC_STRUCT_TRAITS_MEMBER(denominator)
IPC_STRUCT_TRAITS_END()

IPC_SYNC_MESSAGE_CONTROL0_1(MediaCodecBridgeHostMsg_GetSupportedDecoderProfiles,
	std::vector<media::VideoDecodeAccelerator::SupportedProfile> /* profiles */)

IPC_SYNC_MESSAGE_CONTROL0_1(MediaCodecBridgeHostMsg_GetSupportedEncoderProfiles,
	std::vector<media::VideoEncodeAccelerator::SupportedProfile> /* profiles */)