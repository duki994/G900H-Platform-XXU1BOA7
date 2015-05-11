// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/libjingle/source/talk/media/base/videorenderer.h"

namespace content {

class WebAudioCapturerSource;

class MockVideoRenderer : public cricket::VideoRenderer {
 public:
  MockVideoRenderer();
  virtual ~MockVideoRenderer();
  virtual bool SetSize(int width, int height, int reserved) OVERRIDE;
  virtual bool RenderFrame(const cricket::VideoFrame* frame) OVERRIDE;

  int width() const { return width_; }
  int height() const { return height_; }
  int num() const { return num_; }

 private:
  int width_;
  int height_;
  int num_;
};

class MockVideoSource : public webrtc::VideoSourceInterface {
 public:
  MockVideoSource();

  virtual void RegisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual MediaSourceInterface::SourceState state() const OVERRIDE;
  virtual cricket::VideoCapturer* GetVideoCapturer() OVERRIDE;
  virtual void AddSink(cricket::VideoRenderer* output) OVERRIDE;
  virtual void RemoveSink(cricket::VideoRenderer* output) OVERRIDE;
  virtual cricket::VideoRenderer* FrameInput() OVERRIDE;
  virtual const cricket::VideoOptions* options() const OVERRIDE;

  // Changes the state of the source to live and notifies the observer.
  void SetLive();
  // Changes the state of the source to ended and notifies the observer.
  void SetEnded();
  // Set the video capturer.
  void SetVideoCapturer(cricket::VideoCapturer* capturer);

  // Test helpers.
  int GetLastFrameWidth() const { return renderer_.width(); }
  int GetLastFrameHeight() const { return renderer_.height(); }
  int GetFrameNum() const { return renderer_.num(); }

 protected:
  virtual ~MockVideoSource();

 private:
  void FireOnChanged();

  std::vector<webrtc::ObserverInterface*> observers_;
  MediaSourceInterface::SourceState state_;
  scoped_ptr<cricket::VideoCapturer> capturer_;
  MockVideoRenderer renderer_;
};

class MockAudioSource : public webrtc::AudioSourceInterface {
 public:
  explicit MockAudioSource(
      const webrtc::MediaConstraintsInterface* constraints);

  virtual void RegisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual MediaSourceInterface::SourceState state() const OVERRIDE;

  // Changes the state of the source to live and notifies the observer.
  void SetLive();
  // Changes the state of the source to ended and notifies the observer.
  void SetEnded();

  const webrtc::MediaConstraintsInterface::Constraints& optional_constraints() {
    return optional_constraints_;
  }

  const webrtc::MediaConstraintsInterface::Constraints&
  mandatory_constraints() {
    return mandatory_constraints_;
  }

 protected:
  virtual ~MockAudioSource();

 private:
  webrtc::ObserverInterface* observer_;
  MediaSourceInterface::SourceState state_;
  webrtc::MediaConstraintsInterface::Constraints optional_constraints_;
  webrtc::MediaConstraintsInterface::Constraints mandatory_constraints_;
};

class MockLocalVideoTrack : public webrtc::VideoTrackInterface {
 public:
  MockLocalVideoTrack(std::string id,
                      webrtc::VideoSourceInterface* source);
  virtual void AddRenderer(webrtc::VideoRendererInterface* renderer) OVERRIDE;
  virtual void RemoveRenderer(
      webrtc::VideoRendererInterface* renderer) OVERRIDE;
  virtual std::string kind() const OVERRIDE;
  virtual std::string id() const OVERRIDE;
  virtual bool enabled() const OVERRIDE;
  virtual TrackState state() const OVERRIDE;
  virtual bool set_enabled(bool enable) OVERRIDE;
  virtual bool set_state(TrackState new_state) OVERRIDE;
  virtual void RegisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual webrtc::VideoSourceInterface* GetSource() const OVERRIDE;

 protected:
  virtual ~MockLocalVideoTrack();

 private:
  bool enabled_;
  std::string id_;
  TrackState state_;
  scoped_refptr<webrtc::VideoSourceInterface> source_;
  webrtc::ObserverInterface* observer_;
};

class MockMediaStream : public webrtc::MediaStreamInterface {
 public:
  explicit MockMediaStream(const std::string& label);

  virtual bool AddTrack(webrtc::AudioTrackInterface* track) OVERRIDE;
  virtual bool AddTrack(webrtc::VideoTrackInterface* track) OVERRIDE;
  virtual bool RemoveTrack(webrtc::AudioTrackInterface* track) OVERRIDE;
  virtual bool RemoveTrack(webrtc::VideoTrackInterface* track) OVERRIDE;
  virtual std::string label() const OVERRIDE;
  virtual webrtc::AudioTrackVector GetAudioTracks() OVERRIDE;
  virtual webrtc::VideoTrackVector GetVideoTracks() OVERRIDE;
  virtual talk_base::scoped_refptr<webrtc::AudioTrackInterface>
      FindAudioTrack(const std::string& track_id) OVERRIDE;
  virtual talk_base::scoped_refptr<webrtc::VideoTrackInterface>
      FindVideoTrack(const std::string& track_id) OVERRIDE;
  virtual void RegisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;
  virtual void UnregisterObserver(webrtc::ObserverInterface* observer) OVERRIDE;

 protected:
  virtual ~MockMediaStream();

 private:
  std::string label_;
  webrtc::AudioTrackVector audio_track_vector_;
  webrtc::VideoTrackVector video_track_vector_;
  webrtc::ObserverInterface* observer_;
};

// A mock factory for creating different objects for
// RTC MediaStreams and PeerConnections.
class MockMediaStreamDependencyFactory : public MediaStreamDependencyFactory {
 public:
  MockMediaStreamDependencyFactory();
  virtual ~MockMediaStreamDependencyFactory();

  virtual scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const webrtc::PeerConnectionInterface::IceServers& ice_servers,
      const webrtc::MediaConstraintsInterface* constraints,
      blink::WebFrame* frame,
      webrtc::PeerConnectionObserver* observer) OVERRIDE;
  virtual scoped_refptr<webrtc::AudioSourceInterface>
      CreateLocalAudioSource(
          const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
  virtual cricket::VideoCapturer* CreateVideoCapturer(
      const StreamDeviceInfo& info) OVERRIDE;
  virtual scoped_refptr<webrtc::VideoSourceInterface>
      CreateVideoSource(
          cricket::VideoCapturer* capturer,
          const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
  virtual scoped_refptr<WebAudioCapturerSource> CreateWebAudioSource(
      blink::WebMediaStreamSource* source) OVERRIDE;
  virtual scoped_refptr<webrtc::MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) OVERRIDE;
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            webrtc::VideoSourceInterface* source) OVERRIDE;
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            cricket::VideoCapturer* capturer) OVERRIDE;
  virtual scoped_refptr<webrtc::AudioTrackInterface> CreateLocalAudioTrack(
      const blink::WebMediaStreamTrack& blink_track,
      const scoped_refptr<WebRtcAudioCapturer>& capturer,
      WebAudioCapturerSource* webaudio_source,
      webrtc::AudioSourceInterface* source) OVERRIDE;
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& type,
      const std::string& sdp,
      webrtc::SdpParseError* error) OVERRIDE;
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp) OVERRIDE;

  virtual scoped_refptr<WebRtcAudioCapturer> CreateAudioCapturer(
      int render_view_id, const StreamDeviceInfo& device_info,
      const blink::WebMediaConstraints& constraints) OVERRIDE;

  MockAudioSource* last_audio_source() { return last_audio_source_.get(); }
  MockVideoSource* last_video_source() { return last_video_source_.get(); }

 private:
  scoped_refptr <MockAudioSource> last_audio_source_;
  scoped_refptr <MockVideoSource> last_video_source_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
