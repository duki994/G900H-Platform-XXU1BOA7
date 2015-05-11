/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
enum { REDForFECHeaderLength = 1 };

struct RtpPacket {
  uint16_t rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RTPSenderVideo::RTPSenderVideo(const int32_t id,
                               Clock* clock,
                               RTPSenderInterface* rtpSender)
    : _id(id),
      _rtpSender(*rtpSender),
      _sendVideoCritsect(CriticalSectionWrapper::CreateCriticalSection()),
      _videoType(kRtpVideoGeneric),
      _videoCodecInformation(NULL),
      _maxBitrate(0),
      _retransmissionSettings(kRetransmitBaseLayer),

      // Generic FEC
      _fec(id),
      _fecEnabled(false),
      _payloadTypeRED(-1),
      _payloadTypeFEC(-1),
      _numberFirstPartition(0),
      delta_fec_params_(),
      key_fec_params_(),
      producer_fec_(&_fec),
      _fecOverheadRate(clock, NULL),
      _videoBitrate(clock, NULL) {
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
  delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
        kFecMaskRandom;
}

RTPSenderVideo::~RTPSenderVideo()
{
    if(_videoCodecInformation)
    {
        delete _videoCodecInformation;
    }
    delete _sendVideoCritsect;
}

void
RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes videoType)
{
    CriticalSectionScoped cs(_sendVideoCritsect);
    _videoType = videoType;
}

RtpVideoCodecTypes
RTPSenderVideo::VideoCodecType() const
{
    return _videoType;
}

int32_t RTPSenderVideo::RegisterVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payloadType,
    const uint32_t maxBitRate,
    ModuleRTPUtility::Payload*& payload) {
  CriticalSectionScoped cs(_sendVideoCritsect);

  RtpVideoCodecTypes videoType = kRtpVideoGeneric;
  if (ModuleRTPUtility::StringCompare(payloadName, "VP8",3)) {
    videoType = kRtpVideoVp8;
  } else if (ModuleRTPUtility::StringCompare(payloadName, "I420", 4)) {
    videoType = kRtpVideoGeneric;
#if defined(ENABLE_WEBRTC_H264_CODEC)
  } else if (ModuleRTPUtility::StringCompare(payloadName, "h264", 4)) {
    videoType = kRtpVideoH264;
#endif
  } else {
    videoType = kRtpVideoGeneric;
  }
  payload = new ModuleRTPUtility::Payload;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = videoType;
  payload->typeSpecific.Video.maxRate = maxBitRate;
  payload->audio = false;
  return 0;
}

int32_t
RTPSenderVideo::SendVideoPacket(uint8_t* data_buffer,
                                const uint16_t payload_length,
                                const uint16_t rtp_header_length,
                                const uint32_t capture_timestamp,
                                int64_t capture_time_ms,
                                StorageType storage,
                                bool protect) {
  if(_fecEnabled) {
    int ret = 0;
    int fec_overhead_sent = 0;
    int video_sent = 0;

    RedPacket* red_packet = producer_fec_.BuildRedPacket(data_buffer,
                                                         payload_length,
                                                         rtp_header_length,
                                                         _payloadTypeRED);
    TRACE_EVENT_INSTANT2("webrtc_rtp", "Video::PacketRed",
                         "timestamp", capture_timestamp,
                         "seqnum", _rtpSender.SequenceNumber());
    // Sending the media packet with RED header.
    int packet_success = _rtpSender.SendToNetwork(
        red_packet->data(),
        red_packet->length() - rtp_header_length,
        rtp_header_length,
        capture_time_ms,
        storage,
        PacedSender::kNormalPriority);

    ret |= packet_success;

    if (packet_success == 0) {
      video_sent += red_packet->length();
    }
    delete red_packet;
    red_packet = NULL;

    if (protect) {
      ret = producer_fec_.AddRtpPacketAndGenerateFec(data_buffer,
                                                     payload_length,
                                                     rtp_header_length);
      if (ret != 0)
        return ret;
    }

    while (producer_fec_.FecAvailable()) {
      red_packet = producer_fec_.GetFecPacket(
          _payloadTypeRED,
          _payloadTypeFEC,
          _rtpSender.IncrementSequenceNumber(),
          rtp_header_length);
      StorageType storage = kDontRetransmit;
      if (_retransmissionSettings & kRetransmitFECPackets) {
        storage = kAllowRetransmission;
      }
      TRACE_EVENT_INSTANT2("webrtc_rtp", "Video::PacketFec",
                           "timestamp", capture_timestamp,
                           "seqnum", _rtpSender.SequenceNumber());
      // Sending FEC packet with RED header.
      int packet_success = _rtpSender.SendToNetwork(
          red_packet->data(),
          red_packet->length() - rtp_header_length,
          rtp_header_length,
          capture_time_ms,
          storage,
          PacedSender::kNormalPriority);

      ret |= packet_success;

      if (packet_success == 0) {
        fec_overhead_sent += red_packet->length();
      }
      delete red_packet;
      red_packet = NULL;
    }
    _videoBitrate.Update(video_sent);
    _fecOverheadRate.Update(fec_overhead_sent);
    return ret;
  }
  TRACE_EVENT_INSTANT2("webrtc_rtp", "Video::PacketNormal",
                       "timestamp", capture_timestamp,
                       "seqnum", _rtpSender.SequenceNumber());
  int ret = _rtpSender.SendToNetwork(data_buffer,
                                     payload_length,
                                     rtp_header_length,
                                     capture_time_ms,
                                     storage,
                                     PacedSender::kNormalPriority);
  if (ret == 0) {
    _videoBitrate.Update(payload_length + rtp_header_length);
  }
  return ret;
}

int32_t
RTPSenderVideo::SendRTPIntraRequest()
{
    // RFC 2032
    // 5.2.1.  Full intra-frame Request (FIR) packet

    uint16_t length = 8;
    uint8_t data[8];
    data[0] = 0x80;
    data[1] = 192;
    data[2] = 0;
    data[3] = 1; // length

    ModuleRTPUtility::AssignUWord32ToBuffer(data+4, _rtpSender.SSRC());

    TRACE_EVENT_INSTANT1("webrtc_rtp",
                         "Video::IntraRequest",
                         "seqnum", _rtpSender.SequenceNumber());
    return _rtpSender.SendToNetwork(data, 0, length, -1, kDontStore,
                                    PacedSender::kNormalPriority);
}

int32_t
RTPSenderVideo::SetGenericFECStatus(const bool enable,
                                    const uint8_t payloadTypeRED,
                                    const uint8_t payloadTypeFEC)
{
    _fecEnabled = enable;
    _payloadTypeRED = payloadTypeRED;
    _payloadTypeFEC = payloadTypeFEC;
    memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
    memset(&key_fec_params_, 0, sizeof(key_fec_params_));
    delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
    delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
          kFecMaskRandom;
    return 0;
}

int32_t
RTPSenderVideo::GenericFECStatus(bool& enable,
                                 uint8_t& payloadTypeRED,
                                 uint8_t& payloadTypeFEC) const
{
    enable = _fecEnabled;
    payloadTypeRED = _payloadTypeRED;
    payloadTypeFEC = _payloadTypeFEC;
    return 0;
}

uint16_t
RTPSenderVideo::FECPacketOverhead() const
{
    if (_fecEnabled)
    {
      // Overhead is FEC headers plus RED for FEC header plus anything in RTP
      // header beyond the 12 bytes base header (CSRC list, extensions...)
      // This reason for the header extensions to be included here is that
      // from an FEC viewpoint, they are part of the payload to be protected.
      // (The base RTP header is already protected by the FEC header.)
      return ForwardErrorCorrection::PacketOverhead() + REDForFECHeaderLength +
             (_rtpSender.RTPHeaderLength() - kRtpHeaderSize);
    }
    return 0;
}

int32_t RTPSenderVideo::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  assert(delta_params);
  assert(key_params);
  delta_fec_params_ = *delta_params;
  key_fec_params_ = *key_params;
  return 0;
}

int32_t
RTPSenderVideo::SendVideo(const RtpVideoCodecTypes videoType,
                          const FrameType frameType,
                          const int8_t payloadType,
                          const uint32_t captureTimeStamp,
                          int64_t capture_time_ms,
                          const uint8_t* payloadData,
                          const uint32_t payloadSize,
                          const RTPFragmentationHeader* fragmentation,
                          VideoCodecInformation* codecInfo,
                          const RTPVideoTypeHeader* rtpTypeHdr)
{
    if( payloadSize == 0)
    {
        return -1;
    }

    if (frameType == kVideoFrameKey) {
      producer_fec_.SetFecParameters(&key_fec_params_,
                                     _numberFirstPartition);
    } else {
      producer_fec_.SetFecParameters(&delta_fec_params_,
                                     _numberFirstPartition);
    }

    // Default setting for number of first partition packets:
    // Will be extracted in SendVP8 for VP8 codec; other codecs use 0
    _numberFirstPartition = 0;

    int32_t retVal = -1;
    switch(videoType)
    {
    case kRtpVideoGeneric:
        retVal = SendGeneric(frameType, payloadType, captureTimeStamp,
                             capture_time_ms, payloadData, payloadSize);
        break;
#if defined(ENABLE_WEBRTC_H264_CODEC)
    case kRtpVideoH264:
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
            "RTPSenderVideo::SendVideo++ kRtpVideoH264");
        retVal = SendH264(frameType,
                         payloadType,
                         captureTimeStamp,
                         capture_time_ms,
                         payloadData,
                         payloadSize,
                         rtpTypeHdr,
                         0); // Set to Zero Need to take from HW Encoder
        break;
#endif
    case kRtpVideoVp8:
        retVal = SendVP8(frameType,
                         payloadType,
                         captureTimeStamp,
                         capture_time_ms,
                         payloadData,
                         payloadSize,
                         fragmentation,
                         rtpTypeHdr);
        break;
    default:
        assert(false);
        break;
    }
    if(retVal <= 0)
    {
        return retVal;
    }
    WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id, "%s(timestamp:%u)",
                 __FUNCTION__, captureTimeStamp);
    return 0;
}

int32_t RTPSenderVideo::SendGeneric(const FrameType frame_type,
                                    const int8_t payload_type,
                                    const uint32_t capture_timestamp,
                                    int64_t capture_time_ms,
                                    const uint8_t* payload,
                                    uint32_t size) {
  assert(frame_type == kVideoFrameKey || frame_type == kVideoFrameDelta);
  uint16_t rtp_header_length = _rtpSender.RTPHeaderLength();
  uint16_t max_length = _rtpSender.MaxPayloadLength() - FECPacketOverhead() -
                        rtp_header_length - (1 /* generic header length */);

  // Fragment packets more evenly by splitting the payload up evenly.
  uint32_t num_packets = (size + max_length - 1) / max_length;
  uint32_t payload_length = (size + num_packets - 1) / num_packets;
  assert(payload_length <= max_length);

  // Fragment packet into packets of max MaxPayloadLength bytes payload.
  uint8_t buffer[IP_PACKET_SIZE];

  uint8_t generic_header = RtpFormatVideoGeneric::kFirstPacketBit;
  if (frame_type == kVideoFrameKey) {
    generic_header |= RtpFormatVideoGeneric::kKeyFrameBit;
  }

  while (size > 0) {
    if (size < payload_length) {
      payload_length = size;
    }
    size -= payload_length;

    // MarkerBit is 1 on final packet (bytes_to_send == 0)
    if (_rtpSender.BuildRTPheader(buffer, payload_type, size == 0,
                                  capture_timestamp,
                                  capture_time_ms) != rtp_header_length) {
      return -1;
    }

    uint8_t* out_ptr = &buffer[rtp_header_length];

    // Put generic header in packet
    *out_ptr++ = generic_header;
    // Remove first-packet bit, following packets are intermediate
    generic_header &= ~RtpFormatVideoGeneric::kFirstPacketBit;

    // Put payload in packet
    memcpy(out_ptr, payload, payload_length);
    payload += payload_length;

    if (SendVideoPacket(buffer, payload_length + 1, rtp_header_length,
                        capture_timestamp, capture_time_ms,
                        kAllowRetransmission, true)) {
      return -1;
    }
  }
  return 0;
}

VideoCodecInformation*
RTPSenderVideo::CodecInformationVideo()
{
    return _videoCodecInformation;
}

void
RTPSenderVideo::SetMaxConfiguredBitrateVideo(const uint32_t maxBitrate)
{
    _maxBitrate = maxBitrate;
}

uint32_t
RTPSenderVideo::MaxConfiguredBitrateVideo() const
{
    return _maxBitrate;
}

#if defined(ENABLE_WEBRTC_H264_CODEC)
bool
RTPSenderVideo::IsNALUFound(const uint8_t *dataPtr)
{
    if (dataPtr[0] == 0x00 && dataPtr[1] == 0x00 && dataPtr[2]== 0x00 &&
                dataPtr[3] == 0x01)
        return true;

    return false;
}

int32_t
RTPSenderVideo::SendH264(const FrameType frameType,
                        const int8_t payloadType,
                        const uint32_t captureTimeStamp,
                        int64_t capture_time_ms,
                        const uint8_t* payloadData,
                        const uint32_t payloadSize,
                        const RTPVideoTypeHeader* rtpTypeHdr,
                        uint8_t frameMarker)
{
    const uint16_t rtpHeaderLength = _rtpSender.RTPHeaderLength();
    // int32_t payloadBytesToSend = payloadSize;
    // const uint8_t* data = payloadData;
    // uint16_t maxPayloadLengthVP8 = _rtpSender.MaxDataPayloadLength();

    assert(rtpTypeHdr);
    RtpFormatH264 H264Packetizer;
    MediaFrameInfo currentCodecFrame;
    uint8_t dataBuffer[IP_PACKET_SIZE] = {0};

    memset(&currentCodecFrame, 0, sizeof(MediaFrameInfo));

    currentCodecFrame.frm_len = payloadSize;
    currentCodecFrame.frm_marker = frameMarker;
    currentCodecFrame.frm_ts = captureTimeStamp;
    currentCodecFrame.frm_data = payloadData;

    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
        "RTPSenderVideo::SendH264 payloadSize=%d", payloadSize);

    // Process the data in buffer based on H264 standard
    // here we are not checking whether frame is empty
    // or not hence empty frame can be sent
    if (currentCodecFrame.frm_len <= H264MAXMTUSIZE
                && currentCodecFrame.frm_len > 0) {
        if (currentCodecFrame.frm_len >= H264_BASIC_HDR_LEN) {
            uint16_t nalu_size = 0;
            uint32_t index = 0;

            currentCodecFrame.frm_isvalid = true;
            currentCodecFrame.frm_marker = true;

            WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                "RTPSenderVideo::SendH264 len >= H264_BASIC_HDR_LEN"
                "nalu_size=%d frm_len=%d rtpHeaderLength=%d",
                nalu_size,
                currentCodecFrame.frm_len,
                rtpHeaderLength);

            if ( currentCodecFrame.frm_data[4] == 0x67) {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, 0,
                    captureTimeStamp,capture_time_ms);
                // SPS
                if (IsNALUFound(currentCodecFrame.frm_data) &&
                                      currentCodecFrame.frm_data[4] == 0x67) {
                    for (index = 0; index < currentCodecFrame.frm_len; index++) {
                        if (IsNALUFound(currentCodecFrame.frm_data + index) &&
                            currentCodecFrame.frm_data[index+4] == 0x68) {
                          memcpy(&dataBuffer[rtpHeaderLength],
                              currentCodecFrame.frm_data + 4, index - 4);

                          if (-1 == SendVideoPacket(dataBuffer, index - 4,
                              rtpHeaderLength, captureTimeStamp,
                              capture_time_ms, kAllowRetransmission,
                              true)) {
                            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                                "RTPSenderVideo::SendH264 failed to send"
                                "packet number %d", _rtpSender.SequenceNumber());
                          }
                          break;
                        }
                    }
                }
                // PPS
                if (currentCodecFrame.frm_data[index+4] == 0x68) {
                    _rtpSender.BuildRTPheader(dataBuffer, payloadType, 1,
                        captureTimeStamp, capture_time_ms);
                    memcpy(&dataBuffer[rtpHeaderLength], currentCodecFrame.frm_data + index + 4,
                        currentCodecFrame.frm_len - index - 4);
                    if (-1 == SendVideoPacket(dataBuffer, currentCodecFrame.frm_len - index - 4,
                        rtpHeaderLength, captureTimeStamp,
                        capture_time_ms, kAllowRetransmission,
                        true)) {
                      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                      "RTPSenderVideo::SendH264 failed to send packet number"
                      " %d", _rtpSender.SequenceNumber());
                    }
                }
            } else {
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, 1, captureTimeStamp,
                    capture_time_ms);
                memcpy(&dataBuffer[rtpHeaderLength], currentCodecFrame.frm_data + 4,
                    currentCodecFrame.frm_len - 4);
                if (-1 == SendVideoPacket(dataBuffer, currentCodecFrame.frm_len - 4,
                    rtpHeaderLength, captureTimeStamp,
                    capture_time_ms, kAllowRetransmission,
                    true)) {
                  WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                      "RTPSenderVideo::SendH264 failed to send packet number"
                      " %d", _rtpSender.SequenceNumber());
                }
            }
        }
    } else {
        /*
        rfc3984 - packetization-mode is 0 - then use slice mode
        packetization-mode is 1 - then get a NALU per frame - then
        packetizer has to fragment the NALU into multiple FUs
        */
        // TODO: who Need to set ??
        int iPacketizationMode = 1;

        if (0 == iPacketizationMode) {
            /*Check NALU Boundary*/
            const uint8_t *FrmData = payloadData;
            int32_t FrmLen = (int32_t) payloadSize;
            //uint8_t *nau_p = NULL;
            uint16_t nalu_size = 0;
            bool last = false;

            while (FrmLen > 0) {
                H264Packetizer.ExtractNALUFromEncFrame((uint8_t *)FrmData,
                    FrmLen, &nalu_size);
                currentCodecFrame.frm_data = FrmData;
                currentCodecFrame.frm_len = nalu_size;
                currentCodecFrame.frm_isvalid = true;
                currentCodecFrame.frm_ts = captureTimeStamp;

                if ((int32_t)(FrmLen - nalu_size - ANNEXB_BUFF_LEN) > 0) {
                    currentCodecFrame.frm_marker = false;
                } else {
                    //last NALU in the Encoder o/p Frame
                    currentCodecFrame.frm_marker = true;
                }

                /*Setting Values for Next Cycle*/
                FrmLen = FrmLen - nalu_size - ANNEXB_BUFF_LEN;
                FrmData = FrmData + nalu_size + ANNEXB_BUFF_LEN;

                WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                    "RTPSenderVideo::SendH264 FrmLen=%d", FrmLen);

                // Create the RTP payload, update and send the packet to network
                // Write RTP header.
                // Set marker bit true if this is the last packet in frame.
                if (FrmLen < 0)
                    last = true;

                _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
                    captureTimeStamp, capture_time_ms);

                memcpy(&dataBuffer[rtpHeaderLength],
                currentCodecFrame.frm_data, currentCodecFrame.frm_len);

                if (-1 == SendVideoPacket(dataBuffer, currentCodecFrame.frm_len,
                    rtpHeaderLength, captureTimeStamp,
                    capture_time_ms, kAllowRetransmission,
                    true)) {
                  WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                      "RTPSenderVideo::SendH264 failed to send packet number"
                      " %d", _rtpSender.SequenceNumber());
                }
            }
            return 0;
        } else {
            // Frame size exceeds the MTU Size - Fragment the packet
            // Support for Fragmentation Units FU-A */
            // Split the packet into two or more parts
            // depending upon the packet length
            uint16_t ui8_pload_hdr_len = 0;
            uint8_t ui8_num_frames_made = 0;
            MediaFrameInfo operating_frm = {0};
            bool bool_start_bit = true, bool_end_bit = false;
            H264SliceHeader slice_hdr_info = {0};
            bool last = false;
            int spsLength = 0, sps_pps_length;

            signed int frag = currentCodecFrame.frm_len;
            if (IsNALUFound(currentCodecFrame.frm_data) &&
                ((currentCodecFrame.frm_data[4] & 0x1F) == 7)) {
                //Send SPS packet
                for (spsLength = NALU_HDR_LEN + 1; spsLength <
                    (int)currentCodecFrame.frm_len; spsLength++) {
                    if (IsNALUFound(currentCodecFrame.frm_data+spsLength) &&
                        currentCodecFrame.frm_data[spsLength + 4] == 0x68)
                      break;
                }
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, 0,
                    captureTimeStamp, capture_time_ms);
                memcpy(&dataBuffer[rtpHeaderLength],
                    currentCodecFrame.frm_data + H264_SKIP_START_CODE,
                    spsLength - H264_SKIP_START_CODE);

                if (-1 == SendVideoPacket(dataBuffer, spsLength -
                        H264_SKIP_START_CODE, rtpHeaderLength, captureTimeStamp,
                        capture_time_ms, kAllowRetransmission,
                        true)) {
                  WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                      "RTPSenderVideo::SendH264 failed to send packet number"
                      " %d", _rtpSender.SequenceNumber());
                }
                // Send PPS packet
                for (sps_pps_length = spsLength + NALU_HDR_LEN + 1;
                    sps_pps_length < (int)currentCodecFrame.frm_len; sps_pps_length++) {
                    if (IsNALUFound(currentCodecFrame.frm_data + sps_pps_length) &&
                        (currentCodecFrame.frm_data[sps_pps_length + 4] & 0x1F) == 5)
                      break;
                }
                memcpy(&dataBuffer[rtpHeaderLength],
                currentCodecFrame.frm_data + spsLength + H264_SKIP_START_CODE,
                    sps_pps_length - spsLength - H264_SKIP_START_CODE);
                _rtpSender.BuildRTPheader(dataBuffer, payloadType, 1,
                    captureTimeStamp, capture_time_ms);
                if (-1 == SendVideoPacket(dataBuffer, sps_pps_length - spsLength -
                    H264_SKIP_START_CODE, rtpHeaderLength, captureTimeStamp,
                    capture_time_ms, kAllowRetransmission,
                    true)) {
                  WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                      "RTPSenderVideo::SendH264 failed to send packet number"
                      " %d", _rtpSender.SequenceNumber());
                }
            //skip the sps and pps data
            currentCodecFrame.frm_data += sps_pps_length;
            currentCodecFrame.frm_len -= sps_pps_length;
            frag = currentCodecFrame.frm_len;
        }
        memset(&operating_frm, 0, sizeof(MediaFrameInfo));

        // Get the type of the Current frame
        H264Packetizer.ParseSingleNALUSliceHeader(&slice_hdr_info,
            (uint8_t *)currentCodecFrame.frm_data + H264_SKIP_START_CODE);

        //slice_hdr_info.nal_unit_payload_type = NAL_FU_FORMAT;

        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "RTPSenderVideo::SendH264 currentCodecFrame.frm_len =%d",
            currentCodecFrame.frm_len);

        // Divide the payload and hence get the frames
        while (frag/*operating_frm.frm_len*/ > H264MAXMTUSIZE) {
            // Make the frame with largest size possible
            // Build the header of 2bytes including one basic hdr and one FU Indicator hdr
            // ui8_pload_hdr_len = BuildH264SingleNALUFUPayloadHeader(&slice_hdr_info,
            // out_h264_frm.frm_data, bool_start_bit, bool_end_bit);

            ui8_pload_hdr_len = H264Packetizer.BuildSingleNALUFUPayloadHeader (
                    &slice_hdr_info, (uint8_t *)(&dataBuffer[rtpHeaderLength]),
                    bool_start_bit, bool_end_bit);

            if (bool_start_bit == true) {
                // skipping the StartCode for Packetisation
                currentCodecFrame.frm_data += H264_SKIP_START_CODE;
                currentCodecFrame.frm_len -= H264_SKIP_START_CODE;
                currentCodecFrame.frm_data += H264_BASIC_HDR_LEN;
                //as we wont be sending the first NAL Header byte
                currentCodecFrame.frm_len -= H264_BASIC_HDR_LEN;
                frag = currentCodecFrame.frm_len;
            }

            /* Create the RTP Payload */
            operating_frm.frm_data = currentCodecFrame.frm_data +
                    (currentCodecFrame.frm_len - frag)/* + H264_BASIC_HDR_LEN*/;
            operating_frm.frm_isvalid = true;
            operating_frm.frm_marker = false;
            operating_frm.frm_len = H264MAXMTUSIZE - 2;
            operating_frm.frm_ts = currentCodecFrame.frm_ts;

            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                "RTPSenderVideo::SendH264 sending ui8_num_frames_made=%d ,"
                "operating_frm.frm_len = %d",ui8_num_frames_made, operating_frm.frm_len);
            // Add the frame to the frame list
            // Here no frame length checking as FU can also contains empty packet.
            //Create the RTP payload  and update the frame details
            _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
                captureTimeStamp, capture_time_ms);
            memcpy(&dataBuffer[rtpHeaderLength + ui8_pload_hdr_len],
                operating_frm.frm_data, operating_frm.frm_len);
            if (-1 == SendVideoPacket(dataBuffer, operating_frm.frm_len + ui8_pload_hdr_len,
                        rtpHeaderLength, captureTimeStamp,
                        capture_time_ms, kAllowRetransmission,
                        true)) {
                WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                    "RTPSenderVideo::SendH264 failed to send packet number"
                    " %d", _rtpSender.SequenceNumber());
            }

            // Update the operating frame and variables for next iteration
            bool_start_bit = false;
            frag -= H264MAXMTUSIZE - 2;
            ui8_num_frames_made++;
        }

        // As it came out of while loop hence now make the last
        // frame of the Fragmented Unit check for a valid length of
        // packet as we dont want to make a fragmented packet of 0 length
        if (frag > 0) {
            //Update the local variables
            bool_end_bit = true;
            bool_start_bit = false;

            // Build the header of 2bytes including one basic hdr and one FU Indicator hdr
            ui8_pload_hdr_len = H264Packetizer.BuildSingleNALUFUPayloadHeader (
                  &slice_hdr_info, (uint8_t *)&dataBuffer[rtpHeaderLength],
                  bool_start_bit, bool_end_bit);

            /* Create the RTP Payload */
            operating_frm.frm_data = currentCodecFrame.frm_data +
                (currentCodecFrame.frm_len - frag);
            operating_frm.frm_isvalid = true;
            operating_frm.frm_marker = true;
            operating_frm.frm_len = frag;
            operating_frm.frm_ts = currentCodecFrame.frm_ts;

            //Create the RTP payload  and update the frame details
            _rtpSender.BuildRTPheader(dataBuffer, payloadType, 1,
                captureTimeStamp, capture_time_ms);
            memcpy(&dataBuffer[rtpHeaderLength + ui8_pload_hdr_len],
                operating_frm.frm_data, operating_frm.frm_len);
            if (-1 == SendVideoPacket(dataBuffer, operating_frm.frm_len + ui8_pload_hdr_len,
                rtpHeaderLength, captureTimeStamp,
                capture_time_ms, kAllowRetransmission,
                true)) {
              WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                  "RTPSenderVideo::SendH264 failed to send packet number"
                  " %d", _rtpSender.SequenceNumber());
            }
            ui8_num_frames_made++;
            }
        }
    }
    return 0;
}
#endif

int32_t
RTPSenderVideo::SendVP8(const FrameType frameType,
                        const int8_t payloadType,
                        const uint32_t captureTimeStamp,
                        int64_t capture_time_ms,
                        const uint8_t* payloadData,
                        const uint32_t payloadSize,
                        const RTPFragmentationHeader* fragmentation,
                        const RTPVideoTypeHeader* rtpTypeHdr)
{
    const uint16_t rtpHeaderLength = _rtpSender.RTPHeaderLength();

    int32_t payloadBytesToSend = payloadSize;
    const uint8_t* data = payloadData;

    uint16_t maxPayloadLengthVP8 = _rtpSender.MaxDataPayloadLength();

    assert(rtpTypeHdr);
    // Initialize disregarding partition boundaries: this will use kEqualSize
    // packetization mode, which produces ~equal size packets for each frame.
    RtpFormatVp8 packetizer(data, payloadBytesToSend, rtpTypeHdr->VP8,
                            maxPayloadLengthVP8);

    StorageType storage = kAllowRetransmission;
    if (rtpTypeHdr->VP8.temporalIdx == 0 &&
        !(_retransmissionSettings & kRetransmitBaseLayer)) {
      storage = kDontRetransmit;
    }
    if (rtpTypeHdr->VP8.temporalIdx > 0 &&
        !(_retransmissionSettings & kRetransmitHigherLayers)) {
      storage = kDontRetransmit;
    }

    bool last = false;
    _numberFirstPartition = 0;
    // |rtpTypeHdr->VP8.temporalIdx| is zero for base layers, or -1 if the field
    // isn't used. We currently only protect base layers.
    bool protect = (rtpTypeHdr->VP8.temporalIdx < 1);
    while (!last)
    {
        // Write VP8 Payload Descriptor and VP8 payload.
        uint8_t dataBuffer[IP_PACKET_SIZE] = {0};
        int payloadBytesInPacket = 0;
        int packetStartPartition =
            packetizer.NextPacket(&dataBuffer[rtpHeaderLength],
                                  &payloadBytesInPacket, &last);
        // TODO(holmer): Temporarily disable first partition packet counting
        // to avoid a bug in ProducerFec which doesn't properly handle
        // important packets.
        // if (packetStartPartition == 0)
        // {
        //     ++_numberFirstPartition;
        // }
        // else
        if (packetStartPartition < 0)
        {
            return -1;
        }

        // Write RTP header.
        // Set marker bit true if this is the last packet in frame.
        _rtpSender.BuildRTPheader(dataBuffer, payloadType, last,
            captureTimeStamp, capture_time_ms);
        if (-1 == SendVideoPacket(dataBuffer, payloadBytesInPacket,
                                  rtpHeaderLength, captureTimeStamp,
                                  capture_time_ms, storage, protect))
        {
          WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                       "RTPSenderVideo::SendVP8 failed to send packet number"
                       " %d", _rtpSender.SequenceNumber());
        }
    }
    TRACE_EVENT_ASYNC_END1("webrtc", "Video", capture_time_ms,
                           "timestamp", _rtpSender.Timestamp());
    return 0;
}

void RTPSenderVideo::ProcessBitrate() {
  _videoBitrate.Process();
  _fecOverheadRate.Process();
}

uint32_t RTPSenderVideo::VideoBitrateSent() const {
  return _videoBitrate.BitrateLast();
}

uint32_t RTPSenderVideo::FecOverheadRate() const {
  return _fecOverheadRate.BitrateLast();
}

int RTPSenderVideo::SelectiveRetransmissions() const {
  return _retransmissionSettings;
}

int RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  _retransmissionSettings = settings;
  return 0;
}

}  // namespace webrtc
