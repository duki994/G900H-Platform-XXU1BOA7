// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAST_LOGGING_LOGGING_IMPL_H_
#define MEDIA_CAST_LOGGING_LOGGING_IMPL_H_

// Generic class that handles event logging for the cast library.
// Logging has three possible optional forms:
// 1. Raw data and stats accessible by the application.
// 2. Tracing of raw events.

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "media/cast/cast_config.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/logging_raw.h"
#include "media/cast/logging/logging_stats.h"

namespace media {
namespace cast {

// Should only be called from the main thread.
class LoggingImpl : public base::NonThreadSafe {
 public:
  LoggingImpl(scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy,
              const CastLoggingConfig& config);

  ~LoggingImpl();

  void InsertFrameEvent(const base::TimeTicks& time_of_event,
                        CastLoggingEvent event, uint32 rtp_timestamp,
                        uint32 frame_id);

  void InsertFrameEventWithSize(const base::TimeTicks& time_of_event,
                                CastLoggingEvent event, uint32 rtp_timestamp,
                                uint32 frame_id, int frame_size);

  void InsertFrameEventWithDelay(const base::TimeTicks& time_of_event,
                                 CastLoggingEvent event, uint32 rtp_timestamp,
                                 uint32 frame_id, base::TimeDelta delay);

  void InsertPacketListEvent(const base::TimeTicks& time_of_event,
                             CastLoggingEvent event, const PacketList& packets);

  void InsertPacketEvent(const base::TimeTicks& time_of_event,
                         CastLoggingEvent event, uint32 rtp_timestamp,
                         uint32 frame_id, uint16 packet_id,
                         uint16 max_packet_id, size_t size);

  void InsertGenericEvent(const base::TimeTicks& time_of_event,
                          CastLoggingEvent event, int value);


  // Delegates to |LoggingRaw::AddRawEventSubscriber()|.
  void AddRawEventSubscriber(RawEventSubscriber* subscriber);

  // Delegates to |LoggingRaw::RemoveRawEventSubscriber()|.
  void RemoveRawEventSubscriber(RawEventSubscriber* subscriber);

  // Get stats only.
  FrameStatsMap GetFrameStatsData() const;
  PacketStatsMap GetPacketStatsData() const;
  GenericStatsMap GetGenericStatsData() const;

  // Reset stats logging data.
  void ResetStats();

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_proxy_;
  const CastLoggingConfig config_;
  LoggingRaw raw_;
  LoggingStats stats_;

  DISALLOW_COPY_AND_ASSIGN(LoggingImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOGGING_IMPL_H_
