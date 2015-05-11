/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * This file contains the WEBRTC H264 wrapper implementation
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_impl.h"

namespace webrtc {

H264Encoder* H264Encoder::Create() {
  return new H264EncoderImpl();
}

int H264EncoderImpl::InitEncode(const VideoCodec* codec_settings,
                    int number_of_cores,
                    uint32_t max_payload_size) {
  return 1;
}

int H264EncoderImpl::RegisterEncodeCompleteCallback(EncodedImageCallback* callback) {
  return 1;
}

int H264EncoderImpl::SetPeriodicKeyFrames(bool enable) {
  return 1;
}

int H264EncoderImpl::Encode(const I420VideoFrame& input_image,
                    const CodecSpecificInfo* codec_specific_info,
                    const std::vector<VideoFrameType>* frame_types) {
  return 1;
}

int H264EncoderImpl::Release() {
  return 1;
}

int H264EncoderImpl::SetChannelParameters(uint32_t packet_loss, int rtt) {
  return 1;
}

int H264EncoderImpl::SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate) {
  return 1;
}

}  // namespace webrtc
