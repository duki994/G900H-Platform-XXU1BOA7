// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_

#include <list>
#include <queue>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/tuple.h"
#include "content/common/content_export.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
class BitstreamBuffer;
}  // namespace media

namespace content {

// Android-specific implementation of media::VideoEncodeAccelerator, enabling
// hardware-acceleration of video encoding, based on Android's MediaCodec class
// (http://developer.android.com/reference/android/media/MediaCodec.html).  This
// class expects to live and be called on a single thread (the GPU process'
// ChildThread).
class CONTENT_EXPORT AndroidVideoEncodeAccelerator
    : public media::VideoEncodeAccelerator {
 public:
  explicit AndroidVideoEncodeAccelerator(
      media::VideoEncodeAccelerator::Client* client);
  virtual ~AndroidVideoEncodeAccelerator();

  static std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      GetSupportedProfiles();

  // media::VideoEncodeAccelerator implementation.
  virtual void Initialize(media::VideoFrame::Format format,
                          const gfx::Size& input_visible_size,
                          media::VideoCodecProfile output_profile,
                          uint32 initial_bitrate) OVERRIDE;
  virtual void Encode(const scoped_refptr<media::VideoFrame>& frame,
                      bool force_keyframe) OVERRIDE;
  virtual void UseOutputBitstreamBuffer(const media::BitstreamBuffer& buffer)
      OVERRIDE;
  virtual void RequestEncodingParametersChange(uint32 bitrate,
                                               uint32 framerate) OVERRIDE;
  virtual void Destroy() OVERRIDE;

 private:
  enum {
    // Arbitrary choice.
    INITIAL_FRAMERATE = 30,
#if defined(ENABLE_WEBRTC_H264_CODEC)
    // The value for bitrate is calculated using the formula
    // [imsage width] x [image heigth] x [framerate] x [motion rank] x 0.07
    // For the current scenario it is 640 x 480 x 30 x 2 x 0.07.
    INITIAL_H264_BITRATE = 2000000,
    // Until there are non-realtime users, no need for unrequested I-frames.
    IFRAME_H264_INTERVAL = 1,
    IFRAME_VP8_INTERVAL = kint32max,
#else
    // Until there are non-realtime users, no need for unrequested I-frames.
    IFRAME_INTERVAL = kint32max,
#endif
  };

  // Impedance-mismatch fixers: MediaCodec is a poll-based API but VEA is a
  // push-based API; these methods turn the crank to make the two work together.
  void DoIOTask();
  void QueueInput();
  void DequeueOutput();
#if defined(ENABLE_WEBRTC_H264_CODEC)
  void SendSpsPpsData(bool key_frame);
#endif
  // Returns true if we don't need more or bigger output buffers.
  bool DoOutputBuffersSuffice();

  // Start & stop |io_timer_| if the time seems right.
  void MaybeStartIOTimer();
  void MaybeStopIOTimer();

  // Used to DCHECK that we are called on the correct thread.
  base::ThreadChecker thread_checker_;

  // VideoDecodeAccelerator::Client callbacks go here.  Invalidated once any
  // error triggers.
  base::WeakPtrFactory<Client> client_ptr_factory_;

  scoped_ptr<media::VideoCodecBridge> media_codec_;

  // Bitstream buffers waiting to be populated & returned to the client.
  std::vector<media::BitstreamBuffer> available_bitstream_buffers_;

  // Frames waiting to be passed to the codec, queued until an input buffer is
  // available.  Each element is a tuple of <Frame, key_frame, enqueue_time>.
  typedef std::queue<
      Tuple3<scoped_refptr<media::VideoFrame>, bool, base::Time> >
      PendingFrames;
  PendingFrames pending_frames_;

  // Repeating timer responsible for draining pending IO to the codec.
  base::RepeatingTimer<AndroidVideoEncodeAccelerator> io_timer_;

  // The difference between number of buffers queued & dequeued at the codec.
  int32 num_buffers_at_codec_;

  // A monotonically-growing value, used as a fake timestamp just to keep things
  // appearing to move forward.
  base::TimeDelta fake_input_timestamp_;

  // Number of requested output buffers and their capacity.
  int num_output_buffers_;          // -1 until RequireBitstreamBuffers.
  size_t output_buffers_capacity_;  // 0 until RequireBitstreamBuffers.

#if defined(ENABLE_WEBRTC_H264_CODEC)
  uint8_t* h264_sps_pps_buffer;
  uint32 h264_sps_pps_size;
  media::VideoCodecProfile output_profile_;
#endif

  uint32 last_set_bitrate_;  // In bps.

  DISALLOW_COPY_AND_ASSIGN(AndroidVideoEncodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_VIDEO_ENCODE_ACCELERATOR_H_
