/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * WEBRTC H264 wrapper interface
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_IMPL_H_

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"

namespace webrtc {

class H264EncoderImpl : public H264Encoder {

public:
  H264EncoderImpl() { }
  virtual int InitEncode(const VideoCodec* codec_settings,
            int number_of_cores,
            uint32_t max_payload_size);
  virtual int RegisterEncodeCompleteCallback(EncodedImageCallback* callback);
  virtual int SetPeriodicKeyFrames(bool enable);

  virtual int Encode(const I420VideoFrame& input_image,
            const CodecSpecificInfo* codec_specific_info,
            const std::vector<VideoFrameType>* frame_types);
  virtual int Release();
  virtual int SetChannelParameters(uint32_t packet_loss, int rtt);
  virtual int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_IMPL_H_
