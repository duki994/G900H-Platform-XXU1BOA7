/**
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 /**
 * See http://dev.w3.org/2011/webrtc/editor/getusermedia.html for more
 * information on getUserMedia. See
 * http://dev.w3.org/2011/webrtc/editor/webrtc.html for more information on
 * peerconnection and webrtc in general.
 */

/** TODO(jansson) give it a better name
 * Global namespace object.
 */
var global = {};

/**
 * We need a STUN server for some API calls.
 * @private
 */
var STUN_SERVER = 'stun.l.google.com:19302';

/** @private */
global.transformOutgoingSdp = function(sdp) { return sdp; };

/** @private */
global.dataStatusCallback = function(status) {};

/** @private */
global.dataCallback = function(data) {};

/** @private */
global.dtmfOnToneChange = function(tone) {};

/**
 * Used as a shortcut for finding DOM elements by ID.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {object} id returns the element object specified as a parameter
 */
$ = function(id) {
  return document.getElementById(id);
};

/**
 * Prepopulate constraints from JS to the UI. Enumerate devices available
 * via getUserMedia, register elements to be used for local storage.
 */
window.onload = function() {
  hookupDataChannelCallbacks_();
  hookupDtmfSenderCallback_();
  updateGetUserMediaConstraints();
  setupLocalStorageFieldValues();
  acceptIncomingCalls();
  if ($('get-devices-onload').checked == true) {
    getDevices();
  }
};

/**
 * Disconnect before the tab is closed.
 */
window.onbeforeunload = function() {
  disconnect_();
};

/** TODO Add element.id as a parameter and call this function instead?
 *  A list of element id's to be registered for local storage.
 */
function setupLocalStorageFieldValues() {
  registerLocalStorage_('pc-server');
  registerLocalStorage_('pc-createanswer-constraints');
  registerLocalStorage_('pc-createoffer-constraints');
  registerLocalStorage_('get-devices-onload');
}

// Public HTML functions

// The *Here functions are called from peerconnection.html and will make calls
// into our underlying JavaScript library with the values from the page
// (have to be named differently to avoid name clashes with existing functions).

function getUserMediaFromHere() {
  var constraints = $('getusermedia-constraints').value;
  try {
    doGetUserMedia_(constraints);
  } catch (exception) {
    print_('getUserMedia says: ' + exception);
  }
}

function connectFromHere() {
  var server = $('pc-server').value;
  if ($('peer-id').value == '') {
    // Generate a random name to distinguish us from other tabs:
    $('peer-id').value = 'peer_' + Math.floor(Math.random() * 10000);
    print_('Our name from now on will be ' + $('peer-id').value);
  }
  connect(server, $('peer-id').value);
}

function negotiateCallFromHere() {
  // Set the global variables with values from our UI.
  setCreateOfferConstraints(getEvaluatedJavaScript_(
      $('pc-createoffer-constraints').value));
  setCreateAnswerConstraints(getEvaluatedJavaScript_(
      $('pc-createanswer-constraints').value));

  ensureHasPeerConnection_();
  negotiateCall_();
}

function addLocalStreamFromHere() {
  ensureHasPeerConnection_();
  addLocalStream();
}

function removeLocalStreamFromHere() {
  removeLocalStream();
}

function hangUpFromHere() {
  hangUp();
  acceptIncomingCalls();
}

function toggleRemoteVideoFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.getVideoTracks()[0];
  }, 'video');
}

function toggleRemoteAudioFromHere() {
  toggleRemoteStream(function(remoteStream) {
    return remoteStream.getAudioTracks()[0];
  }, 'audio');
}

function toggleLocalVideoFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.getVideoTracks()[0];
  }, 'video');
}

function toggleLocalAudioFromHere() {
  toggleLocalStream(function(localStream) {
    return localStream.getAudioTracks()[0];
  }, 'audio');
}

function stopLocalFromHere() {
  stopLocalStream();
}

function createDataChannelFromHere() {
  ensureHasPeerConnection_();
  createDataChannelOnPeerConnection();
}

function closeDataChannelFromHere() {
  ensureHasPeerConnection_();
  closeDataChannelOnPeerConnection();
}

function sendDataFromHere() {
  var data = $('data-channel-send').value;
  sendDataOnChannel(data);
}

function createDtmfSenderFromHere() {
  ensureHasPeerConnection_();
  createDtmfSenderOnPeerConnection();
}

function insertDtmfFromHere() {
  var tones = $('dtmf-tones').value;
  var duration = $('dtmf-tones-duration').value;
  var gap = $('dtmf-tones-gap').value;
  insertDtmfOnSender(tones, duration, gap);
}

function forceIsacChanged() {
  var forceIsac = $('force-isac').checked;
  if (forceIsac) {
    forceIsac_();
  } else {
    dontTouchSdp_();
  }
}

/**
 * Updates the constraints in the getusermedia-constraints text box with a
 * MediaStreamConstraints string. This string is created based on the status of
 * the checkboxes for audio and video. If device enumeration is supported and
 * device source id's are not null they will be added to the constraints string.
 * Fetches the screen size using "screen" in Chrome as we need to pass a max
 * resolution else it defaults to 640x480 in the constraints for screen
 * capturing.
 */
function updateGetUserMediaConstraints() {
  var audioSelected = $('audiosrc');
  var videoSelected = $('videosrc');
  var constraints = {
    audio: $('audio').checked,
    video: $('video').checked
  };

  if (audioSelected.disabled == false && videoSelected.disabled == false) {
    var devices = getSourcesFromField_(audioSelected, videoSelected);
    if ($('audio').checked == true) {
      if (devices.audioId != null) {
        constraints.audio = {optional: [{sourceId: devices.audioId}]};
      } else {
        constraints.audio = true;
      }
    }
    if ($('video').checked == true) {
      // Default optional constraints placed here.
      constraints.video = {optional: [{minWidth: $('video-width').value},
                                      {minHeight: $('video-height').value},
                                      {googCpuOveruseDetection: true},
                                      {googLeakyBucket: true}]
      };
      if (devices.videoId != null) {
        constraints.video.optional.push({sourceId: devices.videoId});
      }
    }
  }

  if ($('screencapture').checked) {
    var constraints = {
      audio: $('audio').checked,
      video: {mandatory: {chromeMediaSource: 'screen',
                          maxWidth: screen.width,
                          maxHeight: screen.height}}
    };
    if ($('audio').checked == true)
      print_('Audio for screencapture is not implemented yet, please ' +
            'try to set audio = false prior requesting screencapture');
  }
  $('getusermedia-constraints').value = JSON.stringify(constraints, null, ' ');
}

function showServerHelp() {
  alert('You need to build and run a peerconnection_server on some ' +
        'suitable machine. To build it in chrome, just run make/ninja ' +
        'peerconnection_server. Otherwise, read in https://code.google' +
        '.com/searchframe#xSWYf0NTG_Q/trunk/peerconnection/README&q=REA' +
        'DME%20package:webrtc%5C.googlecode%5C.com.');
}

function clearLog() {
  $('messages').innerHTML = '';
  $('debug').innerHTML = '';
}

/**
 * Stops the local stream.
 */
function stopLocalStream() {
  if (global.localStream == null)
    error_('Tried to stop local stream, ' +
           'but media access is not granted.');

  global.localStream.stop();
}

/**
 * Adds the current local media stream to a peer connection.
 * @param {RTCPeerConnection} peerConnection
 */
function addLocalStreamToPeerConnection(peerConnection) {
  if (global.localStream == null)
    error_('Tried to add local stream to peer connection, but there is no ' +
           'stream yet.');
  try {
    peerConnection.addStream(global.localStream, global.addStreamConstraints);
  } catch (exception) {
    error_('Failed to add stream with constraints ' +
           global.addStreamConstraints + ': ' + exception);
  }
  print_('Added local stream.');
}

/**
 * Removes the local stream from the peer connection.
 * @param {rtcpeerconnection} peerConnection
 */
function removeLocalStreamFromPeerConnection(peerConnection) {
  if (global.localStream == null)
    error_('Tried to remove local stream from peer connection, but there is ' +
           'no stream yet.');
  try {
    peerConnection.removeStream(global.localStream);
  } catch (exception) {
    error_('Could not remove stream: ' + exception);
  }
  print_('Removed local stream.');
}

/**
 * Enumerates the audio and video devices available in Chrome and adds the
 * devices to the HTML elements with Id 'audiosrc' and 'videosrc'.
 * Checks if device enumeration is supported and if the 'audiosrc' + 'videosrc'
 * elements exists, if not a debug printout will be displayed.
 * If the device label is empty, audio/video + sequence number will be used to
 * populate the name. Also makes sure the children has been loaded in order
 * to update the constraints.
 */
function getDevices() {
  var audio_select = $('audiosrc');
  var video_select = $('videosrc');
  var get_devices = $('get-devices');
  audio_select.innerHTML = '';
  video_select.innerHTML = '';
  try {
    eval(MediaStreamTrack.getSources(function() {}));
  } catch (exception) {
    audio_select.disabled = true;
    video_select.disabled = true;
    refresh_devices.disabled = true;
    updateGetUserMediaConstraints();
    error_('Device enumeration not supported. ' + exception);
  }
  MediaStreamTrack.getSources(function(devices) {
    for (var i = 0; i < devices.length; i++) {
      var option = document.createElement('option');
      option.value = devices[i].id;
      option.text = devices[i].label;
      if (devices[i].kind == 'audio') {
        if (option.text == '') {
          option.text = devices[i].id;
        }
        audio_select.appendChild(option);
      } else if (devices[i].kind == 'video') {
        if (option.text == '') {
          option.text = devices[i].id;
        }
        video_select.appendChild(option);
      } else {
        error_('Device type ' + devices[i].kind + ' not recognized, ' +
                'cannot enumerate device. Currently only device types' +
                '\'audio\' and \'video\' are supported');
        updateGetUserMediaConstraints();
        return;
      }
    }
  });
  checkIfDeviceDropdownsArePopulated_();
}

/**
 * Sets the transform to apply just before setting the local description and
 * sending to the peer.
 * @param {function} transformFunction A function which takes one SDP string as
 *     argument and returns the modified SDP string.
 */
function setOutgoingSdpTransform(transformFunction) {
  global.transformOutgoingSdp = transformFunction;
}

/**
 * Sets the MediaConstraints to be used for PeerConnection createAnswer() calls.
 * @param {string} mediaConstraints The constraints, as defined in the
 *     PeerConnection JS API spec.
 */
function setCreateAnswerConstraints(mediaConstraints) {
  global.createAnswerConstraints = mediaConstraints;
}

/**
 * Sets the MediaConstraints to be used for PeerConnection createOffer() calls.
 * @param {string} mediaConstraints The constraints, as defined in the
 *     PeerConnection JS API spec.
 */
function setCreateOfferConstraints(mediaConstraints) {
  global.createOfferConstraints = mediaConstraints;
}

/**
 * Sets the callback functions that will receive DataChannel readyState updates
 * and received data.
 * @param {function} status_callback The function that will receive a string
 * with
 *     the current DataChannel readyState.
 * @param {function} data_callback The function that will a string with data
 *     received from the remote peer.
 */
function setDataCallbacks(status_callback, data_callback) {
  global.dataStatusCallback = status_callback;
  global.dataCallback = data_callback;
}

/**
 * Sends data on an active DataChannel.
 * @param {string} data The string that will be sent to the remote peer.
 */
function sendDataOnChannel(data) {
  if (global.dataChannel == null)
    error_('Trying to send data, but there is no DataChannel.');
  global.dataChannel.send(data);
}

/**
 * Sets the callback function that will receive DTMF sender ontonechange events.
 * @param {function} ontonechange The function that will receive a string with
 *     the tone that has just begun playout.
 */
function setOnToneChange(ontonechange) {
  global.dtmfOnToneChange = ontonechange;
}

/**
 * Inserts DTMF tones on an active DTMF sender.
 * @param {string} tones to be sent.
 * @param {string} duration duration of the tones to be sent.
 * @param {string} interToneGap gap between the tones to be sent.
 */
function insertDtmf(tones, duration, interToneGap) {
  if (global.dtmfSender == null)
    error_('Trying to send DTMF, but there is no DTMF sender.');
  global.dtmfSender.insertDTMF(tones, duration, interToneGap);
}

function handleMessage(peerConnection, message) {
  var parsed_msg = JSON.parse(message);
  if (parsed_msg.type) {
    var session_description = new RTCSessionDescription(parsed_msg);
    peerConnection.setRemoteDescription(
        session_description,
        function() { success_('setRemoteDescription'); },
        function() { failure_('setRemoteDescription'); });
    if (session_description.type == 'offer') {
      print_('createAnswer with constraints: ' +
            JSON.stringify(global.createAnswerConstraints, null, ' '));
      peerConnection.createAnswer(
        setLocalAndSendMessage_,
        function() { failure_('createAnswer'); },
        global.createAnswerConstraints);
    }
    return;
  } else if (parsed_msg.candidate) {
    var candidate = new RTCIceCandidate(parsed_msg);
    peerConnection.addIceCandidate(candidate);
    return;
  }
  error_('unknown message received');
}

function createPeerConnection(stun_server, useRtpDataChannels) {
  servers = {iceServers: [{url: 'stun:' + stun_server}]};
  try {
    var constraints = { optional: [{ RtpDataChannels: useRtpDataChannels }]};
    peerConnection = new webkitRTCPeerConnection(servers, constraints);
  } catch (exception) {
    error_('Failed to create peer connection: ' + exception);
  }
  peerConnection.onaddstream = addStreamCallback_;
  peerConnection.onremovestream = removeStreamCallback_;
  peerConnection.onicecandidate = iceCallback_;
  peerConnection.ondatachannel = onCreateDataChannelCallback_;
  return peerConnection;
}

function setupCall(peerConnection) {
  print_('createOffer with constraints: ' +
        JSON.stringify(global.createOfferConstraints, null, ' '));
  peerConnection.createOffer(
      setLocalAndSendMessage_,
      function() { failure_('createOffer'); },
      global.createOfferConstraints);
}

function answerCall(peerConnection, message) {
  handleMessage(peerConnection, message);
}

function createDataChannel(peerConnection, label) {
  if (global.dataChannel != null && global.dataChannel.readyState != 'closed')
    error_('Creating DataChannel, but we already have one.');

  global.dataChannel = peerConnection.createDataChannel(label,
                                                        { reliable: false });
  print_('DataChannel with label ' + global.dataChannel.label + ' initiated ' +
         'locally.');
  hookupDataChannelEvents();
}

function closeDataChannel(peerConnection) {
  if (global.dataChannel == null)
    error_('Closing DataChannel, but none exists.');
  print_('DataChannel with label ' + global.dataChannel.label +
         ' is beeing closed.');
  global.dataChannel.close();
}

function createDtmfSender(peerConnection) {
  if (global.dtmfSender != null)
    error_('Creating DTMF sender, but we already have one.');

  var localStream = global.localStream();
  if (localStream == null)
    error_('Creating DTMF sender but local stream is null.');
  local_audio_track = localStream.getAudioTracks()[0];
  global.dtmfSender = peerConnection.createDTMFSender(local_audio_track);
  global.dtmfSender.ontonechange = global.dtmfOnToneChange;
}

/**
 * Connects to the provided peerconnection_server.
 *
 * @param {string} serverUrl The server URL in string form without an ending
 *     slash, something like http://localhost:8888.
 * @param {string} clientName The name to use when connecting to the server.
 */
function connect(serverUrl, clientName) {
  if (global.ourPeerId != null)
    error_('connecting, but is already connected.');

  print_('Connecting to ' + serverUrl + ' as ' + clientName);
  global.serverUrl = serverUrl;
  global.ourClientName = clientName;

  request = new XMLHttpRequest();
  request.open('GET', serverUrl + '/sign_in?' + clientName, true);
  print_(serverUrl + '/sign_in?' + clientName);
  request.onreadystatechange = function() {
    connectCallback_(request);
  };
  request.send();
}

/**
 * Checks if the remote peer has connected. Returns peer-connected if that is
 * the case, otherwise no-peer-connected.
 */
function remotePeerIsConnected() {
  if (global.remotePeerId == null)
    print_('no-peer-connected');
  else
    print_('peer-connected');
}

/**
 * Creates a peer connection. Must be called before most other public functions
 * in this file.
 */
function preparePeerConnection() {
  if (global.peerConnection != null)
    error_('creating peer connection, but we already have one.');

  global.useRtpDataChannels = $('data-channel-type-rtp').checked;
  global.peerConnection = createPeerConnection(STUN_SERVER,
                                               global.useRtpDataChannels);
  print_('ok-peerconnection-created');
}

/**
 * Adds the local stream to the peer connection. You will have to re-negotiate
 * the call for this to take effect in the call.
 */
function addLocalStream() {
  if (global.peerConnection == null)
    error_('adding local stream, but we have no peer connection.');

  addLocalStreamToPeerConnection(global.peerConnection);
  print_('ok-added');
}

/**
 * Removes the local stream from the peer connection. You will have to
 * re-negotiate the call for this to take effect in the call.
 */
function removeLocalStream() {
  if (global.peerConnection == null)
    error_('attempting to remove local stream, but no call is up');

  removeLocalStreamFromPeerConnection(global.peerConnection);
  print_('ok-local-stream-removed');
}

/**
 * (see getReadyState_)
 */
function getPeerConnectionReadyState() {
  print_(getReadyState_());
}

/**
 * Toggles the remote audio stream's enabled state on the peer connection, given
 * that a call is active. Returns ok-[typeToToggle]-toggled-to-[true/false]
 * on success.
 *
 * @param {function} selectAudioOrVideoTrack A function that takes a remote
 *     stream as argument and returns a track (e.g. either the video or audio
 *     track).
 * @param {function} typeToToggle Either "audio" or "video" depending on what
 *     the selector function selects.
 */
function toggleRemoteStream(selectAudioOrVideoTrack, typeToToggle) {
  if (global.peerConnection == null)
    error_('Tried to toggle remote stream, but have no peer connection.');
  if (global.peerConnection.getRemoteStreams().length == 0)
    error_('Tried to toggle remote stream, but not receiving any stream.');

  var track = selectAudioOrVideoTrack(
      global.peerConnection.getRemoteStreams()[0]);
  toggle_(track, 'remote', typeToToggle);
}

/**
 * See documentation on toggleRemoteStream (this function is the same except
 * we are looking at local streams).
 */
function toggleLocalStream(selectAudioOrVideoTrack, typeToToggle) {
  if (global.peerConnection == null)
    error_('Tried to toggle local stream, but have no peer connection.');
  if (global.peerConnection.getLocalStreams().length == 0)
    error_('Tried to toggle local stream, but there is no local stream in ' +
           'the call.');

  var track = selectAudioOrVideoTrack(
      global.peerConnection.getLocalStreams()[0]);
  toggle_(track, 'local', typeToToggle);
}

/**
 * Hangs up a started call. Returns ok-call-hung-up on success. This tab will
 * not accept any incoming calls after this call.
 */
function hangUp() {
  if (global.peerConnection == null)
    error_('hanging up, but has no peer connection');
  if (getReadyState_() != 'active')
    error_('hanging up, but ready state is not active (no call up).');
  sendToPeer(global.remotePeerId, 'BYE');
  closeCall_();
  global.acceptsIncomingCalls = false;
  print_('ok-call-hung-up');
}

/**
 * Start accepting incoming calls.
 */
function acceptIncomingCalls() {
  global.acceptsIncomingCalls = true;
}

/**
 * Creates a DataChannel on the current PeerConnection. Only one DataChannel can
 * be created on each PeerConnection.
 * Returns ok-datachannel-created on success.
 */
function createDataChannelOnPeerConnection() {
  if (global.peerConnection == null)
    error_('Tried to create data channel, but have no peer connection.');

  createDataChannel(global.peerConnection, global.ourClientName);
  print_('ok-datachannel-created');
}

/**
 * Close the DataChannel on the current PeerConnection.
 * Returns ok-datachannel-close on success.
 */
function closeDataChannelOnPeerConnection() {
  if (global.peerConnection == null)
    error_('Tried to close data channel, but have no peer connection.');

  closeDataChannel(global.peerConnection);
  print_('ok-datachannel-close');
}

/**
 * Creates a DTMF sender on the current PeerConnection.
 * Returns ok-dtmfsender-created on success.
 */
function createDtmfSenderOnPeerConnection() {
  if (global.peerConnection == null)
    error_('Tried to create DTMF sender, but have no peer connection.');

  createDtmfSender(global.peerConnection);
  print_('ok-dtmfsender-created');
}

/**
 * Send DTMF tones on the global.dtmfSender.
 * Returns ok-dtmf-sent on success.
 */
function insertDtmfOnSender(tones, duration, interToneGap) {
  if (global.dtmfSender == null)
    error_('Tried to insert DTMF tones, but have no DTMF sender.');

  insertDtmf(tones, duration, interToneGap);
  print_('ok-dtmf-sent');
}

/**
 * Sends a message to a peer through the peerconnection_server.
 */
function sendToPeer(peer, message) {
  var messageToLog = message.sdp ? message.sdp : message;
  print_('Sending message ' + messageToLog + ' to peer ' + peer + '.');

  var request = new XMLHttpRequest();
  var url = global.serverUrl + '/message?peer_id=' + global.ourPeerId + '&to=' +
      peer;
  request.open('POST', url, false);
  request.setRequestHeader('Content-Type', 'text/plain');
  request.send(message);
}

/**
 * @param {!string} videoTagId The ID of the video tag to update.
 * @param {!number} width of the video to update the video tag, if width or
 *     height is 0, size will be taken from videoTag.videoWidth.
 * @param {!number} height of the video to update the video tag, if width or
 *     height is 0 size will be taken from the videoTag.videoHeight.
 */
function updateVideoTagSize(videoTagId, width, height) {
  var videoTag = $(videoTagId);
  if (width > 0 || height > 0) {
    videoTag.width = width;
    videoTag.height = height;
  }
  else {
    if (videoTag.videoWidth > 0 || videoTag.videoHeight > 0) {
      videoTag.width = videoTag.videoWidth;
      videoTag.height = videoTag.videoHeight;
      print_('Set video tag "' + videoTagId + '" size to ' + videoTag.width +
             'x' + videoTag.height);
    }
    else {
      print_('"' + videoTagId + '" video stream size is 0, skipping resize');
    }
  }
  displayVideoSize_(videoTag);
}

// Internals.

/**
 * Disconnects from the peerconnection server. Returns ok-disconnected on
 * success.
 */
function disconnect_() {
  if (global.ourPeerId == null)
    return;

  request = new XMLHttpRequest();
  request.open('GET', global.serverUrl + '/sign_out?peer_id=' +
               global.ourPeerId, false);
  request.send();
  global.ourPeerId = null;
  print_('ok-disconnected');
}

/**
* Returns true if we are disconnected from peerconnection_server.
*/
function isDisconnected_() {
  return global.ourPeerId == null;
}

/**
 * @private
 * @return {!string} The current peer connection's ready state, or
 *     'no-peer-connection' if there is no peer connection up.
 *
 * NOTE: The PeerConnection states are changing and until chromium has
 *       implemented the new states we have to use this interim solution of
 *       always assuming that the PeerConnection is 'active'.
 */
function getReadyState_() {
  if (global.peerConnection == null)
    return 'no-peer-connection';

  return 'active';
}

/**
 * This function asks permission to use the webcam and mic from the browser. It
 * will return ok-requested to the test. This does not mean the request was
 * approved though. The test will then have to click past the dialog that
 * appears in Chrome, which will run either the OK or failed callback as a
 * a result. To see which callback was called, use obtainGetUserMediaResult_().
 * @private
 * @param {string} constraints Defines what to be requested, with mandatory
 *     and optional constraints defined. The contents of this parameter depends
 *     on the WebRTC version. This should be JavaScript code that we eval().
 */
function doGetUserMedia_(constraints) {
  if (!getUserMedia) {
    print_('Browser does not support WebRTC.');
    return;
  }
  try {
    var evaluatedConstraints;
    eval('evaluatedConstraints = ' + constraints);
  } catch (exception) {
    error_('Not valid JavaScript expression: ' + constraints);
  }
  print_('Requesting doGetUserMedia: constraints: ' + constraints);
  getUserMedia(evaluatedConstraints, getUserMediaOkCallback_,
               getUserMediaFailedCallback_);
}

/**
 * Must be called after calling doGetUserMedia.
 * @private
 * @return {string} Returns not-called-yet if we have not yet been called back
 *     by WebRTC. Otherwise it returns either ok-got-stream or
 *     failed-with-error-x (where x is the error code from the error
 *     callback) depending on which callback got called by WebRTC.
 */
function obtainGetUserMediaResult_() {
  if (global.requestWebcamAndMicrophoneResult == null)
    global.requestWebcamAndMicrophoneResult = ' not called yet';

  return global.requestWebcamAndMicrophoneResult;

}

/**
 * Negotiates a call with the other side. This will create a peer connection on
 * the other side if there isn't one.
 *
 * To call this method we need to be aware of the other side, e.g. we must be
 * connected to peerconnection_server and we must have exactly one peer on that
 * server.
 *
 * This method may be called any number of times. If you haven't added any
 * streams to the call, an "empty" call will result. The method will return
 * ok-negotiating immediately to the test if the negotiation was successfully
 * sent.
 * @private
 */
function negotiateCall_() {
  if (global.peerConnection == null)
    error_('Negotiating call, but we have no peer connection.');
  if (global.ourPeerId == null)
    error_('Negotiating call, but not connected.');
  if (global.remotePeerId == null)
    error_('Negotiating call, but missing remote peer.');

  setupCall(global.peerConnection);
  print_('ok-negotiating');
}

/**
 * This provides the selected source id from the objects in the parameters
 * provided to this function. If the audioSelect or video_select objects does
 * not have any HTMLOptions children it will return null in the source object.
 * @param {!object} audioSelect HTML drop down element with audio devices added
 *     as HTMLOptionsCollection children.
 * @param {!object} videoSelect HTML drop down element with audio devices added
 *     as HTMLOptionsCollection children.
 * @return {!object} source contains audio and video source ID from
 *     the selected devices in the drop down menu elements.
 * @private
 */
function getSourcesFromField_(audioSelect, videoSelect) {
  var source = {
    audioId: null,
    videoId: null
  };
  if (audioSelect.options.length > 0) {
    source.audioId = audioSelect.options[audioSelect.selectedIndex].value;
  }
  if (videoSelect.options.length > 0) {
    source.videoId = videoSelect.options[videoSelect.selectedIndex].value;
  }
  return source;
}

/**
 * @private
 * @param {NavigatorUserMediaError} error Error containing details.
 */
function getUserMediaFailedCallback_(error) {
  print_('GetUserMedia FAILED: Maybe the camera is in use by another process?');
  gRequestWebcamAndMicrophoneResult = 'failed-with-error-' + error.name;
  print_(gRequestWebcamAndMicrophoneResult);
}

/** @private */
function success_(method) {
  print_(method + '(): success.');
}

/** @private */
function failure_(method, error) {
  error_(method + '() failed: ' + error);
}

/** @private */
function iceCallback_(event) {
  if (event.candidate)
    sendToPeer(global.remotePeerId, JSON.stringify(event.candidate));
}

/** @private */
function setLocalAndSendMessage_(session_description) {
  session_description.sdp =
    global.transformOutgoingSdp(session_description.sdp);
  global.peerConnection.setLocalDescription(
    session_description,
    function() { success_('setLocalDescription'); },
    function() { failure_('setLocalDescription'); });
  print_('Sending SDP message:\n' + session_description.sdp);
  sendToPeer(global.remotePeerId, JSON.stringify(session_description));
}

/** @private */
function addStreamCallback_(event) {
  print_('Receiving remote stream...');
  var videoTag = document.getElementById('remote-view');
  attachMediaStream(videoTag, event.stream);

  window.addEventListener('loadedmetadata', function() {
     displayVideoSize_(videoTag);}, true);
}

/** @private */
function removeStreamCallback_(event) {
  print_('Call ended.');
  document.getElementById('remote-view').src = '';
}

/** @private */
function onCreateDataChannelCallback_(event) {
  if (global.dataChannel != null && global.dataChannel.readyState != 'closed') {
    error_('Received DataChannel, but we already have one.');
  }

  global.dataChannel = event.channel;
  print_('DataChannel with label ' + global.dataChannel.label +
            ' initiated by remote peer.');
  hookupDataChannelEvents();
}

/** @private */
function hookupDataChannelEvents() {
  global.dataChannel.onmessage = global.dataCallback;
  global.dataChannel.onopen = onDataChannelReadyStateChange_;
  global.dataChannel.onclose = onDataChannelReadyStateChange_;
  // Trigger global.dataStatusCallback so an application is notified
  // about the created data channel.
  onDataChannelReadyStateChange_();
}

/** @private */
function onDataChannelReadyStateChange_() {
  var readyState = global.dataChannel.readyState;
  print_('DataChannel state:' + readyState);
  global.dataStatusCallback(readyState);
}

/**
 * @private
 * @param {MediaStream} stream Media stream.
 */
function getUserMediaOkCallback_(stream) {
  global.localStream = stream;
  global.requestWebcamAndMicrophoneResult = 'ok-got-stream';

  if (stream.getVideoTracks().length > 0) {
    // Show the video tag if we did request video in the getUserMedia call.
    var videoTag = $('local-view');
    attachMediaStream(videoTag, stream);

   window.addEventListener('loadedmetadata', function() {
       displayVideoSize_(videoTag);}, true);
  }
}

/**
 * @private
 * @param {string} videoTag The ID of the video tag + stream used to
 *     write the size to a HTML tag based on id if the div's exists.
 */
function displayVideoSize_(videoTag) {
  if ($(videoTag.id + '-stream-size') && $(videoTag.id + '-size')) {
    if (videoTag.videoWidth > 0 || videoTag.videoHeight > 0) {
      $(videoTag.id + '-stream-size').innerHTML = '(stream size: ' +
                                                  videoTag.videoWidth + 'x' +
                                                  videoTag.videoHeight + ')';
      $(videoTag.id + '-size').innerHTML = videoTag.width + 'x' +
                                           videoTag.height;
    }
  } else {
    print_('Skipping updating -stream-size and -size tags due to div\'s ' +
           'are missing');
  }
}

/**
 * Checks if the 'audiosrc' and 'videosrc' drop down menu elements has had all
 * of its children appended in order to provide device ID's to the function
 * 'updateGetUserMediaConstraints()', used in turn to populate the getUserMedia
 * constraints text box when the page has loaded.
 * @private
 */
function checkIfDeviceDropdownsArePopulated_() {
  if (document.addEventListener) {
    $('audiosrc').addEventListener('DOMNodeInserted',
         updateGetUserMediaConstraints, false);
    $('videosrc').addEventListener('DOMNodeInserted',
         updateGetUserMediaConstraints, false);
  } else {
    print_('addEventListener is not supported by your browser, cannot update ' +
           'device source ID\'s automatically. Select a device from the audio' +
           ' or video source drop down menu to update device source id\'s');
  }
}

/**
 * Register an input element to use local storage to remember its state between
 * sessions (using local storage). Only input elements are supported.
 * @private
 * @param {!string} element_id to be used as a key for local storage and the id
 *     of the element to store the state for.
 */
function registerLocalStorage_(element_id) {
  var element = $(element_id);
  if (element.tagName != 'INPUT') {
    error_('You can only use registerLocalStorage_ for input elements. ' +
          'Element \"' + element.tagName + '\" is not an input element. ');
  }

  if (localStorage.getItem(element.id) == 'undefined') {
    storeLocalStorageField_(element);
  } else {
    getLocalStorageField_(element);
  }
  // Registers the appropriate events for input elements.
  if (element.type == 'checkbox') {
    element.onclick = function() { storeLocalStorageField_(this); };
  } else if (element.type == 'text') {
    element.onblur = function() { storeLocalStorageField_(this); };
  } else {
    error_('Unsupportered input type: ' + '\"' + element.type + '\"');
  }
}

/**
 * Fetches the stored values from local storage and updates checkbox status.
 * @private
 * @param {!Object} element of which id is representing the key parameter for
 *     local storage.
 */
function getLocalStorageField_(element) {
  // Makes sure the checkbox status is matching the local storage value.
  if (element.type == 'checkbox') {
    element.checked = (localStorage.getItem(element.id) == 'true');
  } else if (element.type == 'text') {
    element.value = localStorage.getItem(element.id);
  } else {
    error_('Unsupportered input type: ' + '\"' + element.type + '\"');
  }
}

/**
 * Stores the string value of the element object using local storage.
 * @private
 * @param {!Object} element of which id is representing the key parameter for
 *     local storage.
 */
function storeLocalStorageField_(element) {
  if (element.type == 'checkbox') {
    localStorage.setItem(element.id, element.checked);
  } else if (element.type == 'text') {
    localStorage.setItem(element.id, element.value);
  }
}

/**
 * Create the peer connection if none is up (this is just convenience to
 * avoid having a separate button for that).
 * @private
 */
function ensureHasPeerConnection_() {
  if (getReadyState_() == 'no-peer-connection') {
    preparePeerConnection();
  }
}

/**
 * @private
 * @param {string} message Text to print.
 */
function print_(message) {
  console.log(message);
  $('messages').innerHTML += message + '<br>';
}

/**
 * @private
 * @param {string} message Text to print.
 */
function debug_(message) {
  console.log(message);
  $('debug').innerHTML += message + '<br>';
}

/**
 * Print error message in the debug log + JS console and throw an Error.
 * @private
 * @param {string} message Text to print.
 */
function error_(message) {
  $('debug').innerHTML += '<span style="color:red;">' + message + '</span><br>';
  throw new Error(message);
}

/**
 * @private
 * @param {string} stringRepresentation JavaScript as a string.
 * @return {Object} The PeerConnection constraints as a JavaScript dictionary.
 */
function getEvaluatedJavaScript_(stringRepresentation) {
  try {
    var evaluatedJavaScript;
    eval('evaluatedJavaScript = ' + stringRepresentation);
    return evaluatedJavaScript;
  } catch (exception) {
    error_('Not valid JavaScript expression: ' + stringRepresentation);
  }
}

/**
 * Swaps lines within a SDP message.
 * @private
 * @param {string} sdp The full SDP message.
 * @param {string} line The line to swap with swapWith.
 * @param {string} swapWith The other line.
 * @return {string} The altered SDP message.
 */
function swapSdpLines_(sdp, line, swapWith) {
  var lines = sdp.split('\r\n');
  var lineStart = lines.indexOf(line);
  var swapLineStart = lines.indexOf(swapWith);
  if (lineStart == -1 || swapLineStart == -1)
    return sdp;  // This generally happens on the first message.

  var tmp = lines[lineStart];
  lines[lineStart] = lines[swapLineStart];
  lines[swapLineStart] = tmp;

  return lines.join('\r\n');
}

/** @private */
function forceIsac_() {
  setOutgoingSdpTransform(function(sdp) {
    // Remove all other codecs (not the video codecs though).
    sdp = sdp.replace(/m=audio (\d+) RTP\/SAVPF.*\r\n/g,
                      'm=audio $1 RTP/SAVPF 104\r\n');
    sdp = sdp.replace('a=fmtp:111 minptime=10', 'a=fmtp:104 minptime=10');
    sdp = sdp.replace(/a=rtpmap:(?!104)\d{1,3} (?!VP8|red|ulpfec).*\r\n/g, '');
    return sdp;
  });
}

/** @private */
function dontTouchSdp_() {
  setOutgoingSdpTransform(function(sdp) { return sdp; });
}

/** @private */
function hookupDataChannelCallbacks_() {
  setDataCallbacks(function(status) {
    $('data-channel-status').value = status;
  },
  function(data_message) {
    print_('Received ' + data_message.data);
    $('data-channel-receive').value =
      data_message.data + '\n' + $('data-channel-receive').value;
  });
}

/** @private */
function hookupDtmfSenderCallback_() {
  setOnToneChange(function(tone) {
    print_('Sent DTMF tone: ' + tone.tone);
    $('dtmf-tones-sent').value =
      tone.tone + '\n' + $('dtmf-tones-sent').value;
  });
}

/** @private */
function toggle_(track, localOrRemote, audioOrVideo) {
  if (!track)
    error_('Tried to toggle ' + localOrRemote + ' ' + audioOrVideo +
                 ' stream, but has no such stream.');

  track.enabled = !track.enabled;
  print_('ok-' + audioOrVideo + '-toggled-to-' + track.enabled);
}

/** @private */
function connectCallback_(request) {
  print_('Connect callback: ' + request.status + ', ' + request.readyState);
  if (request.status == 0) {
    print_('peerconnection_server doesn\'t seem to be up.');
    print_('failed-to-connect');
  }
  if (request.readyState == 4 && request.status == 200) {
    global.ourPeerId = parseOurPeerId_(request.responseText);
    global.remotePeerId = parseRemotePeerIdIfConnected_(request.responseText);
    startHangingGet_(global.serverUrl, global.ourPeerId);
    print_('ok-connected');
  }
}

/** @private */
function parseOurPeerId_(responseText) {
  // According to peerconnection_server's protocol.
  var peerList = responseText.split('\n');
  return parseInt(peerList[0].split(',')[1]);
}

/** @private */
function parseRemotePeerIdIfConnected_(responseText) {
  var peerList = responseText.split('\n');
  if (peerList.length == 1) {
    // No peers have connected yet - we'll get their id later in a notification.
    return null;
  }
  var remotePeerId = null;
  for (var i = 0; i < peerList.length; i++) {
    if (peerList[i].length == 0)
      continue;

    var parsed = peerList[i].split(',');
    var name = parsed[0];
    var id = parsed[1];

    if (id != global.ourPeerId) {
      print_('Found remote peer with name ' + name + ', id ' +
                id + ' when connecting.');

      // There should be at most one remote peer in this test.
      if (remotePeerId != null)
        error_('Expected just one remote peer in this test: ' +
               'found several.');

      // Found a remote peer.
      remotePeerId = id;
    }
  }
  return remotePeerId;
}

/** @private */
function startHangingGet_(server, ourId) {
  if (isDisconnected_())
    return;

  hangingGetRequest = new XMLHttpRequest();
  hangingGetRequest.onreadystatechange = function() {
    hangingGetCallback_(hangingGetRequest, server, ourId);
  };
  hangingGetRequest.ontimeout = function() {
    hangingGetTimeoutCallback_(hangingGetRequest, server, ourId);
  };
  callUrl = server + '/wait?peer_id=' + ourId;
  print_('Sending ' + callUrl);
  hangingGetRequest.open('GET', callUrl, true);
  hangingGetRequest.send();
}

/** @private */
function hangingGetCallback_(hangingGetRequest, server, ourId) {
  if (hangingGetRequest.readyState != 4 || hangingGetRequest.status == 0) {
    // Code 0 is not possible if the server actually responded. Ignore.
    return;
  }
  if (hangingGetRequest.status != 200) {
    error_('Error ' + hangingGetRequest.status + ' from server: ' +
           hangingGetRequest.statusText);
  }
  var targetId = readResponseHeader_(hangingGetRequest, 'Pragma');
  if (targetId == ourId)
    handleServerNotification_(hangingGetRequest.responseText);
  else
    handlePeerMessage_(targetId, hangingGetRequest.responseText);

  hangingGetRequest.abort();
  restartHangingGet_(server, ourId);
}

/** @private */
function hangingGetTimeoutCallback_(hangingGetRequest, server, ourId) {
  print_('Hanging GET times out, re-issuing...');
  hangingGetRequest.abort();
  restartHangingGet_(server, ourId);
}

/** @private */
function handleServerNotification_(message) {
  var parsed = message.split(',');
  if (parseInt(parsed[2]) == 1) {
    // Peer connected - this must be our remote peer, and it must mean we
    // connected before them (except if we happened to connect to the server
    // at precisely the same moment).
    print_('Found remote peer with name ' + parsed[0] + ', id ' + parsed[1] +
           ' when connecting.');
    global.remotePeerId = parseInt(parsed[1]);
  }
}

/** @private */
function closeCall_() {
  if (global.peerConnection == null)
    debug_('Closing call, but no call active.');
  global.peerConnection.close();
  global.peerConnection = null;
}

/** @private */
function handlePeerMessage_(peerId, message) {
  print_('Received message from peer ' + peerId + ': ' + message);
  if (peerId != global.remotePeerId) {
    error_('Received notification from unknown peer ' + peerId +
           ' (only know about ' + global.remotePeerId + '.');
  }
  if (message.search('BYE') == 0) {
    print_('Received BYE from peer: closing call');
    closeCall_();
    return;
  }
  if (global.peerConnection == null && global.acceptsIncomingCalls) {
    // The other side is calling us.
    print_('We are being called: answer...');

    global.peerConnection = createPeerConnection(STUN_SERVER,
                                                 global.useRtpDataChannels);
    if ($('auto-add-stream-oncall') &&
        obtainGetUserMediaResult_() == 'ok-got-stream') {
      print_('We have a local stream, so hook it up automatically.');
      addLocalStreamToPeerConnection(global.peerConnection);
    }
    answerCall(global.peerConnection, message);
    return;
  }
  handleMessage(global.peerConnection, message);
}

/** @private */
function restartHangingGet_(server, ourId) {
  window.setTimeout(function() {
    startHangingGet_(server, ourId);
  }, 0);
}

/** @private */
function readResponseHeader_(request, key) {
  var value = request.getResponseHeader(key);
  if (value == null || value.length == 0) {
    error_('Received empty value ' + value +
           ' for response header key ' + key + '.');
  }
  return parseInt(value);
}
