// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/receiver_rtcp_event_subscriber.h"

#include <utility>

namespace media {
namespace cast {

ReceiverRtcpEventSubscriber::ReceiverRtcpEventSubscriber(
    const size_t max_size_to_retain, Type type)
    : max_size_to_retain_(max_size_to_retain), type_(type) {
  DCHECK(max_size_to_retain_ > 0u);
  DCHECK(type_ == kAudioEventSubscriber || type_ == kVideoEventSubscriber);
}

ReceiverRtcpEventSubscriber::~ReceiverRtcpEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ReceiverRtcpEventSubscriber::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(frame_event.type)) {
    RtcpEvent rtcp_event;
    switch (frame_event.type) {
      case kAudioPlayoutDelay:
      case kVideoRenderDelay:
        rtcp_event.delay_delta = frame_event.delay_delta;
      case kAudioFrameDecoded:
      case kVideoFrameDecoded:
      // TODO(imcheng): This doesn't seem correct because kAudioAckSent and
      // kVideoAckSent logged as generic events in AudioReceiver /
      // VideoReceiver. (crbug.com/339590)
      case kAudioAckSent:
      case kVideoAckSent:
        rtcp_event.type = frame_event.type;
        rtcp_event.timestamp = frame_event.timestamp;
        rtcp_events_.insert(
            std::make_pair(frame_event.rtp_timestamp, rtcp_event));
        break;
      default:
        break;
    }
  }

  TruncateMapIfNeeded();

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

void ReceiverRtcpEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ShouldProcessEvent(packet_event.type)) {
    RtcpEvent rtcp_event;
    if (packet_event.type == kAudioPacketReceived ||
        packet_event.type == kVideoPacketReceived) {
      rtcp_event.type = packet_event.type;
      rtcp_event.timestamp = packet_event.timestamp;
      rtcp_event.packet_id = packet_event.packet_id;
      rtcp_events_.insert(
          std::make_pair(packet_event.rtp_timestamp, rtcp_event));
    }
  }

  TruncateMapIfNeeded();

  DCHECK(rtcp_events_.size() <= max_size_to_retain_);
}

void ReceiverRtcpEventSubscriber::OnReceiveGenericEvent(
    const GenericEvent& generic_event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Do nothing as RTP receiver is not interested in generic events for RTCP.
}

void ReceiverRtcpEventSubscriber::GetReceiverLogMessageAndReset(
    RtcpReceiverLogMessage* receiver_log) {
  DCHECK(thread_checker_.CalledOnValidThread());

  receiver_log->clear();

  typedef std::multimap<RtpTimestamp, RtcpEvent> RtcpEventMultiMap;
  RtcpEventMultiMap::const_iterator it = rtcp_events_.begin();
  while (it != rtcp_events_.end()) {
    // On each iteration, process all entries that have the same key (RTP
    // timestamp) within the multimap, and generate a
    // RtcpReceiverFrameLogMessage from them.
    RtpTimestamp rtp_timestamp = it->first;
    RtcpReceiverFrameLogMessage frame_log(rtp_timestamp);
    do {
      RtcpReceiverEventLogMessage event_log_message;
      event_log_message.type = it->second.type;
      event_log_message.event_timestamp = it->second.timestamp;
      event_log_message.delay_delta = it->second.delay_delta;
      event_log_message.packet_id = it->second.packet_id;
      frame_log.event_log_messages_.push_back(event_log_message);
      ++it;
    } while (it != rtcp_events_.end() && it->first == rtp_timestamp);

    receiver_log->push_back(frame_log);
  }

  rtcp_events_.clear();
}

void ReceiverRtcpEventSubscriber::TruncateMapIfNeeded() {
  // If map size has exceeded |max_size_to_retain_|, remove entry with
  // the smallest RTP timestamp.
  if (rtcp_events_.size() > max_size_to_retain_) {
    DVLOG(2) << "RTCP event map exceeded size limit; "
             << "removing oldest entry";
    // This is fine since we only insert elements one at a time.
    rtcp_events_.erase(rtcp_events_.begin());
  }
}

bool ReceiverRtcpEventSubscriber::ShouldProcessEvent(
    CastLoggingEvent event_type) {
  if (type_ == kAudioEventSubscriber) {
    return event_type == kAudioPlayoutDelay ||
           event_type == kAudioFrameDecoded || event_type == kAudioAckSent ||
           event_type == kAudioPacketReceived;
  } else if (type_ == kVideoEventSubscriber) {
    return event_type == kVideoRenderDelay ||
           event_type == kVideoFrameDecoded || event_type == kVideoAckSent ||
           event_type == kVideoPacketReceived;
  } else {
    return false;
  }
}

}  // namespace cast
}  // namespace media
