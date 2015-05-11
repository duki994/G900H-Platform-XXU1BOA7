/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_WEBRTC_H264_CODEC)
#include "webrtc/modules/rtp_rtcp/source/rtp_h264_sps_pps_parser.h"

namespace webrtc {

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static bool IsNum(unsigned char c) {
    return (c >= 0 || c <= 9) ? true : false;
}

static bool IsBase64(unsigned char c)
{
    return (IsNum(c) || (c == '+') || (c == '/'));
}

RtpH264SpsPpsParser::RtpH264SpsPpsParser()
    :m_nCurrentBit(0) {
}

RtpH264SpsPpsParser::~RtpH264SpsPpsParser(){ }

int32_t RtpH264SpsPpsParser::SpsPpsReadBit(const unsigned char* naluData) {
  int nIndex = m_nCurrentBit / 8;
  int nOffset = m_nCurrentBit % 8 + 1;

  m_nCurrentBit ++;
  return (naluData[nIndex] >> (8-nOffset)) & 0x01;
}

int32_t RtpH264SpsPpsParser::SpsPpsReadBits(const unsigned char* naluData,
    int n) {
  int r = 0;

  for (int i = 0; i < n; i++)
    r |= (SpsPpsReadBit(naluData) << ( n - i - 1 ) );

  return r;
}

int32_t RtpH264SpsPpsParser::SpsPpsReadExpGolomb(
    const unsigned char* naluData) {
  int r = 0;
  int i = 0;

  while ((SpsPpsReadBit(naluData) == 0) && (i < 32))
    i++;

  r = SpsPpsReadBits(naluData, i);
  r += (1 << i) - 1;

  return r;
}

int32_t RtpH264SpsPpsParser::SpsPpsReadSE(const unsigned char* naluData) {
  int r = SpsPpsReadExpGolomb(naluData);

  if (r & 0x01)
    r = (r+1)/2;
  else
    r = -(r/2);

  return r;
}

void RtpH264SpsPpsParser::SpsPpsScalingList(unsigned int ix,
    unsigned int sizeOfScalingList, const unsigned char* naluData) {
  unsigned int lastScale = 8;
  unsigned int nextScale = 8;
  int deltaScale;

  for (unsigned int jx = 0; jx < sizeOfScalingList; jx++) {
    if (nextScale != 0) {
      deltaScale = SpsPpsReadSE(naluData);
      nextScale = (lastScale + deltaScale + 256) % 256;
    }
    if (nextScale == 0)
      lastScale = lastScale;
    else
      lastScale = nextScale;
  }
}

void RtpH264SpsPpsParser::ParseSpsPps(const unsigned char* naluData,
    H264_SpsInfo* spsInfo) {
  SpsPpsReadBits(naluData, 8); // Skip nal unit type
  spsInfo->profile_idc = SpsPpsReadBits(naluData, 8);
  SpsPpsReadBits(naluData, 8);
  spsInfo->level_idc = SpsPpsReadBits(naluData, 8);
  SpsPpsReadExpGolomb(naluData);

  if (spsInfo->profile_idc == 100 || spsInfo->profile_idc == 110 ||
            spsInfo->profile_idc == 122 || spsInfo->profile_idc == 144) {
    int chroma_format_idc = SpsPpsReadExpGolomb(naluData);
    if (chroma_format_idc == 3)
        SpsPpsReadBit(naluData);
    SpsPpsReadExpGolomb(naluData);
    SpsPpsReadExpGolomb(naluData);
    SpsPpsReadBit(naluData);
    int seq_scaling_matrix_present_flag = SpsPpsReadBit(naluData);
    if (seq_scaling_matrix_present_flag) {
      for (int i = 0; i < 8; i++) {
        int seq_scaling_list_present_flag = SpsPpsReadBit(naluData);
        if (seq_scaling_list_present_flag)
          SpsPpsScalingList(i, i < 6 ? 16 : 64, naluData);
      }
    }
  }
  SpsPpsReadExpGolomb(naluData);
  int pic_order_cnt_type = SpsPpsReadExpGolomb(naluData);
  if (pic_order_cnt_type == 0)
    SpsPpsReadExpGolomb(naluData);
  else if (pic_order_cnt_type == 1) {
    SpsPpsReadBit(naluData);
    SpsPpsReadSE(naluData);
    SpsPpsReadSE(naluData);
    int num_ref_frames_in_pic_order_cnt_cycle = SpsPpsReadExpGolomb(naluData);
    for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )
      SpsPpsReadSE(naluData);
  }
  SpsPpsReadExpGolomb(naluData);
  SpsPpsReadBit(naluData);
  int pic_width_in_mbs_minus1 = SpsPpsReadExpGolomb(naluData);
  int pic_height_in_map_units_minus1 = SpsPpsReadExpGolomb(naluData);
  spsInfo->width = (pic_width_in_mbs_minus1 + 1) * 16;
  spsInfo->height = (pic_height_in_map_units_minus1 + 1) * 16;
}

std::string RtpH264SpsPpsParser::DecodeBase64(std::string& encodedString)
{
  int inlen = encodedString.size();
  int i = 0;
  int j = 0;
  int in = 0;

  unsigned char charArray4[4], charArray3[3];
  std::string ret;

  while (inlen-- && ( encodedString[in] != '=') &&
                    IsBase64(encodedString[in])) {
    charArray4[i++] = encodedString[in]; in++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        charArray4[i] = base64_chars.find(charArray4[i]);

      charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
      charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
      charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

      for (i = 0; (i < 3); i++)
        ret += charArray3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      charArray4[j] = 0;

    for (j = 0; j <4; j++)
      charArray4[j] = base64_chars.find(charArray4[j]);

    charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
    charArray3[1] = ((charArray4[1] & 0xf) << 4) + ((charArray4[2] & 0x3c) >> 2);
    charArray3[2] = ((charArray4[2] & 0x3) << 6) + charArray4[3];

    for (j = 0; (j < i - 1); j++)
      ret += charArray3[j];
  }

  return ret;
}

}  // namespace webrtc
#endif // ENABLE_WEBRTC_H264_CODEC
