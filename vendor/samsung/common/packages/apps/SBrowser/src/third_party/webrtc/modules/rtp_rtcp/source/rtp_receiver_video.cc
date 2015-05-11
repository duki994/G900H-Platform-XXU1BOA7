/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_video.h"

#include <assert.h>
#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#if defined(ENABLE_WEBRTC_H264_CODEC)
#include "webrtc/modules/rtp_rtcp/source/rtp_h264_sps_pps_parser.h"
#endif
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

RTPReceiverStrategy* RTPReceiverStrategy::CreateVideoStrategy(
    int32_t id, RtpData* data_callback) {
  return new RTPReceiverVideo(id, data_callback);
}

RTPReceiverVideo::RTPReceiverVideo(int32_t id, RtpData* data_callback)
    : RTPReceiverStrategy(data_callback),
#if defined(ENABLE_WEBRTC_H264_CODEC)
      width_(480), // TODO: Fix hardcoded values.
      height_(640),
#endif
      id_(id) {}

RTPReceiverVideo::~RTPReceiverVideo() {
}

#if defined(ENABLE_WEBRTC_H264_CODEC)
void RTPReceiverVideo::RegisterH264FmtpParameters(
    const char* profileLevelId,
    int packetizationMode,
    const char* spropParameterSets) {

    profileLevelId_ = profileLevelId;
    packetizationMode_ = packetizationMode;
    spropParameterSets_ = spropParameterSets;

    std::string sps_pps(spropParameterSets);
    RtpH264SpsPpsParser spsPpsParser;
    H264_SpsInfo spsInfo;
    int sps_length = sps_pps.find(",");
    if (sps_length != -1) {
      //sps data
      char* sps_ptr = (char*)malloc(sps_length + 1);
      memset(sps_ptr, 0, sps_length + 1);
      sps_pps.copy(sps_ptr, sps_length, 0);
      sps_ptr[sps_length] = '\0'; // don't forget the terminating 0
      std::string sps(sps_ptr);
      sps_ = spsPpsParser.DecodeBase64(sps);
      free(sps_ptr);
      sps_ptr = NULL;

      //pps data
      char* pps_ptr = (char*)malloc(sps_pps.length() - sps_length);
      memset(pps_ptr, 0, (sps_pps.length() - sps_length));
      sps_pps.copy(pps_ptr, sps_pps.length() - sps_length - 1, sps_length+1);
      pps_ptr[sps_pps.length() - sps_length - 1] = '\0'; // don't forget the terminating 0
      std::string pps(pps_ptr);
      pps_ = spsPpsParser.DecodeBase64(pps);
      free(pps_ptr);
      pps_ptr = NULL;

      spsPpsParser.ParseSpsPps(reinterpret_cast<const unsigned char*> (sps_.c_str()), &spsInfo);
      width_ = spsInfo.width;
      height_ = spsInfo.height;
    }
}
#endif

bool RTPReceiverVideo::ShouldReportCsrcChanges(
    uint8_t payload_type) const {
  // Always do this for video packets.
  return true;
}

int32_t RTPReceiverVideo::OnNewPayloadTypeCreated(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type,
    uint32_t frequency) {
  return 0;
}

int32_t RTPReceiverVideo::ParseRtpPacket(
    WebRtcRTPHeader* rtp_header,
    const PayloadUnion& specific_payload,
    bool is_red,
    const uint8_t* payload,
    uint16_t payload_length,
    int64_t timestamp_ms,
    bool is_first_packet) {
  TRACE_EVENT2("webrtc_rtp", "Video::ParseRtp",
               "seqnum", rtp_header->header.sequenceNumber,
               "timestamp", rtp_header->header.timestamp);
  rtp_header->type.Video.codec = specific_payload.Video.videoCodecType;

  const uint16_t payload_data_length =
      payload_length - rtp_header->header.paddingLength;

  if (payload_data_length == 0)
    return data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) == 0 ? 0
                                                                           : -1;

  return ParseVideoCodecSpecific(rtp_header,
                                 payload,
                                 payload_data_length,
                                 specific_payload.Video.videoCodecType,
                                 timestamp_ms,
                                 is_first_packet);
}

int RTPReceiverVideo::GetPayloadTypeFrequency() const {
  return kVideoPayloadTypeFrequency;
}

RTPAliveType RTPReceiverVideo::ProcessDeadOrAlive(
    uint16_t last_payload_length) const {
  return kRtpDead;
}

int32_t RTPReceiverVideo::InvokeOnInitializeDecoder(
    RtpFeedback* callback,
    int32_t id,
    int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const PayloadUnion& specific_payload) const {
  // For video we just go with default values.
  if (-1 == callback->OnInitializeDecoder(
      id, payload_type, payload_name, kVideoPayloadTypeFrequency, 1, 0)) {
    WEBRTC_TRACE(kTraceError,
                 kTraceRtpRtcp,
                 id,
                 "Failed to create video decoder for payload type:%d",
                 payload_type);
    return -1;
  }
  return 0;
}

// We are not allowed to hold a critical section when calling this function.
int32_t RTPReceiverVideo::ParseVideoCodecSpecific(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    uint16_t payload_data_length,
    RtpVideoCodecTypes video_type,
    int64_t now_ms,
    bool is_first_packet) {
  WEBRTC_TRACE(kTraceStream,
               kTraceRtpRtcp,
               id_,
               "%s(timestamp:%u)",
               __FUNCTION__,
               rtp_header->header.timestamp);

  switch (rtp_header->type.Video.codec) {
    case kRtpVideoGeneric:
      rtp_header->type.Video.isFirstPacket = is_first_packet;
      return ReceiveGenericCodec(rtp_header, payload_data, payload_data_length);
#if defined(ENABLE_WEBRTC_H264_CODEC)
    case kRtpVideoH264:
      return ReceiveH264Codec(rtp_header, payload_data, payload_data_length);
#endif
    case kRtpVideoVp8:
      return ReceiveVp8Codec(rtp_header, payload_data, payload_data_length);
    case kRtpVideoNone:
      break;
  }
  return -1;
}

int32_t RTPReceiverVideo::BuildRTPheader(
    const WebRtcRTPHeader* rtp_header,
    uint8_t* data_buffer) const {
  data_buffer[0] = static_cast<uint8_t>(0x80);  // version 2
  data_buffer[1] = static_cast<uint8_t>(rtp_header->header.payloadType);
  if (rtp_header->header.markerBit) {
    data_buffer[1] |= kRtpMarkerBitMask;  // MarkerBit is 1
  }
  ModuleRTPUtility::AssignUWord16ToBuffer(data_buffer + 2,
                                          rtp_header->header.sequenceNumber);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 4,
                                          rtp_header->header.timestamp);
  ModuleRTPUtility::AssignUWord32ToBuffer(data_buffer + 8,
                                          rtp_header->header.ssrc);

  int32_t rtp_header_length = 12;

  // Add the CSRCs if any
  if (rtp_header->header.numCSRCs > 0) {
    if (rtp_header->header.numCSRCs > 16) {
      // error
      assert(false);
    }
    uint8_t* ptr = &data_buffer[rtp_header_length];
    for (uint32_t i = 0; i < rtp_header->header.numCSRCs; ++i) {
      ModuleRTPUtility::AssignUWord32ToBuffer(ptr,
                                              rtp_header->header.arrOfCSRCs[i]);
      ptr += 4;
    }
    data_buffer[0] = (data_buffer[0] & 0xf0) | rtp_header->header.numCSRCs;
    // Update length of header
    rtp_header_length += sizeof(uint32_t) * rtp_header->header.numCSRCs;
  }
  return rtp_header_length;
}

#if defined(ENABLE_WEBRTC_H264_CODEC)
int32_t RTPReceiverVideo::ReceiveH264Codec(WebRtcRTPHeader* rtp_header,
                                          const uint8_t* payload_data,
                                          uint16_t payload_data_length) {
  bool success;
  ModuleRTPUtility::RTPPayload parsed_packet;
  if (payload_data_length == 0) {
    success = true;
    parsed_packet.info.H264.dataLength = 0;
  } else {
    ModuleRTPUtility::RTPPayloadParser rtp_payload_parser(
        kRtpVideoH264, payload_data, payload_data_length, id_);

    success = rtp_payload_parser.Parse(parsed_packet);
  }
  // from here down we only work on local data
  crit_sect_->Leave();

  if (!success)
    return -1;

  if (parsed_packet.info.H264.dataLength == 0) {
    // we have an "empty" H264 packet, it's ok, could be one way video
    // Inform the jitter buffer about this packet.
    rtp_header->frameType = kFrameEmpty;
    if (data_callback_->OnReceivedPayloadData(NULL, 0, rtp_header) != 0)
      return -1;

    return 0;
  }

  rtp_header->frameType = (parsed_packet.frameType ==
    ModuleRTPUtility::kIFrame) ? kVideoFrameKey : kVideoFrameDelta;

  ModuleRTPUtility::RTPPayloadH264* from_header = &parsed_packet.info.H264;

  if (from_header->frameWidth > 0 && from_header->frameHeight > 0) {
    width_ = from_header->frameWidth;
    height_ = from_header->frameHeight; 
  }
  rtp_header->type.Video.width = width_;
  rtp_header->type.Video.height = height_;
  rtp_header->header.markerBit = parsed_packet.info.H264.markerBit;
  rtp_header->type.Video.isFirstPacket = parsed_packet.info.H264.isFirstPacket;

  if (parsed_packet.info.H264.isFirstPacket == true)
    rtp_header->type.Video.codecHeader.H264.hasStartCode = true;

  if (data_callback_->OnReceivedPayloadData(parsed_packet.info.H264.data,
        parsed_packet.info.H264.dataLength, rtp_header) != 0) {
    return -1;
  }
  return 0;
}
#endif // ENABLE_WEBRTC_H264_CODEC

int32_t RTPReceiverVideo::ReceiveVp8Codec(WebRtcRTPHeader* rtp_header,
                                          const uint8_t* payload_data,
                                          uint16_t payload_data_length) {
  ModuleRTPUtility::RTPPayload parsed_packet;
  uint32_t id;
  {
    CriticalSectionScoped cs(crit_sect_.get());
    id = id_;
  }
  ModuleRTPUtility::RTPPayloadParser rtp_payload_parser(
      kRtpVideoVp8, payload_data, payload_data_length, id);

  if (!rtp_payload_parser.Parse(parsed_packet))
    return -1;

  if (parsed_packet.info.VP8.dataLength == 0)
    return 0;

  rtp_header->frameType = (parsed_packet.frameType == ModuleRTPUtility::kIFrame)
      ? kVideoFrameKey : kVideoFrameDelta;

  RTPVideoHeaderVP8* to_header = &rtp_header->type.Video.codecHeader.VP8;
  ModuleRTPUtility::RTPPayloadVP8* from_header = &parsed_packet.info.VP8;

  rtp_header->type.Video.isFirstPacket =
      from_header->beginningOfPartition && (from_header->partitionID == 0);
  to_header->nonReference = from_header->nonReferenceFrame;
  to_header->pictureId =
      from_header->hasPictureID ? from_header->pictureID : kNoPictureId;
  to_header->tl0PicIdx =
      from_header->hasTl0PicIdx ? from_header->tl0PicIdx : kNoTl0PicIdx;
  if (from_header->hasTID) {
    to_header->temporalIdx = from_header->tID;
    to_header->layerSync = from_header->layerSync;
  } else {
    to_header->temporalIdx = kNoTemporalIdx;
    to_header->layerSync = false;
  }
  to_header->keyIdx = from_header->hasKeyIdx ? from_header->keyIdx : kNoKeyIdx;

  rtp_header->type.Video.width = from_header->frameWidth;
  rtp_header->type.Video.height = from_header->frameHeight;

  to_header->partitionId = from_header->partitionID;
  to_header->beginningOfPartition = from_header->beginningOfPartition;

  if (data_callback_->OnReceivedPayloadData(parsed_packet.info.VP8.data,
                                            parsed_packet.info.VP8.dataLength,
                                            rtp_header) != 0) {
    return -1;
  }
  return 0;
}

int32_t RTPReceiverVideo::ReceiveGenericCodec(
    WebRtcRTPHeader* rtp_header,
    const uint8_t* payload_data,
    uint16_t payload_data_length) {
  uint8_t generic_header = *payload_data++;
  --payload_data_length;

  rtp_header->frameType =
      ((generic_header & RtpFormatVideoGeneric::kKeyFrameBit) != 0) ?
      kVideoFrameKey : kVideoFrameDelta;
  rtp_header->type.Video.isFirstPacket =
      (generic_header & RtpFormatVideoGeneric::kFirstPacketBit) != 0;

  if (data_callback_->OnReceivedPayloadData(
          payload_data, payload_data_length, rtp_header) != 0) {
    return -1;
  }
  return 0;
}
}  // namespace webrtc
