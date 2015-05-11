// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/audio_sender/audio_encoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_defines.h"
#include "media/cast/cast_environment.h"
#include "third_party/opus/src/include/opus.h"

namespace media {
namespace cast {

// Base class that handles the common problem of feeding one or more AudioBus'
// data into a 10 ms buffer and then, once the buffer is full, encoding the
// signal and emitting an EncodedAudioFrame via the FrameEncodedCallback.
//
// Subclasses complete the implementation by handling the actual encoding
// details.
class AudioEncoder::ImplBase : public base::SupportsWeakPtr<ImplBase> {
 public:
  ImplBase(CastEnvironment* cast_environment,
           transport::AudioCodec codec,
           int num_channels,
           int sampling_rate,
           const FrameEncodedCallback& callback)
      : cast_environment_(cast_environment),
        codec_(codec),
        num_channels_(num_channels),
        samples_per_10ms_(sampling_rate / 100),
        callback_(callback),
        buffer_fill_end_(0),
        frame_id_(0),
        rtp_timestamp_(0),
        weak_factory_(this) {
    DCHECK_GT(num_channels_, 0);
    DCHECK_GT(samples_per_10ms_, 0);
    DCHECK_EQ(sampling_rate % 100, 0);
    DCHECK_LE(samples_per_10ms_ * num_channels_,
              transport::EncodedAudioFrame::kMaxNumberOfSamples);

    if (num_channels_ <= 0 || samples_per_10ms_ <= 0 ||
        sampling_rate % 100 != 0 ||
        samples_per_10ms_ * num_channels_ >
            transport::EncodedAudioFrame::kMaxNumberOfSamples) {
      initialization_status_ = STATUS_INVALID_AUDIO_CONFIGURATION;
    } else {
      initialization_status_ = STATUS_INITIALIZED;
    }
  }

  virtual ~ImplBase() {}

  CastInitializationStatus InitializationResult() const {
    return initialization_status_;
  }

  void LogAudioFrameEvent(uint32 rtp_timestamp,
                          uint32 frame_id,
                          CastLoggingEvent type) {
    cast_environment_->Logging()->InsertFrameEvent(
        cast_environment_->Clock()->NowTicks(), type, rtp_timestamp, frame_id);
  }

  void EncodeAudio(const AudioBus* audio_bus,
                   const base::TimeTicks& recorded_time,
                   const base::Closure& done_callback) {
    int src_pos = 0;
    int packet_count = 0;
    while (audio_bus && src_pos < audio_bus->frames()) {
      const int num_samples_to_xfer = std::min(
          samples_per_10ms_ - buffer_fill_end_, audio_bus->frames() - src_pos);
      DCHECK_EQ(audio_bus->channels(), num_channels_);
      TransferSamplesIntoBuffer(
          audio_bus, src_pos, buffer_fill_end_, num_samples_to_xfer);
      src_pos += num_samples_to_xfer;
      buffer_fill_end_ += num_samples_to_xfer;

      if (src_pos == audio_bus->frames()) {
        cast_environment_->PostTask(
            CastEnvironment::MAIN, FROM_HERE, done_callback);
        // Note: |audio_bus| is invalid once done_callback is invoked.
        audio_bus = NULL;
      }

      if (buffer_fill_end_ == samples_per_10ms_) {
        scoped_ptr<transport::EncodedAudioFrame> audio_frame(
            new transport::EncodedAudioFrame());
        audio_frame->codec = codec_;
        audio_frame->frame_id = frame_id_++;
        rtp_timestamp_ += samples_per_10ms_;
        audio_frame->rtp_timestamp = rtp_timestamp_;

        // Update logging.
        cast_environment_->PostTask(CastEnvironment::MAIN,
                                    FROM_HERE,
                                    base::Bind(&ImplBase::LogAudioFrameEvent,
                                               weak_factory_.GetWeakPtr(),
                                               audio_frame->rtp_timestamp,
                                               audio_frame->frame_id,
                                               kAudioFrameReceived));

        if (EncodeFromFilledBuffer(&audio_frame->data)) {
          // Update logging.
          cast_environment_->PostTask(CastEnvironment::MAIN,
                                      FROM_HERE,
                                      base::Bind(&ImplBase::LogAudioFrameEvent,
                                                 weak_factory_.GetWeakPtr(),
                                                 audio_frame->rtp_timestamp,
                                                 audio_frame->frame_id,
                                                 kAudioFrameEncoded));
          // Compute an offset to determine the recorded time for the first
          // audio sample in the buffer.
          const base::TimeDelta buffer_time_offset =
              (buffer_fill_end_ - src_pos) *
              base::TimeDelta::FromMilliseconds(10) / samples_per_10ms_;
          // TODO(miu): Consider batching EncodedAudioFrames so we only post a
          // at most one task for each call to this method.
          // Postpone every packet by 10mS with respect to the previous. Playout
          // is postponed already by 10mS, and this will better correlate with
          // the pacer's expectations.
          //TODO(mikhal): Turn this into a list of packets.
          // Update the end2end allowed error once this is fixed.
          cast_environment_->PostDelayedTask(
              CastEnvironment::MAIN,
              FROM_HERE,
              base::Bind(callback_,
                         base::Passed(&audio_frame),
                         recorded_time - buffer_time_offset),
              base::TimeDelta::FromMilliseconds(packet_count * 10));
          ++packet_count;
        }
        buffer_fill_end_ = 0;
      }
    }
  }

 protected:
  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) = 0;
  virtual bool EncodeFromFilledBuffer(std::string* out) = 0;

  CastEnvironment* const cast_environment_;
  const transport::AudioCodec codec_;
  const int num_channels_;
  const int samples_per_10ms_;
  const FrameEncodedCallback callback_;
  CastInitializationStatus initialization_status_;

 private:
  // In the case where a call to EncodeAudio() cannot completely fill the
  // buffer, this points to the position at which to populate data in a later
  // call.
  int buffer_fill_end_;

  // A counter used to label EncodedAudioFrames.
  uint32 frame_id_;

  // For audio, rtp_timestamp is computed as the sum of the audio samples seen
  // so far.
  uint32 rtp_timestamp_;

  base::WeakPtrFactory<ImplBase> weak_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImplBase);
};

class AudioEncoder::OpusImpl : public AudioEncoder::ImplBase {
 public:
  OpusImpl(CastEnvironment* cast_environment,
           int num_channels,
           int sampling_rate,
           int bitrate,
           const FrameEncodedCallback& callback)
      : ImplBase(cast_environment,
                 transport::kOpus,
                 num_channels,
                 sampling_rate,
                 callback),
        encoder_memory_(new uint8[opus_encoder_get_size(num_channels)]),
        opus_encoder_(reinterpret_cast<OpusEncoder*>(encoder_memory_.get())),
        buffer_(new float[num_channels * samples_per_10ms_]) {
    if (ImplBase::initialization_status_ != STATUS_INITIALIZED) {
      return;
    }

    CHECK_EQ(
        opus_encoder_init(
            opus_encoder_, sampling_rate, num_channels, OPUS_APPLICATION_AUDIO),
        OPUS_OK);
    if (bitrate <= 0) {
      // Note: As of 2013-10-31, the encoder in "auto bitrate" mode would use a
      // variable bitrate up to 102kbps for 2-channel, 48 kHz audio and a 10 ms
      // frame size.  The opus library authors may, of course, adjust this in
      // later versions.
      bitrate = OPUS_AUTO;
    }
    CHECK_EQ(opus_encoder_ctl(opus_encoder_, OPUS_SET_BITRATE(bitrate)),
             OPUS_OK);
  }

  virtual ~OpusImpl() {}

 private:
  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) OVERRIDE {
    // Opus requires channel-interleaved samples in a single array.
    for (int ch = 0; ch < audio_bus->channels(); ++ch) {
      const float* src = audio_bus->channel(ch) + source_offset;
      const float* const src_end = src + num_samples;
      float* dest = buffer_.get() + buffer_fill_offset * num_channels_ + ch;
      for (; src < src_end; ++src, dest += num_channels_)
        *dest = *src;
    }
  }

  virtual bool EncodeFromFilledBuffer(std::string* out) OVERRIDE {
    out->resize(kOpusMaxPayloadSize);
    const opus_int32 result =
        opus_encode_float(opus_encoder_,
                          buffer_.get(),
                          samples_per_10ms_,
                          reinterpret_cast<uint8*>(&out->at(0)),
                          kOpusMaxPayloadSize);
    if (result > 1) {
      out->resize(result);
      return true;
    } else if (result < 0) {
      LOG(ERROR) << "Error code from opus_encode_float(): " << result;
      return false;
    } else {
      // Do nothing: The documentation says that a return value of zero or
      // one byte means the packet does not need to be transmitted.
      return false;
    }
  }

  const scoped_ptr<uint8[]> encoder_memory_;
  OpusEncoder* const opus_encoder_;
  const scoped_ptr<float[]> buffer_;

  // This is the recommended value, according to documentation in
  // third_party/opus/src/include/opus.h, so that the Opus encoder does not
  // degrade the audio due to memory constraints.
  //
  // Note: Whereas other RTP implementations do not, the cast library is
  // perfectly capable of transporting larger than MTU-sized audio frames.
  static const int kOpusMaxPayloadSize = 4000;

  DISALLOW_COPY_AND_ASSIGN(OpusImpl);
};

class AudioEncoder::Pcm16Impl : public AudioEncoder::ImplBase {
 public:
  Pcm16Impl(CastEnvironment* cast_environment,
            int num_channels,
            int sampling_rate,
            const FrameEncodedCallback& callback)
      : ImplBase(cast_environment,
                 transport::kPcm16,
                 num_channels,
                 sampling_rate,
                 callback),
        buffer_(new int16[num_channels * samples_per_10ms_]) {}

  virtual ~Pcm16Impl() {}

 private:
  virtual void TransferSamplesIntoBuffer(const AudioBus* audio_bus,
                                         int source_offset,
                                         int buffer_fill_offset,
                                         int num_samples) OVERRIDE {
    audio_bus->ToInterleavedPartial(
        source_offset,
        num_samples,
        sizeof(int16),
        buffer_.get() + buffer_fill_offset * num_channels_);
  }

  virtual bool EncodeFromFilledBuffer(std::string* out) OVERRIDE {
    // Output 16-bit PCM integers in big-endian byte order.
    out->resize(num_channels_ * samples_per_10ms_ * sizeof(int16));
    const int16* src = buffer_.get();
    const int16* const src_end = src + num_channels_ * samples_per_10ms_;
    uint16* dest = reinterpret_cast<uint16*>(&out->at(0));
    for (; src < src_end; ++src, ++dest)
      *dest = base::HostToNet16(*src);
    return true;
  }

 private:
  const scoped_ptr<int16[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(Pcm16Impl);
};

AudioEncoder::AudioEncoder(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const AudioSenderConfig& audio_config,
    const FrameEncodedCallback& frame_encoded_callback)
    : cast_environment_(cast_environment) {
  // Note: It doesn't matter which thread constructs AudioEncoder, just so long
  // as all calls to InsertAudio() are by the same thread.
  insert_thread_checker_.DetachFromThread();

  switch (audio_config.codec) {
    case transport::kOpus:
      impl_.reset(new OpusImpl(cast_environment,
                               audio_config.channels,
                               audio_config.frequency,
                               audio_config.bitrate,
                               frame_encoded_callback));
      break;
    case transport::kPcm16:
      impl_.reset(new Pcm16Impl(cast_environment,
                                audio_config.channels,
                                audio_config.frequency,
                                frame_encoded_callback));
      break;
    default:
      NOTREACHED() << "Unsupported or unspecified codec for audio encoder";
      break;
  }
}

AudioEncoder::~AudioEncoder() {}

CastInitializationStatus AudioEncoder::InitializationResult() const {
  if (impl_) {
    return impl_->InitializationResult();
  }
  return STATUS_UNSUPPORTED_AUDIO_CODEC;
}

void AudioEncoder::InsertAudio(const AudioBus* audio_bus,
                               const base::TimeTicks& recorded_time,
                               const base::Closure& done_callback) {
  DCHECK(insert_thread_checker_.CalledOnValidThread());
  if (!impl_) {
    NOTREACHED();
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE, done_callback);
    return;
  }
  cast_environment_->PostTask(CastEnvironment::AUDIO_ENCODER,
                              FROM_HERE,
                              base::Bind(&AudioEncoder::EncodeAudio,
                                         this,
                                         audio_bus,
                                         recorded_time,
                                         done_callback));
}

void AudioEncoder::EncodeAudio(const AudioBus* audio_bus,
                               const base::TimeTicks& recorded_time,
                               const base::Closure& done_callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::AUDIO_ENCODER));
  impl_->EncodeAudio(audio_bus, recorded_time, done_callback);
}

}  // namespace cast
}  // namespace media
