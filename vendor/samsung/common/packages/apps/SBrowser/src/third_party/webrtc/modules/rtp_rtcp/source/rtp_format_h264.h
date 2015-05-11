/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_

#if defined(ENABLE_WEBRTC_H264_CODEC)
#include <queue>
#include <vector>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/typedefs.h"

namespace webrtc {

#define FOUND_ANNEXB_HDR(data) (((data)[0] == 0x00 && (data)[1] == 0x00  && (data)[2] == 0x00 && (data)[3] == 0x01) ? 1 : 0)

#define SIZE_FRAME_RING_BUFFER 90
#define MAX_H264_CODEC_CONTEXT 4
#define MAX_CFG_FRAME_LEN 64
#define MAX_SPS_LEN 32
#define MAX_PPS_LEN 16

#define H264_NUM_FRAMES_IN_BUFFER 40
#define MAX_H264_BUFFER 7000
#define H264_MODIFIED_IFRAME_SIZE 5000
// H264MAXMTUSIZE reduced by (28+25)payloadOverload;
// earlier was set to 1200 bytes
#define H264MAXMTUSIZE 1147
#define H264_BASIC_HDR_LEN 1
#define H264_SKIP_START_CODE 4
#define H264_FU_HDR_LEN 2
#define H264_NRI_BIT_MASK 0x60
#define H264_TYPE_BIT_MASK 0x1F
#define H264_FUA_S_BIT_MASK 0x80
#define H264_FUA_E_BIT_MASK 0x40
#define NALU_HDR_LEN 4

#define ANNEXB_BUFF_LEN 4
#define RTP_HEADER_LEN 12

#define MAX_STAPA_BUFF_LEN 2000
#define NAL_STAPA_FORMAT 0x18

#define NAL_FU_FORMAT 28

#define NAL_TYPE_IFRAME 5
#define NAL_TYPE_PFRAME 1
#define NAL_TYPE_SPS 7
#define NAL_TYPE_PPS 8

typedef enum
{
  H264_CONTEXT_TYPE_NONE,
  H264_CONTEXT_TYPE_ENCODE,
  H264_CONTEXT_TYPE_DECODE
}H264CodecContextType;


/* Data structure for H264 Slice header */
typedef struct
{
  // Element for f-bit is not needed as it will be 0 when packet is sent
  uint8_t     nal_ref_id;       // NAL reference idc. 00 indicates not used to
  // reconstruct reference pictures for inter
  // picture prediction(2 bits).
  uint8_t     nal_unit_payload_type;    // nal_unit_type of 5 bits.
} H264SliceHeader;

typedef struct _H264_Encode_Context_Params
{
  bool         bSPS_PPS_Sent;
  bool         bFlagConfigFrameRead;
  uint32_t     H264ConfgFrameLen;
  uint32_t     H264SPSFrameLen;
  uint32_t     H264PPSFrameLen ;
  uint8_t      H264EncConfigFrame[MAX_CFG_FRAME_LEN];
  uint8_t      H264SPSframe[MAX_SPS_LEN];
  uint8_t      H264PPSframe[MAX_PPS_LEN];
}H264EncodeParams;

typedef struct _H264_Decode_Context_Params
{
  bool         DecConfigFrameFound;
  uint8_t      *pStapABuff;
  uint8_t      *pNewBuff;
  uint32_t     H264SPSFrameLen;
  uint32_t     H264PPSFrameLen ;
  uint8_t      H264SPSframe[MAX_SPS_LEN];
  uint8_t      H264PPSframe[MAX_PPS_LEN];
}H264DecodeParams;

typedef struct _H264CodecContext
{
  H264CodecContextType eType;
  uint8_t      iContextID;
  bool         bContextUsed;
  union
  {
    H264EncodeParams sEncode_Params;
    H264DecodeParams sDecode_Params;
  }u;
}H264CodecContext;


typedef struct {
 uint32_t     uiFrameNum[SIZE_FRAME_RING_BUFFER];
 uint32_t     uiTimeStamp[SIZE_FRAME_RING_BUFFER];
 uint32_t     uiFrameNumCounter;
 uint32_t     uicounter;
 int32_t      iPktSeqNum;
}RTPFrameInfo;


typedef struct {
  bool                frm_isvalid;
  uint8_t             frm_marker;
  uint8_t             evrc_format;
  uint8_t             redundancyLevel;  // Redundancy: Redundancy level identifier
  uint32_t            frm_len;
  uint32_t            frm_ts;
  const uint8_t      *frm_data;
}MediaFrameInfo;


class RtpFormatH264 {
 public:
  RtpFormatH264();
  ~RtpFormatH264();

  void ExtractNALUFromEncFrame(uint8_t* ip_frame,
                  uint32_t ip_frame_size,
                  uint16_t* op_nalu_size);

  uint16_t ParseSingleNALUSliceHeader(
                  H264SliceHeader* slice_hdr_info,
                  uint8_t *buf);

  uint16_t BuildSingleNALUFUPayloadHeader(
                  H264SliceHeader* slice_hdr_info,
                  uint8_t* buf,
                  bool bool_startbit,
                  bool bool_endbit);
 private:
};

}  // namespace webrtc
#endif  //ENABLE_WEBRTC_H264_CODEC
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H264_H_
