// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_raw.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

class LoggingRawTest : public ::testing::Test {
 protected:
  LoggingRawTest() {
    raw_.AddSubscriber(&event_subscriber_);
  }

  virtual ~LoggingRawTest() { raw_.RemoveSubscriber(&event_subscriber_); }

  LoggingRaw raw_;
  SimpleEventSubscriber event_subscriber_;
  std::vector<FrameEvent> frame_events_;
  std::vector<PacketEvent> packet_events_;
  std::vector<GenericEvent> generic_events_;
};

TEST_F(LoggingRawTest, FrameEvent) {
  CastLoggingEvent event_type = kVideoFrameDecoded;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, rtp_timestamp, frame_id);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);
}

TEST_F(LoggingRawTest, FrameEventWithSize) {
  CastLoggingEvent event_type = kVideoFrameEncoded;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  int size = 1024;
  raw_.InsertFrameEventWithSize(timestamp, event_type, rtp_timestamp, frame_id,
                                size);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(size, static_cast<int>(frame_events_[0].size));
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);
}

TEST_F(LoggingRawTest, FrameEventWithDelay) {
  CastLoggingEvent event_type = kVideoRenderDelay;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(20);
  raw_.InsertFrameEventWithDelay(timestamp, event_type, rtp_timestamp, frame_id,
                                 delay);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(delay, frame_events_[0].delay_delta);
}

TEST_F(LoggingRawTest, PacketEvent) {
  CastLoggingEvent event_type = kVideoPacketReceived;
  uint32 frame_id = 456u;
  uint16 packet_id = 1u;
  uint16 max_packet_id = 10u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  size_t size = 1024u;
  raw_.InsertPacketEvent(timestamp, event_type, rtp_timestamp, frame_id,
                         packet_id, max_packet_id, size);

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  EXPECT_TRUE(frame_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  ASSERT_EQ(1u, packet_events_.size());
  EXPECT_EQ(rtp_timestamp, packet_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, packet_events_[0].frame_id);
  EXPECT_EQ(max_packet_id, packet_events_[0].max_packet_id);
  EXPECT_EQ(packet_id, packet_events_[0].packet_id);
  EXPECT_EQ(size, packet_events_[0].size);
  EXPECT_EQ(timestamp, packet_events_[0].timestamp);
  EXPECT_EQ(event_type, packet_events_[0].type);
}

TEST_F(LoggingRawTest, GenericEvent) {
  CastLoggingEvent event_type = kRttMs;
  base::TimeTicks timestamp = base::TimeTicks();
  int value = 100;
  raw_.InsertGenericEvent(timestamp, event_type, value);

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  EXPECT_TRUE(frame_events_.empty());

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  ASSERT_EQ(1u, generic_events_.size());
  EXPECT_EQ(event_type, generic_events_[0].type);
  EXPECT_EQ(value, generic_events_[0].value);
  EXPECT_EQ(timestamp, generic_events_[0].timestamp);
}

TEST_F(LoggingRawTest, MultipleSubscribers) {
  SimpleEventSubscriber event_subscriber_2;

  // Now raw_ has two subscribers.
  raw_.AddSubscriber(&event_subscriber_2);

  CastLoggingEvent event_type = kVideoFrameDecoded;
  uint32 frame_id = 456u;
  RtpTimestamp rtp_timestamp = 123u;
  base::TimeTicks timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, rtp_timestamp, frame_id);

  event_subscriber_.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  event_subscriber_2.GetPacketEventsAndReset(&packet_events_);
  EXPECT_TRUE(packet_events_.empty());

  event_subscriber_2.GetGenericEventsAndReset(&generic_events_);
  EXPECT_TRUE(generic_events_.empty());

  event_subscriber_2.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  // Remove event_subscriber_2, so it shouldn't receive events after this.
  raw_.RemoveSubscriber(&event_subscriber_2);

  event_type = kAudioFrameDecoded;
  frame_id = 789;
  rtp_timestamp = 456;
  timestamp = base::TimeTicks();
  raw_.InsertFrameEvent(timestamp, event_type, rtp_timestamp, frame_id);

  // |event_subscriber_| should still receive events.
  event_subscriber_.GetFrameEventsAndReset(&frame_events_);
  ASSERT_EQ(1u, frame_events_.size());
  EXPECT_EQ(rtp_timestamp, frame_events_[0].rtp_timestamp);
  EXPECT_EQ(frame_id, frame_events_[0].frame_id);
  EXPECT_EQ(0u, frame_events_[0].size);
  EXPECT_EQ(timestamp, frame_events_[0].timestamp);
  EXPECT_EQ(event_type, frame_events_[0].type);
  EXPECT_EQ(base::TimeDelta(), frame_events_[0].delay_delta);

  event_subscriber_2.GetFrameEventsAndReset(&frame_events_);
  EXPECT_TRUE(frame_events_.empty());
}

}  // namespace cast
}  // namespace media
