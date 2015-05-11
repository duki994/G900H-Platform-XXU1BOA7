// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

Polymer('audio-player', {
  /**
   * Child Elements
   */
  audioController: null,
  audioElement: null,
  trackList: null,

  // Attributes of the element (little charactor only)
  playing: false,
  currenttrackurl: '',

  /**
   * Model object of the Audio Player.
   * @type {AudioPlayerModel}
   */
  model: null,

  /**
   * Initializes an element. This method is called automatically when the
   * element is ready.
   */
  ready: function() {
    this.audioController = this.$.audioController;
    this.audioElement = this.$.audio;
    this.trackList = this.$.trackList;

    this.audioElement.volume = 0;  // Temporaly initial volume.
    this.audioElement.addEventListener('ended', this.onAudioEnded.bind(this));
    this.audioElement.addEventListener('error', this.onAudioError.bind(this));

    var onAudioStatusUpdatedBound = this.onAudioStatusUpdate_.bind(this);
    this.audioElement.addEventListener('timeupdate', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('ended', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('play', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('pause', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('suspend', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('abort', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('error', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('emptied', onAudioStatusUpdatedBound);
    this.audioElement.addEventListener('stalled', onAudioStatusUpdatedBound);
  },

  /**
   * Registers handlers for changing of external variables
   */
  observe: {
    'trackList.currentTrackIndex': 'onCurrentTrackIndexChanged',
    'audioController.playing': 'onControllerPlayingChanged',
    'audioController.time': 'onControllerTimeChanged',
    'model.volume': 'onVolumeChanged',
  },

  /**
   * Invoked when trackList.currentTrackIndex is changed.
   * @param {number} oldValue old value.
   * @param {number} newValue new value.
   */
  onCurrentTrackIndexChanged: function(oldValue, newValue) {
    var currentTrackUrl = '';

    if (oldValue != newValue) {
      var currentTrack = this.trackList.getCurrentTrack();
      if (currentTrack && currentTrack.url != this.audioElement.src) {
        this.audioElement.src = currentTrack.url;
        currentTrackUrl = this.audioElement.src;
        this.audioElement.play();
      }
    }

    // The attributes may be being watched, so we change it at the last.
    this.currenttrackurl = currentTrackUrl;
  },

  /**
   * Invoked when audioController.playing is changed.
   * @param {boolean} oldValue old value.
   * @param {boolean} newValue new value.
   */
  onControllerPlayingChanged: function(oldValue, newValue) {
    this.playing = newValue;

    if (newValue) {
      if (!this.audioElement.src) {
        var currentTrack = this.trackList.getCurrentTrack();
        if (currentTrack && currentTrack.url != this.audioElement.src) {
          this.audioElement.src = currentTrack.url;
        }
      }

      if (this.audioElement.src) {
        this.currenttrackurl = this.audioElement.src;
        this.audioElement.play();
        return;
      }
    }

    this.audioController.playing = false;
    this.audioElement.pause();
    this.currenttrackurl = '';
    this.lastAudioUpdateTime_ = null;
  },

  /**
   * Invoked when audioController.volume is changed.
   * @param {number} oldValue old value.
   * @param {number} newValue new value.
   */
  onVolumeChanged: function(oldValue, newValue) {
    this.audioElement.volume = newValue / 100;
  },

  /**
   * Invoked when the model changed.
   * @param {AudioPlayerModel} oldValue Old Value.
   * @param {AudioPlayerModel} newValue Nld Value.
   */
  modelChanged: function(oldValue, newValue) {
    this.trackList.model = newValue;
    this.audioController.model = newValue;

    // Invoke the handler manually.
    this.onVolumeChanged(0, newValue.volume);
  },

  /**
   * Invoked when audioController.time is changed.
   * @param {number} oldValue old time (in ms).
   * @param {number} newValue new time (in ms).
   */
  onControllerTimeChanged: function(oldValue, newValue) {
    // Ignores updates from the audio element.
    if (this.lastAudioUpdateTime_ === newValue)
      return;

    if (this.audioElement.readyState !== 0)
      this.audioElement.currentTime = this.audioController.time / 1000;
  },

  /**
   * Invoked when the next button in the controller is clicked.
   * This handler is registered in the 'on-click' attribute of the element.
   */
  onControllerNextClicked: function() {
    this.advance_(true /* forward */, true /* repeat */);
  },

  /**
   * Invoked when the previous button in the controller is clicked.
   * This handler is registered in the 'on-click' attribute of the element.
   */
  onControllerPreviousClicked: function() {
    this.advance_(false /* forward */, true /* repeat */);
  },

  /**
   * Invoked when the playback in the audio element is ended.
   * This handler is registered in this.ready().
   */
  onAudioEnded: function() {
    this.advance_(true /* forward */, this.model.repeat);
  },

  /**
   * Invoked when the playback in the audio element gets error.
   * This handler is registered in this.ready().
   */
  onAudioError: function() {
    this.scheduleAutoAdvance_(true /* forward */, this.model.repeat);
  },

  /**
   * Invoked when the time of playback in the audio element is updated.
   * This handler is registered in this.ready().
   * @private
   */
  onAudioStatusUpdate_: function() {
    this.audioController.time =
        (this.lastAudioUpdateTime_ = this.audioElement.currentTime * 1000);
    this.audioController.duration = this.audioElement.duration * 1000;
    this.audioController.playing = !this.audioElement.paused;
  },

  /**
   * Goes to the previous or the next track.
   * @param {boolean} forward True if next, false if previous.
   * @param {boolean} repeat True if repeat-mode is enabled. False otherwise.
   * @private
   */
  advance_: function(forward, repeat) {
    this.cancelAutoAdvance_();

    var nextTrackIndex = this.trackList.getNextTrackIndex(forward, true);
    var isNextTrackAvailable =
        (this.trackList.getNextTrackIndex(forward, repeat) !== -1);

    this.trackList.currentTrackIndex = nextTrackIndex;

    if (isNextTrackAvailable) {
      var nextTrack = this.trackList.tracks[nextTrackIndex];
      this.audioElement.src = nextTrack.url;
      this.audioElement.play();
    } else {
      this.audioElement.pause();
    }
  },

  /**
   * Timeout ID of auto advance. Used internally in scheduleAutoAdvance_() and
   *     cancelAutoAdvance_().
   * @type {number}
   * @private
   */
  autoAdvanceTimer_: null,

  /**
   * Schedules automatic advance to the next track after a timeout.
   * @param {boolean} forward True if next, false if previous.
   * @param {boolean} repeat True if repeat-mode is enabled. False otherwise.
   * @private
   */
  scheduleAutoAdvance_: function(forward, repeat) {
    this.cancelAutoAdvance_();
    this.autoAdvanceTimer_ = setTimeout(
        function() {
          this.autoAdvanceTimer_ = null;
          // We are advancing only if the next track is not known to be invalid.
          // This prevents an endless auto-advancing in the case when all tracks
          // are invalid (we will only visit each track once).
          this.advance_(forward, repeat, true /* only if valid */);
        }.bind(this),
        3000);
  },

  /**
   * Cancels the scheduled auto advance.
   * @private
   */
  cancelAutoAdvance_: function() {
    if (this.autoAdvanceTimer_) {
      clearTimeout(this.autoAdvanceTimer_);
      this.autoAdvanceTimer_ = null;
    }
  },

  /**
   * The index of the current track.
   * If the list has no tracks, the value must be -1.
   *
   * @type {number}
   */
  get currentTrackIndex() {
    return this.trackList.currentTrackIndex;
  },
  set currentTrackIndex(value) {
    this.trackList.currentTrackIndex = value;
  },

  /**
   * The list of the tracks in the playlist.
   *
   * When it changed, current operation including playback is stopped and
   * restarts playback with new tracks if necessary.
   *
   * @type {Array.<AudioPlayer.TrackInfo>}
   */
  get tracks() {
    return this.trackList ? this.trackList.tracks : null;
  },
  set tracks(tracks) {
    if (this.trackList.tracks === tracks)
      return;

    this.cancelAutoAdvance_();

    this.trackList.tracks = tracks;
    var currentTrack = this.trackList.getCurrentTrack();
    if (currentTrack && currentTrack.url != this.audioElement.src) {
      this.audioElement.src = currentTrack.url;
      this.audioElement.play();
    }
  },

  /**
   * Invoked when the audio player is being unloaded.
   */
  onPageUnload: function() {
    this.audioElement.src = '';  // Hack to prevent crashing.
  },
});
