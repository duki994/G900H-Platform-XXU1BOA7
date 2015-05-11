/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_H264_SPS_PPS_PARSER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_H264_SPS_PPS_PARSER_H_

#if defined(ENABLE_WEBRTC_H264_CODEC)

#include "webrtc/typedefs.h"

#include <string>

namespace webrtc {

typedef struct {
  uint16_t width;
  uint16_t height;
  uint16_t profile_idc;
  uint16_t level_idc;
} H264_SpsInfo;

class RtpH264SpsPpsParser {
 public:
  RtpH264SpsPpsParser();
  ~RtpH264SpsPpsParser();

  int32_t SpsPpsReadBit(const unsigned char* naluData);
  int32_t SpsPpsReadBits(const unsigned char* naluData, int n);
  int32_t SpsPpsReadExpGolomb(const unsigned char* naluData);
  int32_t SpsPpsReadSE(const unsigned char* naluData);
  void SpsPpsScalingList(unsigned int i, unsigned int sizeOfScalingList, const unsigned char* naluData);
  int32_t ReadGolombSE(bool isItSigned, const unsigned char* naluData);
  void ParseSpsPps(const unsigned char* naluData, H264_SpsInfo* spsInfo);
  std::string DecodeBase64(std::string& encodedString);

 private:
    int m_nCurrentBit;
    unsigned char* m_binaryData;
};

}  // namespace webrtc
#endif  // ENABLE_WEBRTC_H264_CODEC
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_H264_SPS_PPS_PARSER_H_
