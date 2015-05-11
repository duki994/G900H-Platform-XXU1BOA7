/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_

#include <stddef.h> // size_t, ptrdiff_t

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/typedefs.h"

namespace webrtc {

const uint8_t kRtpMarkerBitMask = 0x80;

RtpData* NullObjectRtpData();
RtpFeedback* NullObjectRtpFeedback();
RtpAudioFeedback* NullObjectRtpAudioFeedback();
ReceiveStatistics* NullObjectReceiveStatistics();

namespace ModuleRTPUtility
{
    // January 1970, in NTP seconds.
    const uint32_t NTP_JAN_1970 = 2208988800UL;

    // Magic NTP fractional unit.
    const double NTP_FRAC = 4.294967296E+9;

    struct Payload
    {
        char name[RTP_PAYLOAD_NAME_SIZE];
        bool audio;
        PayloadUnion typeSpecific;
    };

    typedef std::map<int8_t, Payload*> PayloadTypeMap;

    // Return the current RTP timestamp from the NTP timestamp
    // returned by the specified clock.
    uint32_t GetCurrentRTP(Clock* clock, uint32_t freq);

    // Return the current RTP absolute timestamp.
    uint32_t ConvertNTPTimeToRTP(uint32_t NTPsec,
                                 uint32_t NTPfrac,
                                 uint32_t freq);

    uint32_t pow2(uint8_t exp);

    // Returns true if |newTimestamp| is older than |existingTimestamp|.
    // |wrapped| will be set to true if there has been a wraparound between the
    // two timestamps.
    bool OldTimestamp(uint32_t newTimestamp,
                      uint32_t existingTimestamp,
                      bool* wrapped);

    bool StringCompare(const char* str1,
                       const char* str2,
                       const uint32_t length);

    void AssignUWord32ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord24ToBuffer(uint8_t* dataBuffer, uint32_t value);
    void AssignUWord16ToBuffer(uint8_t* dataBuffer, uint16_t value);

    /**
     * Converts a network-ordered two-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered two-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint16_t BufferToUWord16(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered three-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered three-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord24(const uint8_t* dataBuffer);

    /**
     * Converts a network-ordered four-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered four-byte buffer to convert.
     * \return Host-ordered value.
     */
    uint32_t BufferToUWord32(const uint8_t* dataBuffer);

    class RTPHeaderParser
    {
    public:
        RTPHeaderParser(const uint8_t* rtpData,
                        const uint32_t rtpDataLength);
        ~RTPHeaderParser();

        bool RTCP() const;
        bool ParseRtcp(RTPHeader* header) const;
        bool Parse(RTPHeader& parsedPacket,
                   RtpHeaderExtensionMap* ptrExtensionMap = NULL) const;

    private:
        void ParseOneByteExtensionHeader(
            RTPHeader& parsedPacket,
            const RtpHeaderExtensionMap* ptrExtensionMap,
            const uint8_t* ptrRTPDataExtensionEnd,
            const uint8_t* ptr) const;

        uint8_t ParsePaddingBytes(
            const uint8_t* ptrRTPDataExtensionEnd,
            const uint8_t* ptr) const;

        const uint8_t* const _ptrRTPDataBegin;
        const uint8_t* const _ptrRTPDataEnd;
    };

    enum FrameTypes
    {
        kIFrame,    // key frame
        kPFrame         // Delta frame
    };

#if defined(ENABLE_WEBRTC_H264_CODEC)
    enum
    {
        H264_FRAME_INVALID = 0x0,
        H264_FRAME_BEGIN = 0x1,
        H264_FRAME_PART = 0x2,
        H264_FRAME_END = 0x4,
    };

    typedef enum
    {
        APPEND_INVALID = 0,
        APPEND_H264_ANNEXB, //Append H264 annexB header
        APPEND_MAX = 2
    } HeaderAppendType;

    struct RTPPayloadH264
    {
        bool                frameValid;
        bool                frameMarker;
        bool                markerBit;
        bool                appendCodecSpecificHeader;
        bool                isFirstPacket;
        uint32_t            frameTimeStamp;
        int32_t            packetSequenceNumber;
        uint64_t            framePlayoutTime;
        int32_t            frameWidth;
        int32_t            frameHeight;
        const uint8_t*            data;
        uint16_t            dataLength;
        bool            startBit;
    };
#endif

    struct RTPPayloadVP8
    {
        bool                 nonReferenceFrame;
        bool                 beginningOfPartition;
        int                  partitionID;
        bool                 hasPictureID;
        bool                 hasTl0PicIdx;
        bool                 hasTID;
        bool                 hasKeyIdx;
        int                  pictureID;
        int                  tl0PicIdx;
        int                  tID;
        bool                 layerSync;
        int                  keyIdx;
        int                  frameWidth;
        int                  frameHeight;

        const uint8_t*   data;
        uint16_t         dataLength;
    };

    union RTPPayloadUnion
    {
#if defined(ENABLE_WEBRTC_H264_CODEC)
        RTPPayloadH264   H264;
#endif
        RTPPayloadVP8   VP8;
    };

    struct RTPPayload
    {
        void SetType(RtpVideoCodecTypes videoType);

        RtpVideoCodecTypes  type;
        FrameTypes          frameType;
        RTPPayloadUnion     info;
    };

#if defined(ENABLE_WEBRTC_H264_CODEC)
    /* Data structure for FU-A */
    typedef struct
    {
        bool     s_bit;
        bool     e_bit;
        bool     r_bit;
        uint8_t    nal_unit_payload_type;
    } nal_unit_fu;

    /* Union which will support in future all other NAL Unit types defined in RFC 3984 */
    typedef union
    {
        nal_unit_fu           fu_nal_unit;
        // in structure type II
    } nal_unit;

    /* Data structure for all types of packets */
    typedef struct
    {
        bool     f_bit;     // Forbidden zero bit (1 bit).
        uint8_t     nal_ref_id;     // NAL reference idc. 00 indicates not used to
        // reconstruct reference pictures for inter picture
        // prediction(2 bits).
        uint8_t     nal_unit_payload_type;     // nal_unit_type of 5 bits.
        uint32_t     don;     // Decoding Order Number - which will be used in
        // case of Interleaved mode.
        nal_unit     u_nalu;
        bool     start_detected;
    } H264ExtnHdrParseInfo;
#endif

    // RTP payload parser
    class RTPPayloadParser
    {
    public:
        RTPPayloadParser(const RtpVideoCodecTypes payloadType,
                         const uint8_t* payloadData,
                         const uint16_t payloadDataLength, // Length w/o padding.
                         const int32_t id);

        ~RTPPayloadParser();

        bool Parse(RTPPayload& parsedPacket) const;

    private:
        bool ParseGeneric(RTPPayload& parsedPacket) const;

#if defined(ENABLE_WEBRTC_H264_CODEC)
        bool ParseH264(RTPPayload& parsedPacket) const;

        int32_t ParseH264PayloadHdr(uint8_t* dataPtr,
                              H264ExtnHdrParseInfo* currentFrameH264Hdr) const;
#endif
        bool ParseVP8(RTPPayload& parsedPacket) const;

        int ParseVP8Extension(RTPPayloadVP8 *vp8,
                              const uint8_t *dataPtr,
                              int dataLength) const;

        int ParseVP8PictureID(RTPPayloadVP8 *vp8,
                              const uint8_t **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8Tl0PicIdx(RTPPayloadVP8 *vp8,
                              const uint8_t **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8TIDAndKeyIdx(RTPPayloadVP8 *vp8,
                                 const uint8_t **dataPtr,
                                 int *dataLength,
                                 int *parsedBytes) const;

        int ParseVP8FrameSize(RTPPayload& parsedPacket,
                              const uint8_t *dataPtr,
                              int dataLength) const;

    private:
        int32_t               _id;
        const uint8_t*        _dataPtr;
        const uint16_t        _dataLength;
        const RtpVideoCodecTypes    _videoType;
    };

}  // namespace ModuleRTPUtility

}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
