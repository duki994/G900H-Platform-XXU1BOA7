// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The class to Manage both offline / online speech recognition.
 */

<include src="plugin_manager.js"/>
<include src="audio_manager.js"/>
<include src="speech_recognition_manager.js"/>

cr.define('speech', function() {
  'use strict';

  /**
   * The state of speech recognition.
   *
   * @enum {string}
   */
  var SpeechState = {
    READY: 'READY',
    HOTWORD_RECOGNIZING: 'HOTWORD_RECOGNIZING',
    RECOGNIZING: 'RECOGNIZING',
    IN_SPEECH: 'IN_SPEECH',
    STOPPING: 'STOPPING'
  };

  /**
   * @constructor
   */
  function SpeechManager() {
    this.audioManager_ = new speech.AudioManager();
    this.audioManager_.addEventListener('audio', this.onAudioLevel_.bind(this));
    this.shown_ = false;
    this.speechRecognitionManager_ = new speech.SpeechRecognitionManager(this);
    this.setState_(SpeechState.READY);
  }

  /**
   * Updates the state.
   *
   * @param {SpeechState} newState The new state.
   * @private
   */
  SpeechManager.prototype.setState_ = function(newState) {
    if (this.state == newState)
      return;

    this.state = newState;
    chrome.send('setSpeechRecognitionState', [this.state]);
  };

  /**
   * Called with the mean audio level when audio data arrives.
   *
   * @param {cr.event.Event} event The event object for the audio data.
   * @private
   */
  SpeechManager.prototype.onAudioLevel_ = function(event) {
    var data = event.data;
    var level = 0;
    for (var i = 0; i < data.length; ++i)
      level += Math.abs(data[i]);
    level /= data.length;
    chrome.send('speechSoundLevel', [level]);
  };

  /**
   * Called when the hotword recognizer is ready.
   *
   * @param {PluginManager} pluginManager The hotword plugin manager which gets
   *   ready.
   * @private
   */
  SpeechManager.prototype.onHotwordRecognizerReady_ = function(pluginManager) {
    this.pluginManager_ = pluginManager;
    this.audioManager_.addEventListener(
        'audio', pluginManager.sendAudioData.bind(pluginManager));
    if (this.shown_) {
      this.pluginManager_.startRecognizer();
      this.audioManager_.start();
      this.setState_(SpeechState.HOTWORD_RECOGNIZING);
    } else {
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Called when the hotword is recognized.
   *
   * @param {number} confidence The confidence store of the recognition.
   * @private
   */
  SpeechManager.prototype.onHotwordRecognized_ = function(confidence) {
    if (this.state != SpeechState.HOTWORD_RECOGNIZING)
      return;
    this.pluginManager_.stopRecognizer();
    this.speechRecognitionManager_.start();
  };

  /**
   * Called when the speech recognition has happened.
   *
   * @param {string} result The speech recognition result.
   * @param {boolean} isFinal Whether the result is final or not.
   */
  SpeechManager.prototype.onSpeechRecognized = function(result, isFinal) {
    chrome.send('speechResult', [result, isFinal]);
    if (isFinal)
      this.speechRecognitionManager_.stop();
  };

  /**
   * Called when the speech recognition has started.
   */
  SpeechManager.prototype.onSpeechRecognitionStarted = function() {
    this.setState_(SpeechState.RECOGNIZING);
  };

  /**
   * Called when the speech recognition has ended.
   */
  SpeechManager.prototype.onSpeechRecognitionEnded = function() {
    // Restarts the hotword recognition.
    if (this.state != SpeechState.STOPPING && this.pluginManager_) {
      this.pluginManager_.startRecognizer();
      this.audioManager_.start();
      this.setState_(SpeechState.HOTWORD_RECOGNIZING);
    } else {
      this.audioManager_.stop();
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Called when a speech has started.
   */
  SpeechManager.prototype.onSpeechStarted = function() {
    if (this.state == SpeechState.RECOGNIZING)
      this.setState_(SpeechState.IN_SPEECH);
  };

  /**
   * Called when a speech has ended.
   */
  SpeechManager.prototype.onSpeechEnded = function() {
    if (this.state == SpeechState.IN_SPEECH)
      this.setState_(SpeechState.RECOGNIZING);
  };

  /**
   * Called when an error happened during the speech recognition.
   *
   * @param {SpeechRecognitionError} e The error object.
   */
  SpeechManager.prototype.onSpeechRecognitionError = function(e) {
    if (this.state != SpeechState.STOPPING)
      this.setState_(SpeechState.READY);
  };

  /**
   * Changes the availability of the hotword plugin.
   *
   * @param {boolean} enabled Whether enabled or not.
   */
  SpeechManager.prototype.setHotwordEnabled = function(enabled) {
    var recognizer = $('recognizer');
    if (enabled) {
      if (recognizer)
        return;

      var pluginManager = new speech.PluginManager(
          this.onHotwordRecognizerReady_.bind(this),
          this.onHotwordRecognized_.bind(this));
      pluginManager.scheduleInitialize(
          this.audioManager_.getSampleRate(),
          'chrome://app-list/okgoogle_hotword.config');
    } else {
      if (!recognizer)
        return;
      document.body.removeChild(recognizer);
      this.pluginManager_ = null;
      if (this.state == SpeechState.HOTWORD_RECOGNIZING)
        this.setState(SpeechState.READY);
    }
  };

  /**
   * Called when the app-list bubble is shown.
   */
  SpeechManager.prototype.onShown = function() {
    this.shown_ = true;
    if (!this.pluginManager_)
      return;

    if (this.state == SpeechState.HOTWORD_RECOGNIZING) {
      console.warn('Already in recognition state...');
      return;
    }

    this.pluginManager_.startRecognizer();
    this.audioManager_.start();
    this.setState_(SpeechState.HOTWORD_RECOGNIZING);
  };

  /**
   * Called when the app-list bubble is hidden.
   */
  SpeechManager.prototype.onHidden = function() {
    this.shown_ = false;
    if (this.pluginManager_)
      this.pluginManager_.stopRecognizer();

    // SpeechRecognition is asynchronous.
    this.audioManager_.stop();
    if (this.state == SpeechState.RECOGNIZING ||
        this.state == SpeechState.IN_SPEECH) {
      this.setState_(SpeechState.STOPPING);
      this.speechRecognitionManager_.stop();
    } else {
      this.setState_(SpeechState.READY);
    }
  };

  /**
   * Toggles the current state of speech recognition.
   */
  SpeechManager.prototype.toggleSpeechRecognition = function() {
    if (this.state == SpeechState.RECOGNIZING ||
        this.state == SpeechState.IN_SPEECH) {
      this.audioManager_.stop();
      this.speechRecognitionManager_.stop();
    } else {
      if (this.pluginManager_)
        this.pluginManager_.stopRecognizer();
      if (this.audioManager_.state == speech.AudioState.STOPPED)
        this.audioManager_.start();
      this.speechRecognitionManager_.start();
    }
  };

  return {
    SpeechManager: SpeechManager
  };
});
