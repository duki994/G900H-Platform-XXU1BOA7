/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCPeerConnection_h
#define RTCPeerConnection_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/RTCIceCandidate.h"
#include "platform/AsyncMethodRunner.h"
#include "public/platform/WebMediaConstraints.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class ExceptionState;
class MediaStreamTrack;
class RTCConfiguration;
class RTCDTMFSender;
class RTCDataChannel;
class RTCErrorCallback;
class RTCSessionDescription;
class RTCSessionDescriptionCallback;
class RTCStatsCallback;
class VoidCallback;

class RTCPeerConnection FINAL : public RefCounted<RTCPeerConnection>, public ScriptWrappable, public blink::WebRTCPeerConnectionHandlerClient, public EventTargetWithInlineData, public ActiveDOMObject {
    REFCOUNTED_EVENT_TARGET(RTCPeerConnection);
public:
    static PassRefPtr<RTCPeerConnection> create(ExecutionContext*, const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState&);
    virtual ~RTCPeerConnection();

    void createOffer(PassOwnPtr<RTCSessionDescriptionCallback>, PassOwnPtr<RTCErrorCallback>, const Dictionary& mediaConstraints, ExceptionState&);

    void createAnswer(PassOwnPtr<RTCSessionDescriptionCallback>, PassOwnPtr<RTCErrorCallback>, const Dictionary& mediaConstraints, ExceptionState&);

    void setLocalDescription(PassRefPtr<RTCSessionDescription>, PassOwnPtr<VoidCallback>, PassOwnPtr<RTCErrorCallback>, ExceptionState&);
    PassRefPtr<RTCSessionDescription> localDescription(ExceptionState&);

    void setRemoteDescription(PassRefPtr<RTCSessionDescription>, PassOwnPtr<VoidCallback>, PassOwnPtr<RTCErrorCallback>, ExceptionState&);
    PassRefPtr<RTCSessionDescription> remoteDescription(ExceptionState&);

    String signalingState() const;

    void updateIce(const Dictionary& rtcConfiguration, const Dictionary& mediaConstraints, ExceptionState&);

    // DEPRECATED
    void addIceCandidate(RTCIceCandidate*, ExceptionState&);

    void addIceCandidate(RTCIceCandidate*, PassOwnPtr<VoidCallback>, PassOwnPtr<RTCErrorCallback>, ExceptionState&);

    String iceGatheringState() const;

    String iceConnectionState() const;

    MediaStreamVector getLocalStreams() const;

    MediaStreamVector getRemoteStreams() const;

    MediaStream* getStreamById(const String& streamId);

    void addStream(PassRefPtr<MediaStream>, const Dictionary& mediaConstraints, ExceptionState&);

    void removeStream(PassRefPtr<MediaStream>, ExceptionState&);

    void getStats(PassOwnPtr<RTCStatsCallback> successCallback, PassRefPtr<MediaStreamTrack> selector);

    PassRefPtr<RTCDataChannel> createDataChannel(String label, const Dictionary& dataChannelDict, ExceptionState&);

    PassRefPtr<RTCDTMFSender> createDTMFSender(PassRefPtr<MediaStreamTrack>, ExceptionState&);

    void close(ExceptionState&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(negotiationneeded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(signalingstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(addstream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removestream);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(iceconnectionstatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(datachannel);

    // blink::WebRTCPeerConnectionHandlerClient
    virtual void negotiationNeeded() OVERRIDE;
    virtual void didGenerateICECandidate(const blink::WebRTCICECandidate&) OVERRIDE;
    virtual void didChangeSignalingState(SignalingState) OVERRIDE;
    virtual void didChangeICEGatheringState(ICEGatheringState) OVERRIDE;
    virtual void didChangeICEConnectionState(ICEConnectionState) OVERRIDE;
    virtual void didAddRemoteStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void didRemoveRemoteStream(const blink::WebMediaStream&) OVERRIDE;
    virtual void didAddRemoteDataChannel(blink::WebRTCDataChannelHandler*) OVERRIDE;

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE { return !m_stopped; }

private:
    RTCPeerConnection(ExecutionContext*, PassRefPtr<RTCConfiguration>, blink::WebMediaConstraints, ExceptionState&);

    static PassRefPtr<RTCConfiguration> parseConfiguration(const Dictionary& configuration, ExceptionState&);
    void scheduleDispatchEvent(PassRefPtr<Event>);
    void dispatchScheduledEvent();
    bool hasLocalStreamWithTrackId(const String& trackId);

    void changeSignalingState(blink::WebRTCPeerConnectionHandlerClient::SignalingState);
    void changeIceGatheringState(blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState);
    void changeIceConnectionState(blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState);

    SignalingState m_signalingState;
    ICEGatheringState m_iceGatheringState;
    ICEConnectionState m_iceConnectionState;

    MediaStreamVector m_localStreams;
    MediaStreamVector m_remoteStreams;

    Vector<RefPtr<RTCDataChannel> > m_dataChannels;

    OwnPtr<blink::WebRTCPeerConnectionHandler> m_peerHandler;

    AsyncMethodRunner<RTCPeerConnection> m_dispatchScheduledEventRunner;
    Vector<RefPtr<Event> > m_scheduledEvents;

    bool m_stopped;
};

} // namespace WebCore

#endif // RTCPeerConnection_h
