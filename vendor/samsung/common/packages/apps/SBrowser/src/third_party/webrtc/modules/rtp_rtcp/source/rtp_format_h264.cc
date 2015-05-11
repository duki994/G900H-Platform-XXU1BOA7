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
#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"

#include <assert.h>   // assert
#include <string.h>  // memcpy

#include <vector>

namespace webrtc {

RtpFormatH264::RtpFormatH264(){ }
RtpFormatH264::~RtpFormatH264(){ }

void RtpFormatH264::ExtractNALUFromEncFrame(uint8_t *ip_frame,
                uint32_t ip_frame_size,
                uint16_t *op_nalu_size)
{
  unsigned int i = 0;
  *op_nalu_size = ip_frame_size;
  for (i = 0; i < ip_frame_size - ANNEXB_BUFF_LEN; i++) {
    if (FOUND_ANNEXB_HDR(ip_frame+i)) {
      *op_nalu_size = i;
      break;
    }
  }
}

uint16_t RtpFormatH264::ParseSingleNALUSliceHeader (
                H264SliceHeader *slice_hdr_info,
                uint8_t *buf)
{
  memset (slice_hdr_info, 0, sizeof (H264SliceHeader));

  // Get the various frame header field values
  //  |0 |1 |2 |3 |4 |5 |6 |7 |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+
  //  |F | NRI |    Type      |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+
  // Start of H264 NAL unit (SPS/PPS,IFRAME,PFRAME):
  // [00] [00] [00] [01] [67/65/21]
  // Get the various frame header field values
  // TODO - Praveen : correctly determine NRI bit
  slice_hdr_info->nal_ref_id = ((buf[0] & H264_NRI_BIT_MASK) >> 5);
  slice_hdr_info->nal_unit_payload_type = (buf[0] & H264_TYPE_BIT_MASK);

  return 0;
}

uint16_t RtpFormatH264::BuildSingleNALUFUPayloadHeader (
                H264SliceHeader * slice_hdr_info,
                uint8_t * buf, bool bool_startbit, bool bool_endbit)
{
  uint16_t ui16_retval = H264_FU_HDR_LEN;
  // Fill FU indicator and FU header field values - RFC6184
  //  |0 |1 |2 |3 |4 |5 |6 |7 |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+
  //  |F | NRI |    Type      |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+
  //  |s |e |r |    Type      |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+
  // Here the two bytes of header of FU-A packet will be set
  // Slice header pointer is NULL OR buf is NULL
  if ((slice_hdr_info == NULL) || (buf == NULL)) {
    return 0;
  }
  memset(buf, 0, H264_FU_HDR_LEN * sizeof(uint8_t));
  // Set the first byte of header with NRI same as previous and TYPE = 28
  // for FU-A as F bit would be 0 always and 5 is the length of TYPE field in
  // NAL header
  buf[0] = (uint8_t) ((slice_hdr_info->nal_ref_id << 5) | 0x1C);

  // Set the second byte of header with corresponding start(S) bit, end(E) bit,
  // R=0 and Type field
  if ((bool_startbit == true) && (bool_endbit == false)) {
    buf[1] = 0x80 | (uint8_t)(slice_hdr_info->nal_unit_payload_type);
  }
  else if ((bool_startbit == false) && (bool_endbit == false)) {
    buf[1] = 0x00 | (uint8_t)(slice_hdr_info->nal_unit_payload_type);
  }
  else if ((bool_startbit == false) && (bool_endbit == true)) {
    buf[1] = 0x40 | (uint8_t)(slice_hdr_info->nal_unit_payload_type);
  }
  return ui16_retval;
}

}  // namespace webrtc
#endif // ENABLE_WEBRTC_H264_CODEC
